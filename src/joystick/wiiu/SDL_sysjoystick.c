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

#if SDL_JOYSTICK_WIIU

#include <vpad/input.h>
#include <coreinit/debug.h>

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../../events/SDL_touch_c.h"

#include "SDL_assert.h"
#include "SDL_events.h"

#define JOYSTICK_COUNT 1

static VPADButtons vpad_button_map[] = {
	VPAD_BUTTON_A, VPAD_BUTTON_B, VPAD_BUTTON_X, VPAD_BUTTON_Y,
	VPAD_BUTTON_STICK_L, VPAD_BUTTON_STICK_R,
	VPAD_BUTTON_L, VPAD_BUTTON_R,
	VPAD_BUTTON_ZL, VPAD_BUTTON_ZR,
	VPAD_BUTTON_PLUS, VPAD_BUTTON_MINUS,
	VPAD_BUTTON_LEFT, VPAD_BUTTON_UP, VPAD_BUTTON_RIGHT, VPAD_BUTTON_DOWN,
	VPAD_STICK_L_EMULATION_LEFT, VPAD_STICK_L_EMULATION_UP, VPAD_STICK_L_EMULATION_RIGHT, VPAD_STICK_L_EMULATION_DOWN,
	VPAD_STICK_R_EMULATION_LEFT, VPAD_STICK_R_EMULATION_UP, VPAD_STICK_R_EMULATION_RIGHT, VPAD_STICK_R_EMULATION_DOWN
};

/* Function to scan the system for joysticks.
 * It should return the number of available joysticks,
 * or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
	return JOYSTICK_COUNT;
}

int
SDL_SYS_NumJoysticks(void)
{
	return JOYSTICK_COUNT;
}

void
SDL_SYS_JoystickDetect(void)
{
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
	// Gamepad
	if (device_index == 0) {
		return "WiiU Gamepad";
	}

	return "Unknown";
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
	return device_index;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
	// Gamepad
	if (device_index == 0) {
		SDL_AddTouch(0, "WiiU Gamepad Touchscreen");
		joystick->nbuttons = sizeof(vpad_button_map) / sizeof(VPADButtons);
		joystick->naxes = 4;
		joystick->nhats = 0;
	}

	joystick->instance_id = device_index;

	return 0;
}

/* Function to determine if this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
	// Gamepad
	if (joystick->instance_id == 0) {
		return SDL_TRUE;
	}
	return SDL_FALSE;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick *joystick)
{
	// Gamepad
	if (joystick->instance_id == 0)
	{
		static uint16_t last_touch_x = 0;
		static uint16_t last_touch_y = 0;
		static uint16_t last_touched = 0;

		static int16_t x1_old = 0;
		static int16_t y1_old = 0;
		static int16_t x2_old = 0;
		static int16_t y2_old = 0;

		VPADStatus vpad;
		VPADReadError error;
		VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
		if (error != VPAD_READ_SUCCESS)
			return;

		// touchscreen
		if (vpad.tpFiltered1.touched) {
			// Send an initial touch
			SDL_SendTouch(0, 0, SDL_TRUE,
					(float) vpad.tpFiltered1.x / 4096.0f,
					(float) (4096 - vpad.tpFiltered1.y) / 4096.0f, 1);

			// Always send the motion
			SDL_SendTouchMotion(0, 0,
					(float) vpad.tpFiltered1.x / 4096.0f,
					(float) (4096 - vpad.tpFiltered1.y) / 4096.0f, 1);

			// Update old values
			last_touch_x = vpad.tpFiltered1.x;
			last_touch_y = vpad.tpFiltered1.y;
			last_touched = 1;
		} else if (last_touched) {
			// Finger released from screen
			SDL_SendTouch(0, 0, SDL_FALSE,
					(float) last_touch_x / 4096.0f,
					(float) (4096 - last_touch_y) / 4096.0f, 1);
		}

		// axys
		int16_t x1 = (int16_t) ((vpad.leftStick.x) * 0x7ff0);
		int16_t y1 = (int16_t) -((vpad.leftStick.y) * 0x7ff0);
		int16_t x2 = (int16_t) ((vpad.rightStick.x) * 0x7ff0);
		int16_t y2 = (int16_t) -((vpad.rightStick.y) * 0x7ff0);

		if(x1 != x1_old) {
			SDL_PrivateJoystickAxis(joystick, 0, x1);
			x1_old = x1;
		}
		if(y1 != y1_old) {
			SDL_PrivateJoystickAxis(joystick, 1, y1);
			y1_old = y1;
		}
		if(x2 != x2_old) {
			SDL_PrivateJoystickAxis(joystick, 2, x2);
			x2_old = x2;
		}
		if(y2 != y2_old) {
			SDL_PrivateJoystickAxis(joystick, 3, y2);
			y2_old = y2;
		}

		// buttons
		for(int i = 0; i < joystick->nbuttons; i++)
			if (vpad.trigger & vpad_button_map[i])
				SDL_PrivateJoystickButton(joystick, (Uint8)i, SDL_PRESSED);
		for(int i = 0; i < joystick->nbuttons; i++)
			if (vpad.release & vpad_button_map[i])
				SDL_PrivateJoystickButton(joystick, (Uint8)i, SDL_RELEASED);
	}
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick *joystick)
{
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID(int device_index)
{
	SDL_JoystickGUID guid;
	/* the GUID is just the first 16 chars of the name for now */
	const char *name = SDL_SYS_JoystickNameForDeviceIndex(device_index);
	SDL_zero(guid);
	SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
	return guid;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick *joystick)
{
	SDL_JoystickGUID guid;
	/* the GUID is just the first 16 chars of the name for now */
	const char *name = joystick->name;
	SDL_zero(guid);
	SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
	return guid;
}

#endif
