/*
 * audiality2.h - Audiality 2 Realtime Scriptable Audio Engine
 *
 * Copyright (C) 2010-2012 David Olofson <david@olofson.net>
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

#ifndef AUDIALITY2_H
#define AUDIALITY2_H

#include "drivers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Versioning */
#define	A2_MAKE_VERSION(major, minor, micro, build)	\
		(((major) << 24) | ((minor) << 16) | ((micro) << 8) | (build))
#define	A2_MAJOR(ver)	(((ver) >> 24) & 0xff)
#define	A2_MINOR(ver)	(((ver) >> 16) & 0xff)
#define	A2_MICRO(ver)	(((ver) >> 8) & 0xff)
#define	A2_BUILD(ver)	((ver) & 0xff)

/* Current version */
#define	A2_VERSION	A2_MAKE_VERSION(1,9,0,0)

/* Default reference frequence for linear pitch 0.0; "middle C" */
#define	A2_MIDDLEC	261.626f

/* Number of VM registers */
#define	A2_REGISTERS	32

/* Maximum number of arguments to a VM program or function */
#define	A2_MAXARGS	8

/* Maximum number of entry points a VM program can have. (EP 0 is "main()".) */
#define	A2_MAXEPS	8

/* Number of waveform mipmap levels */
#define	A2_MIPLEVELS	10

/* Maximum number of sample frames to process at a time */
#define	A2_MAXFRAG	64

/* Maximum number of audio channels supported */
#define	A2_MAXCHANNELS	8


/*---------------------------------------------------------
	Error handling
---------------------------------------------------------*/

/* Return last error code set */
A2_errors a2_LastError(void);

/* Return textual explanation of a Audiality 2 error code */
const char *a2_ErrorString(unsigned errorcode);


/*---------------------------------------------------------
	Versioning
---------------------------------------------------------*/

/* Return version of the linked Audiality 2 library. */
static inline unsigned a2_HeaderVersion(void)
{
	return A2_VERSION;
}

unsigned a2_LinkedVersion(void);


/*---------------------------------------------------------
	Open/close
---------------------------------------------------------*/

/*
 * Create an Audiality state using the provided configuration. If NULL is
 * specified, a default configuration is created.
 * 
 * If a driver in a provided configuration is already open, the 'samplerate',
 * 'buffer', 'channels' and 'flags' arguments are ignored, and the corresponding values are instead
 * retrieved from the driver. In this case, the driver will NOT be closed with
 * the state, unless the application sets the A2_STATECLOSE flag in the driver's
 * 'flag' field.
 *
 * A driver provided by the application will NOT be destroyed by Audiality as
 * the state is closed, unless the application sets the A2_STATEDESTROY flag in
 * the driver's 'flag' field.
 *
 * NOTE:
 *	The 'flags' argument is only passed on to the driver 'flags' field when
 *	a driver is opened by the state! That is, flags are not passed on to a
 *	driver that is already open when a2_Open() is called.
 */
A2_state *a2_Open(A2_config *config);

/*
 * Create a substate to state 'master'.
 *
 * The substate shares waves, programs and other objects with the master state.
 * Making API calls that create or manipulate such on a substate is equivalent
 * to operating directly on the substate's master state.
 *
 * The substate has its own engine context, with its own set of groups and
 * voices, independent from and asynchrounous to those of the substate's master
 * state. Realtime control API calls on a substate operate on this local engine
 * context, allowing the substate to perform realtime or offline processing
 * independent of the master state.
 *
TODO: What about a2_Now()? Should we add an offline version that just sets the
TODO: current time in terms of audio samples at the state's sample rate?
 *
 * NOTE:
 *	Substates are NOT reentrant/thread safe in relation to each other, or
 *	their master states! To safely perform background rendering in another
 *	thread or similar, a separate master state must be used.
 */
A2_state *a2_SubState(A2_state *master, A2_config *config);

/*
 * Close an Audiality state or substate.
 *
 * NOTE:
 *	Substates CAN be closed manually, but if they aren't, they are closed
 *	automatically as their master state is closed.
 */
void a2_Close(A2_state *st);


/*---------------------------------------------------------
	Handle management
---------------------------------------------------------*/

/*
 * Hardcoded handles
 */
#define	A2_ROOTBANK	0


