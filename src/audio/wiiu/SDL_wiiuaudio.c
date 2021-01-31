/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2018 Ash Logan <ash@heyquark.com>

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

#if SDL_AUDIO_DRIVER_WIIU

#include <stdio.h>

#include "SDL_audio.h"
#include "SDL_error.h"
#include "SDL_timer.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_wiiuaudio.h"
#include "SDL_wiiuaudio_mix.h"

#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include <sndcore2/drcvs.h>
#include <coreinit/core.h>
#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#define WIIUAUDIO_DRIVER_NAME "wiiu"

#define AX_MAIN_AFFINITY OS_THREAD_ATTRIB_AFFINITY_CPU1

static void _WIIUAUDIO_framecallback();
static SDL_AudioDevice* cb_this;
#define cb_hidden cb_this->hidden

/*  Some helpers for AX-related math */
/*  Absolute address to an AXVoiceOffsets offset */
#define calc_ax_offset(offs, addr) (((void*)addr - offs.data) \
    / sizeof_sample(offs))

#define sizeof_sample(offs) (offs.dataType == AX_VOICE_FORMAT_LPCM8 ? 1 : 2)

/*  +1, but never goes above NUM_BUFFERS */
#define next_id(id) (id + 1) % NUM_BUFFERS

static int WIIUAUDIO_OpenDevice(_THIS, void* handle, const char* devname, int iscapture) {
    AXVoiceOffsets offs;
    AXVoiceVeData vol = {
        .volume = 0x8000,
    };
    uint32_t old_affinity;
    float srcratio;

    this->hidden = (struct SDL_PrivateAudioData*)SDL_malloc(sizeof(*this->hidden));
    if (this->hidden == NULL) return SDL_OutOfMemory();
    SDL_zerop(this->hidden);

/*  We *must not* change cores when setting stuff up */
    old_affinity = OSGetThreadAffinity(OSGetCurrentThread());
    OSSetThreadAffinity(OSGetCurrentThread(), AX_MAIN_AFFINITY);

/*  Take a quick aside to init the wiiu audio */
    if (!AXIsInit()) {
    /*  Init the AX audio engine */
        AXInitParams initparams = {
            .renderer = AX_INIT_RENDERER_48KHZ,
            .pipeline = AX_INIT_PIPELINE_SINGLE,
        };
        AXInitWithParams(&initparams);
    } else printf("DEBUG: AX already up?\n");

    if (this->spec.channels < 1) this->spec.channels = 1;
    if (this->spec.channels > WIIU_MAX_VALID_CHANNELS)
        this->spec.channels = WIIU_MAX_VALID_CHANNELS;

/*  Force wiiu-compatible audio formats.
    TODO verify - unsigned or signed? */
    switch (SDL_AUDIO_BITSIZE(this->spec.format)) {
        case 8:
        /*  TODO 8-bit audio sounds broken */
            /*this->spec.format = AUDIO_S8;
            break;*/
        case 16:
        default:
            this->spec.format = AUDIO_S16MSB;
            break;
    }

    //TODO maybe round this->spec.samples up even when >?
    //maybe even force at least 2* so we get more frame callbacks to think
    if (this->spec.samples < AXGetInputSamplesPerFrame()) {
        this->spec.samples = AXGetInputSamplesPerFrame();
    }

/*  We changed channels and samples, so recalculate the spec */
    SDL_CalculateAudioSpec(&this->spec);

/*  Allocate buffers for double-buffering and samples */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        this->hidden->mixbufs[i] = SDL_malloc(this->spec.size);
        if (this->hidden->mixbufs[i] == NULL) {
            AXQuit();
            printf("DEBUG: Couldn't allocate buffer");
            return SDL_SetError("Couldn't allocate buffer");
        }

        memset(this->hidden->mixbufs[i], 0, this->spec.size);
        DCStoreRange(this->hidden->mixbufs[i], this->spec.size);
    }

