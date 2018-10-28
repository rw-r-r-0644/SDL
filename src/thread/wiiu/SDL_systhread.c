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

/* WiiU thread management routines for SDL */

#include <stdio.h>
#include <stdlib.h>

#include "SDL_error.h"
#include "SDL_thread.h"
#include "../SDL_systhread.h"
#include "../SDL_thread_c.h"
#include <malloc.h>
#include <coreinit/thread.h>

static void
thread_deallocator(OSThread *thread, void *stack)
{
   free(thread);
   free(stack);
}

static void
thread_cleanup(OSThread *thread, void *stack)
{
}

int SDL_SYS_CreateThread(SDL_Thread *thread, void *args)
{
    unsigned int stackSize = thread->stacksize ? thread->stacksize : 0x8000;
    int priority = OSGetThreadPriority(OSGetCurrentThread());
    OSThread *handle = (OSThread *)memalign(16, sizeof(OSThread));
    char *stack = (char *)memalign(16, stackSize);
    memset(handle, 0, sizeof(OSThread));

    if (!OSCreateThread(handle,
                        (OSThreadEntryPointFn)SDL_RunThread,
                        (int32_t)args,
                        NULL,
                        stack,
                        stackSize,
                        priority,
                        OS_THREAD_ATTRIB_AFFINITY_ANY))
    {
        return SDL_SetError("OSCreateThread() failed");
    }

    OSSetThreadDeallocator(handle, &thread_deallocator);
    OSSetThreadCleanupCallback(handle, &thread_cleanup);
    OSResumeThread(handle);
    thread->handle = handle;
    return 0;
}

void SDL_SYS_SetupThread(const char *name)
{
    /* Do nothing. */
}

SDL_threadID SDL_ThreadID(void)
{
    return (SDL_threadID) OSGetCurrentThread();
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    OSJoinThread(thread->handle, NULL);
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    OSDetachThread(thread->handle);
}

void SDL_SYS_KillThread(SDL_Thread *thread)
{
    OSCancelThread(thread->handle);
}

int SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    int value;

    if (priority == SDL_THREAD_PRIORITY_LOW) {
        value = 17;
    } else if (priority == SDL_THREAD_PRIORITY_HIGH) {
        value = 15;
    } else if (priority == SDL_THREAD_PRIORITY_TIME_CRITICAL) {
        value = 14;
    } else {
        value = 16;
    }

    return OSSetThreadPriority(OSGetCurrentThread(), value);

}

#endif /* SDL_THREAD_WIIU */

/* vim: ts=4 sw=4
 */