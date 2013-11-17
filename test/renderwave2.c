/*
 * renderwave2.c - Audiality 2 render-to-wave via a2_RenderWave()
 *
 *	This does essentially the same thing as renderwave.c, except using the
 *	higher level conveniency API call a2_RenderWave().
 *
 * Copyright 2013 David Olofson <david@olofson.net>
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "audiality2.h"
#include "waves.h"


/* Configuration */
const char *audiodriver = "default";
int samplerate = 44100;
int channels = 2;
int audiobuf = 4096;
int waverate = 0;

static int do_exit = 0;


static void usage(const char *exename)
{
	fprintf(stderr,	"\n\nUsage: %s [switches] <file>\n\n", exename);
	fprintf(stderr, "Switches:  -d<name>[,opt[,opt[,...]]]\n"
			"                       Audio driver + options\n"
			"           -b<n>       Audio buffer size (frames)\n"
			"           -r<n>       Audio sample rate (Hz)\n"
			"           -c<n>       Number of audio channels\n\n"
			"           -wr<n>      Wave sample rate (Hz)\n"
			"           -h          Help\n\n");
}


/* Parse driver selection and configuration switches */
static void parse_args(int argc, const char *argv[])
{
	int i;
	for(i = 1; i < argc; ++i)
	{
		if(argv[i][0] != '-')
			continue;
		if(strncmp(argv[i], "-d", 2) == 0)
		{
			audiodriver = &argv[i][2];
			printf("[Driver: %s]\n", audiodriver);
		}
		else if(strncmp(argv[i], "-a", 2) == 0)
		{
			audiobuf = atoi(&argv[i][2]);
			printf("[Buffer: %d]\n", audiobuf);
		}
		else if(strncmp(argv[i], "-r", 2) == 0)
		{
			samplerate = atoi(&argv[i][2]);
			printf("[Sample rate: %d]\n", samplerate);
		}
		else if(strncmp(argv[i], "-c", 2) == 0)
		{
			channels = atoi(&argv[i][2]);
			printf("[Channels %d]\n", channels);
		}
		else if(strncmp(argv[i], "-wr", 3) == 0)
		{
			waverate = atoi(&argv[i][3]);
			printf("[Wave sample rate: %d]\n", waverate);
		}
		else if(strncmp(argv[i], "-h", 2) == 0)
		{
			usage(argv[0]);
			exit(0);
		}
		else
		{
			fprintf(stderr, "Unknown switch '%s'!\n", argv[i]);
			exit(1);
		}
	}
}


static void breakhandler(int a)
{
	fprintf(stderr, "Stopping...\n");
	do_exit = 1;
}


static void fail(unsigned where, A2_errors err)
{
	fprintf(stderr, "ERROR at %d: %s\n", where, a2_ErrorString(err));
	exit(100);
}


int main(int argc, const char *argv[])
{
	A2_handle h, songh, ph, vh;
	A2_driver *drv;
	A2_config *cfg;
	A2_state *state;
	signal(SIGTERM, breakhandler);
	signal(SIGINT, breakhandler);

	/* Command line switches */
	parse_args(argc, argv);

	/* Configure and open master state */
	if(!(drv = a2_NewDriver(A2_AUDIODRIVER, audiodriver)))
		fail(1, a2_LastError());
	if(!(cfg = a2_OpenConfig(samplerate, audiobuf, channels, A2_RTERRORS |
			A2_TIMESTAMP | A2_REALTIME | A2_STATECLOSE)))
		fail(2, a2_LastError());
	if(drv && a2_AddDriver(cfg, drv))
		fail(3, a2_LastError());
	if(!(state = a2_Open(cfg)))
		fail(4, a2_LastError());
	if(samplerate != cfg->samplerate)
		printf("Actual master state sample rate: %d (requested %d)\n",
				cfg->samplerate, samplerate);

	fprintf(stderr, "Loading...\n");
	
	/* Load jingle */
	if((h = a2_Load(state, "data/a2jingle.a2s")) < 0)
		fail(5, -h);
	if((songh = a2_Get(state, h, "Song")) < 0)
		fail(6, -songh);

	/* Load wave player program */
	if((h = a2_Load(state, "data/playtestwave.a2s")) < 0)
		fail(7, -h);
	if((ph = a2_Get(state, h, "PlayTestWave")) < 0)
		fail(8, -ph);

	/* Render! */
	fprintf(stderr, "Rendering...\n");
	if(!waverate)
		waverate = samplerate;
	if((h = a2_RenderWave(state,
			A2_WWAVE, 0, 0,	/* no MIP, auto period, no flags */
			waverate, 0,	/* sample rate, stop when silent */
			songh, 0, NULL)) < 0)	/* program, no args */
		fail(9, -h);

	/* Start playing! */
	fprintf(stderr, "Playing...\n");
	a2_Now(state);
	vh = a2_Start(state, a2_RootVoice(state), ph, 0.0f, 1.0f, h);
	if(vh < 0)
		fail(10, -vh);

	/* Wait for completion or abort */
	while(!do_exit)
	{
		a2_Now(state);
		sleep(1);
	}

	a2_Now(state);
	a2_Send(state, vh, 1);
	sleep(1);

	a2_Close(state);
	return 0;
}