/*  Allocate a scratch buffer for deinterleaving operations */
    this->hidden->deintvbuf = SDL_malloc(this->spec.size);
    if (this->hidden->deintvbuf == NULL) {
        AXQuit();
        printf("DEBUG: Couldn't allocate deinterleave buffer");
        return SDL_SetError("Couldn't allocate deinterleave buffer");
    }


    for (int i = 0; i < this->spec.channels; i++) {
    /*  Get a voice, top priority */
        this->hidden->voice[i] = AXAcquireVoice(31, NULL, NULL);
        if (!this->hidden->voice[i]) {
            AXQuit();
            printf("DEBUG: couldn't get voice\n");
            return SDL_OutOfMemory();
        }

    /*  Start messing with it */
        AXVoiceBegin(this->hidden->voice[i]);
        AXSetVoiceType(this->hidden->voice[i], 0);

    /*  Set the voice's volume. */
        AXSetVoiceVe(this->hidden->voice[i], &vol);
        switch (this->spec.channels) {
            case 1: /* mono */ {
                AXSetVoiceDeviceMix(this->hidden->voice[i],
                    AX_DEVICE_TYPE_DRC, 0, mono_mix[i]);
                AXSetVoiceDeviceMix(this->hidden->voice[i],
                    AX_DEVICE_TYPE_TV, 0, mono_mix[i]);
            } break;
            case 2: /* stereo */ {
                AXSetVoiceDeviceMix(this->hidden->voice[i],
                    AX_DEVICE_TYPE_DRC, 0, stereo_mix[i]);
                AXSetVoiceDeviceMix(this->hidden->voice[i],
                    AX_DEVICE_TYPE_TV, 0, stereo_mix[i]);
            } break;
        }

    /*  Set the samplerate conversion ratio
        <source sample rate> / <target sample rate> */
        srcratio = (float)this->spec.freq / (float)AXGetInputSamplesPerSec();
        AXSetVoiceSrcRatio(this->hidden->voice[i], srcratio);
        AXSetVoiceSrcType(this->hidden->voice[i], AX_VOICE_SRC_TYPE_LINEAR);

    /*  Set up the offsets for the first mixbuf */
        switch (SDL_AUDIO_BITSIZE(this->spec.format)) {
            case 8:
                offs.dataType = AX_VOICE_FORMAT_LPCM8;
                offs.endOffset = this->spec.samples;
                break;
            case 16:
            default:
                offs.dataType = AX_VOICE_FORMAT_LPCM16;
                offs.endOffset = this->spec.samples;
                break;
        }
        offs.loopingEnabled = AX_VOICE_LOOP_ENABLED;
        offs.loopOffset = 0;
        offs.currentOffset = 0;

        if (offs.dataType == AX_VOICE_FORMAT_LPCM8) {
            offs.data = this->hidden->mixbufs[0]
                + this->spec.samples * i * sizeof(Uint8);
        } else if (offs.dataType == AX_VOICE_FORMAT_LPCM16) {
            offs.data = this->hidden->mixbufs[0]
                + this->spec.samples * i * sizeof(Uint16);
        }
        AXSetVoiceOffsets(this->hidden->voice[i], &offs);

    /*  Set the last good loopcount */
        this->hidden->last_loopcount = AXGetVoiceLoopCount(this->hidden->voice[i]);

    /*  Offsets are set for playing the first mixbuf, so we should render the second */
        this->hidden->playingid = 0;
        this->hidden->renderingid = 1;

    /*  Start playing. */
        AXSetVoiceState(this->hidden->voice[i], AX_VOICE_STATE_PLAYING);

    /*  Okay, we're good */
        AXVoiceEnd(this->hidden->voice[i]);
    }

    cb_this = this; //wish there was a better way
    AXRegisterAppFrameCallback(_WIIUAUDIO_framecallback);

/*  Put the thread affinity back to normal - we won't call any more AX funcs */
    OSSetThreadAffinity(OSGetCurrentThread(), old_affinity);
    return 0;
}