/*
 * Returns the handle of the root voice of the specified (sub)state.
 *
 * NOTE:
 *	While substates share banks, waves, programs etc with their parent
 *	states, all in the same handle space, they have their own voices - and
 *	these must not be mixed up! Bad Things(TM) will happen if you talk to
 *	a state about voices that belong to another state...
 */
A2_handle a2_RootVoice(A2_state *st);


/*
 * General handle operations
 */

/* Return type of object with 'handle', or -1 if 'handle' is invalid. */
A2_otypes a2_TypeOf(A2_state *st, A2_handle handle);

/* Return name string of 'type'. */
const char *a2_TypeName(A2_state *st, A2_otypes typecode);

/* Return a string representation of the object assigned to 'handle' */
const char *a2_String(A2_state *st, A2_handle handle);

/* Return the name of the object assigned to 'handle', if any is defined */
const char *a2_Name(A2_state *st, A2_handle handle);

/*
 * Attempt to increase the reference count of 'handle' by one.
 */
A2_errors a2_Retain(A2_state *st, A2_handle handle);

/*
 * Decrease the reference count of 'handle' by one. If the reference count
 * reaches zero, the handle will be released, and (typically) the associated
 * object is destroyed.
 *
 * Returns 0 (A2_OK) if the object actually is released. Otherwise, an error
 * code is returned, most commonly A2_REFUSE, as a result of the object
 * intentionally refusing to destruct.
 * 
 * NOTE:
 *	Voices will return A2_REFUSE here, as they need a roundtrip to the
 *	engine context before the handle can safely be returned to the pool!
 */
A2_errors a2_Release(A2_state *st, A2_handle handle);

/*
 * Have 'owner' claim ownership of 'handle'.
 *
 * NOTE:
 *	Only certain object types can claim ownership of other objects!
 *
 * NOTE:
 *	This does NOT increase the reference count of 'handle'! The logic is
 *	that the caller owns the object, and hands it over to 'owner'.
 */
A2_errors a2_Assign(A2_state *st, A2_handle owner, A2_handle handle);

/*
 * Have 'owner' claim ownership of 'handle' and add it to 'owner's exports as
 * 'name'. If 'name' is NULL, an attempt is made at getting a name from
 * a2_Name().
 */
A2_errors a2_Export(A2_state *st, A2_handle owner, A2_handle handle,
		const char *name);


/*---------------------------------------------------------
	Object loading/creation
---------------------------------------------------------*/

/*
 * Create a new, empty bank. 'name' is the import name for scripts to use;
 * NULL results in a unique name being generated automatically.
 */
A2_handle a2_NewBank(A2_state *st, const char *name, int flags);

/*
 * Load .a2s file 'fn' or null terminated string 'code' as a bank.
 *
 * Returns the handle of the resulting bank, or if the operation fails, a
 * negative error code. (Use (-result) to get the A2_errors code.)
 */
A2_handle a2_LoadString(A2_state *st, const char *code, const char *name);
A2_handle a2_Load(A2_state *st, const char *fn);

/*
 * Create a string object from the null terminated 'string'. Returns the handle
 * of the string object, or a negative error code.
 */
A2_handle a2_NewString(A2_state *st, const char *string);

/*
 * Decreases the reference count of all objects that have been created as
 * direct results of API calls.
 *
 * Returns the number of objects released, not including recursive side effects.
 *
 * NOTE:
 *	This call should generally NOT be used by applications that manage
 *	objects explicitly! It still affects objects after a2_IncRef() has been
 *	used on them.
 */
int a2_UnloadAll(A2_state *st);


/*---------------------------------------------------------
	Offline rendering
---------------------------------------------------------*/

/*
 * Run a state (or substate), converting the output and writing it to memory.
 *
 * NOTE:
 *	The context needs to be using an audio driver that implements the Run()
 *	method. Typically the "dummy" driver is used for this, and this is the
 *	default driver for states created with a2_SubState().
 *
 * Returns the number of sample frames (not bytes!) actually read, or a negated
 * A2_errors error code.
 */
/*TODO:*/
int a2_Read(A2_state *st, A2_handle handle,
		A2_sampleformats fmt, void *buffer, unsigned size);

/*TODO: Render CSL program into wave. */
A2_handle a2_RenderWave(A2_state *st, A2_handle bank, const char *name,
		unsigned period, int flags, unsigned samplerate, unsigned length,
		A2_handle program, unsigned argc, int *argv);

