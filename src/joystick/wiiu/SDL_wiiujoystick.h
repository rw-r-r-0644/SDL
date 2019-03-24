/*
  Simple DirectMedia Layer
  Copyright (C) 2018 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>

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

#include <padscore/wpad.h>

#define WIIU_DEVICE_INVALID (-1)
#define WIIU_DEVICE_GAMEPAD (0)
#define WIIU_DEVICE_WPAD(x) (x + 1)
#define WIIU_WPAD_CHAN(device) (device - 1)
#define WIIU_NUM_WPADS (4)

#define MAX_CONTROLLERS WIIU_DEVICE_WPAD(WIIU_NUM_WPADS)

#define SIZEOF_ARR(arr) (sizeof(arr) / sizeof(arr[0]))

static VPADButtons vpad_button_map[] =
{
    VPAD_BUTTON_A, VPAD_BUTTON_B, VPAD_BUTTON_X, VPAD_BUTTON_Y,
    VPAD_BUTTON_STICK_L, VPAD_BUTTON_STICK_R,
    VPAD_BUTTON_L, VPAD_BUTTON_R,
    VPAD_BUTTON_ZL, VPAD_BUTTON_ZR,
    VPAD_BUTTON_PLUS, VPAD_BUTTON_MINUS,
    VPAD_BUTTON_LEFT, VPAD_BUTTON_UP, VPAD_BUTTON_RIGHT, VPAD_BUTTON_DOWN,
    VPAD_STICK_L_EMULATION_LEFT, VPAD_STICK_L_EMULATION_UP, VPAD_STICK_L_EMULATION_RIGHT, VPAD_STICK_L_EMULATION_DOWN,
    VPAD_STICK_R_EMULATION_LEFT, VPAD_STICK_R_EMULATION_UP, VPAD_STICK_R_EMULATION_RIGHT, VPAD_STICK_R_EMULATION_DOWN
};

static WPADButton wiimote_button_map[] = {
    WPAD_BUTTON_A, WPAD_BUTTON_B, WPAD_BUTTON_Z, WPAD_BUTTON_C,
    0, 0,
    WPAD_BUTTON_1, WPAD_BUTTON_2,
    0, 0,
    WPAD_BUTTON_PLUS, WPAD_BUTTON_MINUS,
    WPAD_BUTTON_LEFT, WPAD_BUTTON_UP, WPAD_BUTTON_RIGHT, WPAD_BUTTON_DOWN,
};

static WPADClassicButton classic_button_map[] = {
    WPAD_CLASSIC_BUTTON_A, WPAD_CLASSIC_BUTTON_B, WPAD_CLASSIC_BUTTON_X,
    WPAD_CLASSIC_BUTTON_Y,
    0, 0,
    WPAD_CLASSIC_BUTTON_L, WPAD_CLASSIC_BUTTON_R,
    WPAD_CLASSIC_BUTTON_ZL, WPAD_CLASSIC_BUTTON_ZR,
    WPAD_CLASSIC_BUTTON_PLUS, WPAD_CLASSIC_BUTTON_MINUS,
    WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_RIGHT,
    WPAD_CLASSIC_BUTTON_DOWN,
};

static WPADProButton pro_button_map[] = {
    WPAD_PRO_BUTTON_A, WPAD_PRO_BUTTON_B, WPAD_PRO_BUTTON_X, WPAD_PRO_BUTTON_Y,
    WPAD_PRO_BUTTON_STICK_L, WPAD_PRO_BUTTON_STICK_R,
    WPAD_PRO_TRIGGER_L, WPAD_PRO_TRIGGER_R,
    WPAD_PRO_TRIGGER_ZL, WPAD_PRO_TRIGGER_ZR,
    WPAD_PRO_BUTTON_PLUS, WPAD_PRO_BUTTON_MINUS,
    WPAD_PRO_BUTTON_LEFT, WPAD_PRO_BUTTON_UP,
    WPAD_PRO_BUTTON_RIGHT, WPAD_PRO_BUTTON_DOWN,
    WPAD_PRO_STICK_L_EMULATION_LEFT, WPAD_PRO_STICK_L_EMULATION_UP,
    WPAD_PRO_STICK_L_EMULATION_RIGHT, WPAD_PRO_STICK_L_EMULATION_DOWN,
    WPAD_PRO_STICK_R_EMULATION_LEFT, WPAD_PRO_STICK_R_EMULATION_UP,
    WPAD_PRO_STICK_R_EMULATION_RIGHT, WPAD_PRO_STICK_R_EMULATION_DOWN
};

static int WIIU_JoystickInit(void);
static int WIIU_JoystickGetCount(void);
static void WIIU_JoystickDetect(void);
static const char *WIIU_JoystickGetDeviceName(int device_index);
static int WIIU_JoystickGetDevicePlayerIndex(int device_index);
static SDL_JoystickGUID WIIU_JoystickGetDeviceGUID(int device_index);
static SDL_JoystickID WIIU_JoystickGetDeviceInstanceID(int device_index);
static int WIIU_JoystickOpen(SDL_Joystick *joystick, int device_index);
static int WIIU_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms);
static void WIIU_JoystickUpdate(SDL_Joystick *joystick);
static void WIIU_JoystickClose(SDL_Joystick *joystick);
static void WIIU_JoystickQuit(void);

#define RETURN_WPAD_NAME(chan, ext) { \
    switch (ext) { \
    case WPAD_EXT_CORE: \
    case WPAD_EXT_MPLUS: \
    default: { \
        return "Wii Remote #" #chan; \
    } \
    case WPAD_EXT_NUNCHUK: \
    case WPAD_EXT_MPLUS_NUNCHUK: { \
        return "Wii Remote #" #chan " and Nunchuk"; \
    } \
    case WPAD_EXT_CLASSIC: \
    case WPAD_EXT_MPLUS_CLASSIC: { \
        return "Wii Classic Controller #" #chan; \
    } \
    case WPAD_EXT_PRO_CONTROLLER: { \
        return "WiiU Pro Controller #" #chan; \
    } \
    } \
}