/*  Called every 3ms before a frame of audio is rendered. Keep it fast! */
static void _WIIUAUDIO_framecallback() {
    int playing_buffer = -1;
    AXVoiceOffsets offs[6];
    void* endaddr;

    for (int i = 0; i < cb_this->spec.channels; i++) {
        AXGetVoiceOffsets(cb_hidden->voice[i], &offs[i]);
    }

/*  Figure out which buffer is being played by the hardware */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        void* buf = cb_hidden->mixbufs[i];
        uint32_t startOffset = calc_ax_offset(offs[0], buf);
        uint32_t endOffset = startOffset + cb_this->spec.samples;

    /*  NOTE endOffset definitely needs to be <= (AX plays the sample at
        endOffset), dunno about startOffset */
        if (offs[0].currentOffset >= startOffset &&
            offs[0].currentOffset <= endOffset) {
            playing_buffer = i;
            break;
        }
    }

    if (playing_buffer < 0 || playing_buffer >= NUM_BUFFERS) {
    /*  UM */
    /*  Uncomment for craploads of debug info */
        /*printf("bad buffer %d\n" "|> %08X, %08X-%08X\n" \
            "0: xxxxxxxx, %08X-%08X (%08X@%08X)\n" \
            "1: xxxxxxxx, %08X-%08X (%08X@%08X)\n", \
            playing_buffer, offs.currentOffset, offs.loopOffset, offs.endOffset,
            calc_ax_offset(offs, (void*)cb_hidden->mixbufs[0]),
                calc_ax_offset(offs, (void*)cb_hidden->mixbufs[0] + cb_this->spec.size),
                cb_this->spec.size, (void*)cb_hidden->mixbufs[0],
            calc_ax_offset(offs, (void*)cb_hidden->mixbufs[1]),
                calc_ax_offset(offs, (void*)cb_hidden->mixbufs[1] + cb_this->spec.size),
                cb_this->spec.size, (void*)cb_hidden->mixbufs[1]);*/
        printf("DEBUG: Playing an invalid buffer? This is not a good sign.\n");
        playing_buffer = 0;
    }

/*  Make sure playingid is in sync with the hardware */
    cb_hidden->playingid = playing_buffer;

/*  Make sure the end offset is correct for the playing buffer */
    for (int i = 0; i < cb_this->spec.channels; i++) {
    /*  Calculate end address, aka start of the next (i+1) channel's buffer */
        endaddr = cb_hidden->mixbufs[cb_hidden->playingid] +
            (cb_this->spec.samples * sizeof_sample(offs[i]) * (i + 1));

    /*  Trial end error to try and limit popping */
        endaddr -= 2;

        AXSetVoiceEndOffset(
            cb_hidden->voice[i],
            calc_ax_offset(offs[i], endaddr)
        );

    /*  The next buffer is good to go, set the loop offset */
        if (cb_hidden->renderingid != next_id(cb_hidden->playingid)) {
        /*  Calculate start address for this channel's buffer */
            void* loopaddr = cb_hidden->mixbufs[next_id(cb_hidden->playingid)] +
                (cb_this->spec.samples * sizeof_sample(offs[i]) * i);

            AXSetVoiceLoopOffset(cb_hidden->voice[i], calc_ax_offset(offs[i], loopaddr));
    /*  Otherwise, make sure the loop offset is correct for the playing buffer */
        } else {
            void* loopaddr = cb_hidden->mixbufs[cb_hidden->playingid] +
                (cb_this->spec.samples * sizeof_sample(offs[i]) * i);

            AXSetVoiceLoopOffset(cb_hidden->voice[i], calc_ax_offset(offs[i], loopaddr));
        }
    }
}