/*TODO: Render CSL program, provided as source code string, into wave. */
A2_handle a2_RenderString(A2_state *st, A2_handle bank, const char *name,
		unsigned period, int flags, unsigned samplerate, unsigned length,
		const char *source);


/*---------------------------------------------------------
	Objects and exports
---------------------------------------------------------*/

/*
 * Return handle of object specified by 'path' relative to object 'node'.
 * Object names are separated with '/' characters.
 *
 * Returns a negative A2_errors error code if no object was found.
 */
A2_handle a2_Get(A2_state *st, A2_handle node, const char *path);

/*
 * Get handle of export 'i' of object 'node'.
 *
 * Returns -A2_WRONGTYPE if 'node' cannot have exports, or -A2_INDEXRANGE if
 * 'i' is out of range.
 */
A2_handle a2_GetExport(A2_state *st, A2_handle node, unsigned i);

/*
 * Get name of export 'i' of object 'node'.
 *
 * Returns NULL if 'node' cannot have exports, or if 'i' is out of range.
 */
const char *a2_GetExportName(A2_state *st, A2_handle node, unsigned i);


/*---------------------------------------------------------
	Playing and controlling
---------------------------------------------------------*/

/*
 * Schedule subsequent commands to be executed as soon as possible with constant
 * latency.
 */
void a2_Now(A2_state *st);

/*
 * Bump the timestamp for subsequent commands. (milliseconds)
 */
void a2_Wait(A2_state *st, float dt);

/*
 * Create a new voice under voice 'parent', running the default group program,
 * "a2_groupdriver", featuring volume and pan controls, and support for
 * a2_Tap() and a2_Insert().
 */
A2_handle a2_NewGroup(A2_state *st, A2_handle parent);

/*
 * Start 'program' on a new subvoice of 'parent'.
 *
 * The a2_Starta() versions expect arrays of 16:16 fixed point values.
 *
 * The a2_Start() macro converts and passes any number of floating point
 * arguments.
 *
 * Returns a handle, or a negative error code.
 */
A2_handle a2_Starta(A2_state *st, A2_handle parent, A2_handle program,
		unsigned argc, int *argv);
#define a2_Start(st, p, prg, args...)					\
	({								\
		float fa[] = { args };					\
		int i, ia[sizeof(fa) / sizeof(float)];			\
		for(i = 0; i < sizeof(ia) / sizeof(int); ++i)		\
			ia[i] = fa[i] * 65536.0f;			\
		a2_Starta(st, p, prg, sizeof(ia) / sizeof(int), ia);	\
	})

/*
 * Start 'program' on a new subvoice of 'parent', without attaching the new
 * voice to a handle.
 *
 * NOTE:
 *	Although detached voices cannot be addressed directly, they still
 *	recieve messages sent to all voices in the group!
 */
A2_errors a2_Playa(A2_state *st, A2_handle parent, A2_handle program,
		unsigned argc, int *argv);
#define a2_Play(st, p, prg, args...)					\
	({								\
		float fa[] = { args };					\
		int i, ia[sizeof(fa) / sizeof(float)];			\
		for(i = 0; i < sizeof(ia) / sizeof(int); ++i)		\
			ia[i] = fa[i] * 65536.0f;			\
		a2_Playa(st, p, prg, sizeof(ia) / sizeof(int), ia);	\
	})

/* Send a message to entry point 'ep' of the program running on 'voice'. */
A2_errors a2_Senda(A2_state *st, A2_handle voice, unsigned ep,
		unsigned argc, int *argv);
#define a2_Send(st, v, ep, args...)					\
	({								\
		float fa[] = { args };					\
		int i, ia[sizeof(fa) / sizeof(float)];			\
		for(i = 0; i < sizeof(ia) / sizeof(int); ++i)		\
			ia[i] = fa[i] * 65536.0f;			\
		a2_Senda(st, v, ep, sizeof(ia) / sizeof(int), ia);	\
	})

/* Send a message to entry point 'ep' of all subvoices of 'voice'. */
A2_errors a2_SendSuba(A2_state *st, A2_handle voice, unsigned ep,
		unsigned argc, int *argv);
