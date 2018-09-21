/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_THREAD_WIIU

#include "SDL_thread.h"

#include <stdbool.h>
#include <coreinit/alarm.h>
#include <coreinit/mutex.h>
#include <coreinit/condition.h>

typedef struct
{
   OSCondition *cond;
   bool timed_out;
} WIIU_CondWaitTimeoutData;

/* Create a condition variable */
SDL_cond *
SDL_CreateCond(void)
{
    OSCondition *cond;

    cond = (OSCondition *) SDL_malloc(sizeof(OSCondition));
    if (cond) {
        OSInitCond(cond);
    } else {
        SDL_OutOfMemory();
    }
    return (SDL_cond *)cond;
}

/* Destroy a condition variable */
void
SDL_DestroyCond(SDL_cond * cond)
{
    if (cond) {
        SDL_free(cond);
    }
}

/* Restart one of the threads that are waiting on the condition variable */
int
SDL_CondSignal(SDL_cond * cond)
{
    return SDL_CondBroadcast(cond);
}

/* Restart all threads that are waiting on the condition variable */
int
SDL_CondBroadcast(SDL_cond * cond)
{
    if (!cond) {
        return SDL_SetError("Passed a NULL condition variable");
    }

    OSSignalCond((OSCondition *)cond);
    return 0;
}

/* Wait on the condition variable for at most 'ms' milliseconds.
   The mutex must be locked before entering this function!
   The mutex is unlocked during the wait, and locked again after the wait.

Typical use:

Thread A:
    SDL_LockMutex(lock);
    while ( ! condition ) {
        SDL_CondWait(cond, lock);
    }
    SDL_UnlockMutex(lock);

Thread B:
    SDL_LockMutex(lock);
    ...
    condition = true;
    ...
    SDL_CondSignal(cond);
    SDL_UnlockMutex(lock);
 */

static void
SDL_CondWaitTimeoutCallback(OSAlarm *alarm, OSContext *context)
{
   WIIU_CondWaitTimeoutData *data = (WIIU_CondWaitTimeoutData *)OSGetAlarmUserData(alarm);
   data->timed_out = true;
   OSSignalCond(data->cond);
}

int
SDL_CondWaitTimeout(SDL_cond * cond, SDL_mutex * mutex, Uint32 ms)
{
	WIIU_CondWaitTimeoutData data;
	data.timed_out = false;
	data.cond = (OSCondition *)cond;

	// Timeout is zero
	if (!ms)
		return SDL_MUTEX_TIMEDOUT;

	// Set an alarm
	OSAlarm alarm;
	OSCreateAlarm(&alarm);
	OSSetAlarmUserData(&alarm, &data);
	OSSetAlarm(&alarm, OSMillisecondsToTicks(ms), &SDL_CondWaitTimeoutCallback);

	// Wait on the condition
	OSWaitCond((OSCondition *)cond, (OSMutex *)mutex);

	OSCancelAlarm(&alarm);
	return data.timed_out ? SDL_MUTEX_TIMEDOUT : 0;
}

/* Wait on the condition variable forever */
int
SDL_CondWait(SDL_cond * cond, SDL_mutex * mutex)
{
    OSWaitCond((OSCondition *)cond, (OSMutex *)mutex);
    return 0;
}

#endif /* SDL_THREAD_WIIU */

/* vi: set ts=4 sw=4 expandtab: */