static void WIIUAUDIO_PlayDevice(_THIS) {
/*  Deinterleave stereo audio */
    switch (SDL_AUDIO_BITSIZE(this->spec.format)) {
        case 8: {
            Uint8* samples = (Uint8*)this->hidden->mixbufs[this->hidden->renderingid];
            Uint8* deintv = (Uint8*)this->hidden->deintvbuf;

            /* Store the samples in a separate deinterleaved buffer */
            for (int ch = 0; ch < this->spec.channels; ch++) {
                for (int i = 0; i < this->spec.samples; i++) {
                    deintv[this->spec.samples * ch + i] = samples[i * this->spec.channels + ch];
                }
            }
        } break;
        case 16: {
            Uint16* samples = (Uint16*)this->hidden->mixbufs[this->hidden->renderingid];
            Uint16* deintv = (Uint16*)this->hidden->deintvbuf;

            /* Store the samples in a separate deinterleaved buffer */
            for (int ch = 0; ch < this->spec.channels; ch++) {
                for (int i = 0; i < this->spec.samples; i++) {
                    deintv[this->spec.samples * ch + i] = samples[i * this->spec.channels + ch];
                }
            }
        } break;
        default: {} break;
    }

/*  Copy the deinterleaved buffer to the mixing buffer */
    memcpy(
        this->hidden->mixbufs[this->hidden->renderingid],
        this->hidden->deintvbuf,
        this->spec.size
    );
/*  Comment this out for broken-record mode ;3 */
    DCStoreRange(this->hidden->mixbufs[this->hidden->renderingid], this->spec.size);
/*  Signal we're no longer rendering this buffer, AX callback will notice later */
    this->hidden->renderingid = next_id(this->hidden->renderingid);
}

static void WIIUAUDIO_WaitDevice(_THIS) {
/*  TODO use real thread sync stuff */
    while (SDL_AtomicGet(&this->enabled) && this->hidden->renderingid == this->hidden->playingid) {
        OSSleepTicks(OSMillisecondsToTicks(3));
    }
}

static Uint8* WIIUAUDIO_GetDeviceBuf(_THIS) {
/*  SDL will write audio samples into this buffer */
    return this->hidden->mixbufs[this->hidden->renderingid];
}

static void WIIUAUDIO_CloseDevice(_THIS) {
    if (AXIsInit()) {
        AXDeregisterAppFrameCallback(_WIIUAUDIO_framecallback);
        for (int i = 0; i < SIZEOF_ARR(this->hidden->voice); i++) {
            if (this->hidden->voice[i]) {
                AXFreeVoice(this->hidden->voice[i]);
                this->hidden->voice[i] = NULL;
            }
        }
        AXQuit();
    }
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (this->hidden->mixbufs[i]) SDL_free(this->hidden->mixbufs[i]);
    }
    if (this->hidden->deintvbuf) SDL_free(this->hidden->deintvbuf);
    SDL_free(this->hidden);
}

static void WIIUAUDIO_ThreadInit(_THIS) {
/*  Bump our thread's priority a bit */
    OSThread* currentThread = OSGetCurrentThread();
    int32_t priority = OSGetThreadPriority(currentThread);
    priority -= 1;
    OSSetThreadPriority(currentThread, priority);
}

static int WIIUAUDIO_Init(SDL_AudioDriverImpl* impl) {
    impl->OpenDevice = WIIUAUDIO_OpenDevice;
    impl->PlayDevice = WIIUAUDIO_PlayDevice;
    impl->WaitDevice = WIIUAUDIO_WaitDevice;
    impl->GetDeviceBuf = WIIUAUDIO_GetDeviceBuf;
    impl->CloseDevice = WIIUAUDIO_CloseDevice;
    impl->ThreadInit = WIIUAUDIO_ThreadInit;

    impl->OnlyHasDefaultOutputDevice = 1;

    return 1;
}

AudioBootStrap WIIUAUDIO_bootstrap = {
    WIIUAUDIO_DRIVER_NAME, "Wii U AX Audio Driver", WIIUAUDIO_Init, 0,
};

#endif //SDL_AUDIO_DRIVER_WIIU