#define a2_SendSub(st, v, ep, args...)					\
	({								\
		float fa[] = { args };					\
		int i, ia[sizeof(fa) / sizeof(float)];			\
		for(i = 0; i < sizeof(ia) / sizeof(int); ++i)		\
			ia[i] = fa[i] * 65536.0f;			\
		a2_SendSuba(st, v, ep, sizeof(ia) / sizeof(int), ia);	\
	})

/*
 * Instantly stop 'voice' and any subvoices running under it. The handles of
 * the voice and any subvoices running under it will be released.
 *
 * NOTE:
 *	Care should be taken to not use the 'voice' handle, or any subvoice
 *	handles after this call, as they're all released! (Using invalid
 *	handles is technically safe, but may have "interesting" effects...)
 *
 * WARNING:
 *	There is no fadeout or anything, so this will most definitely result in
 *	a nasty click or pop if done to voices that are still audible!
 */
A2_errors a2_Kill(A2_state *st, A2_handle voice);

/* Kill all subvoices of 'voice', but not 'voice' itself */
A2_errors a2_KillSub(A2_state *st, A2_handle voice);


/*---------------------------------------------------------
	Simplified "plugin" interface
---------------------------------------------------------*/

/*
 * Set up 'callback' to receive audio from 'group'. The callback will run as
 * the final stage of 'group', allowing it to capture the post pan, post volume
 * controls audio that is sent to the master group. 'userdata' will be passed
 * as is to 'callback'.
 */
A2_errors a2_Tap(A2_state *st, A2_handle voice, A2_xinsert_cb callback,
		void *userdata);

/*
 * Set up 'callback' to run as the final stage of 'group', allowing it to
 * process the post pan, post volume controls audio before it is sent to the
 * master group. 'userdata' will be passed as is to 'callback'.
 */
A2_errors a2_Insert(A2_state *st, A2_handle voice, A2_xinsert_cb callback,
		void *userdata);


/*---------------------------------------------------------
	Utilities
---------------------------------------------------------*/

/* Convert Hz to linear pitch */
float a2_F2P(float f);

/* Return pseudo-random number in the range [0, max[ */
float a2_Rand(A2_state *st, float max);


/*---------------------------------------------------------
	Object property interface
---------------------------------------------------------*/

typedef enum A2_properties
{
	/* Global settings and profiling information */
	A2_PSAMPLERATE=	0x00010000,	/* Audio I/O sample rate */
	A2_PBUFFER,			/* Audio I/O buffer size */
	A2_PACTIVEVOICES,		/* Number of active voices */
	A2_PFREEVOICES,			/* Number of voices in pool */
	A2_PTOTALVOICES,		/* Number of voices in total */
	A2_PCPULOADAVG,			/* Average DSP CPU load (%) */
	A2_PCPULOADMAX,			/* Peak DSP CPU load (%) */
	A2_PCPUTIMEAVG,			/* Average buffer processing time (ms) */
	A2_PCPUTIMEMAX,			/* Peak buffer processing time (ms) */
	A2_PINSTRUCTIONS,		/* VM instructions executed */
	A2_PEXPORTALL,			/* Export all programs! (Debug) */

	/* General properties */
	A2_PCHANNELS =	0x00020000,	/* Number of channels */
	A2_PFLAGS,			/* Flags */
	A2_PREFCOUNT,			/* Reference count of the handle */

	/* Wave properties */
	A2_PLOOPED =	0x00030000	/* Waveform is looped */
} A2_properties;

int a2_GetProperty(A2_state *st, A2_handle h, A2_properties p);
A2_errors a2_SetProperty(A2_state *st, A2_handle h, A2_properties p, int v);

/*TODO:
	* Remove A2_properties!

int a2_PropertyCount(A2_state *st, A2_handle h);
const char *a2_PropertyName(A2_state *st, A2_handle h, unsigned p);

double a2_GetPropertyd(A2_state *st, A2_handle h, unsigned p);
A2_errors a2_SetPropertyd(A2_state *st, A2_handle h, unsigned p, double v);

const char *a2_GetPropertys(A2_state *st, A2_handle h, unsigned p);
A2_errors a2_SetPropertys(A2_state *st, A2_handle h, unsigned p, const char *s);
*/

#ifdef __cplusplus
};
#endif

#endif /* AUDIALITY2_H */
