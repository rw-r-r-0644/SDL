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

#ifndef SDL_wiiuaudio_mix_h_
#define SDL_wiiuaudio_mix_h_

/* Some defines to help make the code below more readable */
#define AX_VOICE(x) x
#define AX_CHANNEL_LEFT 0
#define AX_CHANNEL_RIGHT 1
#define AX_BUS_MASTER 0

/* TODO: AXGetDeviceChannelCount. For now we'll use Decaf's values.
 * According to Decaf, the TV has 6 channels and the gamepad has 4. We set up
 * both arrays with 6, the Gamepad callbacks just won't use the whole buffer. */
#define AX_NUM_CHANNELS 6

static AXVoiceDeviceMixData stereo_mix[2 /* voices */][AX_NUM_CHANNELS] = {
    [AX_VOICE(0)] = {
        [AX_CHANNEL_LEFT] = {
            .bus = {
                [AX_BUS_MASTER] = { .volume = 0x8000 },
            }
        },
    },
    [AX_VOICE(1)] = {
        [AX_CHANNEL_RIGHT] = {
            .bus = {
                [AX_BUS_MASTER] = { .volume = 0x8000 },
            }
        },
    },
};
static AXVoiceDeviceMixData mono_mix[1 /* voice */][AX_NUM_CHANNELS] = {
    [AX_VOICE(0)] = {
        [AX_CHANNEL_LEFT] = { .bus = {
            [AX_BUS_MASTER] = { .volume = 0x8000 },
        }},
        [AX_CHANNEL_RIGHT] = { .bus = {
            [AX_BUS_MASTER] = { .volume = 0x8000 },
        }},
    },
};

#define WIIU_MAX_VALID_CHANNELS 2

#endif //SDL_wiiuaudio_mix_h_
