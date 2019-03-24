/*
  Simple DirectMedia Layer
  Copyright (C) 2018 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>
  Copyright (C) 2019 Ash Logan <ash@heyquark.com>

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

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../../events/SDL_touch_c.h"

#include "SDL_log.h"
#include "SDL_assert.h"
#include "SDL_events.h"

#include "SDL_wiiujoystick.h"

#define WIIU_DEVICE_INVALID (-1)
#define WIIU_DEVICE_GAMEPAD (0)
#define WIIU_DEVICE_WPAD(x) (x + 1)

#define MAX_CONTROLLERS WIIU_DEVICE_WPAD(4)
//index with device_index, get WIIU_DEVICE*
static int deviceMap[MAX_CONTROLLERS];
//index with device_index, get SDL_JoystickID
static SDL_JoystickID instanceMap[MAX_CONTROLLERS];

static int WIIU_GetDeviceForIndex(int device_index) {
	return deviceMap[device_index];
}
static int WIIU_GetIndexForDevice(int wiiu_device) {
	for (int i = 0; i < MAX_CONTROLLERS; i++) {
		if (deviceMap[i] == wiiu_device) return i;
	}
	return -1;
}

static int WIIU_GetNextDeviceIndex() {
	return WIIU_GetIndexForDevice(WIIU_DEVICE_INVALID);
}

static int WIIU_GetInstForIndex(int device_index) {
	if (device_index == -1) return -1;
	return instanceMap[device_index];
}
static int WIIU_GetInstForDevice(int wiiu_device) {
	int device_index = WIIU_GetIndexForDevice(wiiu_device);
	return WIIU_GetInstForIndex(device_index);
}

static void WIIU_RemoveDevice(int wiiu_device) {
	int device_index = WIIU_GetIndexForDevice(wiiu_device);
	if (device_index == -1) return;
	/* Move all the other controllers back, so all device_indexes are valid */
	for (int i = device_index; i < MAX_CONTROLLERS; i++) {
		if (i + 1 < MAX_CONTROLLERS) {
			deviceMap[i] = deviceMap[i + 1];
			instanceMap[i] = instanceMap[i + 1];
		} else {
			deviceMap[i] = -1;
			instanceMap[i] = -1;
		}
	}
}

/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * This function should return 0, or -1 on an unrecoverable error.
 */
static int WIIU_JoystickInit(void)
{
	for (int i = 0; i < MAX_CONTROLLERS; i++) {
		deviceMap[i] = WIIU_DEVICE_INVALID;
		instanceMap[i] = -1;
	}
	WIIU_JoystickDetect();
	return 0;
}

/* Function to return the number of joystick devices plugged in right now */
static int WIIU_JoystickGetCount(void)
{
	return WIIU_GetNextDeviceIndex();
}

/* Function to cause any queued joystick insertions to be processed */
static void WIIU_JoystickDetect(void)
{
/*	Make sure there are no dangling instances or device indexes
 	These checks *should* be unneccesary, remove once battle-tested */
	for (int i = 0; i < MAX_CONTROLLERS; i++) {
		if (deviceMap[i] == WIIU_DEVICE_INVALID && instanceMap[i] != -1) {

			SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
				"WiiU device_index %d dangling instance %d!\n",
				i, instanceMap[i]
			);
			/* Make sure that joystick actually got removed */
			SDL_PrivateJoystickRemoved(instanceMap[i]);
			instanceMap[i] = -1;
		}
		if (deviceMap[i] != WIIU_DEVICE_INVALID && instanceMap[i] == -1) {
			SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
				"WiiU device_index %d assigned to %d, but has no instance!\n",
				i, deviceMap[i]
			);
			instanceMap[i] = -1;
		}
	}
/*	Check if we are missing the WiiU Gamepad and try to connect it
	if the gamepad is disconnected that's handled in SDL_UpdateJoystick */
	if (WIIU_GetIndexForDevice(WIIU_DEVICE_GAMEPAD) == -1) {
	/*	Try and detect a gamepad */
		VPADStatus status;
		VPADReadError err;
		VPADRead(VPAD_CHAN_0, &status, 1, &err);
		if (err == VPAD_READ_SUCCESS || err == VPAD_READ_NO_SAMPLES) {
		/*	We have a gamepad! Assign a device index and instance ID. */
			int device_index = WIIU_GetNextDeviceIndex();
			if (device_index != -1) {
			/*	Save its device index */
				deviceMap[device_index] = WIIU_DEVICE_GAMEPAD;
				instanceMap[device_index] = SDL_GetNextJoystickInstanceID();
				SDL_PrivateJoystickAdded(instanceMap[device_index]);
				SDL_LogInfo(SDL_LOG_CATEGORY_INPUT,
					"WiiU: Detected Gamepad, assigned device %d/instance %d\n",
					device_index, instanceMap[device_index]);
			}
		}
	}
}

/* Function to get the device-dependent name of a joystick */
static const char *WIIU_JoystickGetDeviceName(int device_index)
{
	/* Gamepad */
	if (WIIU_GetDeviceForIndex(device_index) == WIIU_DEVICE_GAMEPAD)
		return "WiiU Gamepad";
	return "Unknown";
}

/* Function to get the player index of a joystick */
static int WIIU_JoystickGetDevicePlayerIndex(int device_index)
{
	return -1;
}

/* Function to return the stable GUID for a plugged in device */
static SDL_JoystickGUID WIIU_JoystickGetDeviceGUID(int device_index)
{
	SDL_JoystickGUID guid;
	/* the GUID is just the first 16 chars of the name for now */
	const char *name = WIIU_JoystickGetDeviceName(device_index);
	SDL_zero(guid);
	SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
	return guid;
}

/* Function to get the current instance id of the joystick located at device_index */
static SDL_JoystickID WIIU_JoystickGetDeviceInstanceID(int device_index)
{
	return WIIU_GetInstForIndex(device_index);
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
static int WIIU_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
	/* Gamepad */
	if (WIIU_GetDeviceForIndex(device_index) == WIIU_DEVICE_GAMEPAD) {
		SDL_AddTouch(0, "WiiU Gamepad Touchscreen");
		joystick->nbuttons = sizeof(vpad_button_map) / sizeof(VPADButtons);
		joystick->naxes = 4;
		joystick->nhats = 0;
	}

	joystick->instance_id = WIIU_GetInstForIndex(device_index);

	return 0;
}

/* Rumble functionality */
static int WIIU_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
	/* TODO */
	return SDL_Unsupported();
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
static void WIIU_JoystickUpdate(SDL_Joystick *joystick)
{
	/* Gamepad */
	if (joystick->instance_id == WIIU_GetInstForDevice(WIIU_DEVICE_GAMEPAD)) {
		int16_t x1, y1, x2, y2;

		static uint16_t last_touch_x = 0;
		static uint16_t last_touch_y = 0;
		static uint16_t last_touched = 0;

		static int16_t x1_old = 0;
		static int16_t y1_old = 0;
		static int16_t x2_old = 0;
		static int16_t y2_old = 0;

		VPADStatus vpad;
		VPADReadError error;
		VPADTouchData tpdata;
		VPADRead(VPAD_CHAN_0, &vpad, 1, &error);
		if (error == VPAD_READ_INVALID_CONTROLLER) {
			/* Gamepad disconnected! */
			SDL_PrivateJoystickRemoved(joystick->instance_id);
			/* Unlink Gamepad, device_index, instance_id */
			WIIU_RemoveDevice(WIIU_DEVICE_GAMEPAD);
		} else if (error != VPAD_READ_SUCCESS) {
			return;
		}

		/* touchscreen */
		VPADGetTPCalibratedPoint(VPAD_CHAN_0, &tpdata, &vpad.tpNormal);
		if (tpdata.touched) {
			/* Send an initial touch */
			SDL_SendTouch(0, 0, SDL_TRUE,
					(float) tpdata.x / 1280.0f,
					(float) tpdata.y / 720.0f, 1);

			/* Always send the motion */
			SDL_SendTouchMotion(0, 0,
					(float) tpdata.x / 1280.0f,
					(float) tpdata.y / 720.0f, 1);

			/* Update old values */
			last_touch_x = tpdata.x;
			last_touch_y = tpdata.y;
			last_touched = 1;
		} else if (last_touched) {
			/* Finger released from screen */
			SDL_SendTouch(0, 0, SDL_FALSE,
					(float) last_touch_x / 1280.0f,
					(float) last_touch_y / 720.0f, 1);
			last_touched = 0;
		}

		/* axys */
		x1 = (int16_t) ((vpad.leftStick.x) * 0x7ff0);
		y1 = (int16_t) -((vpad.leftStick.y) * 0x7ff0);
		x2 = (int16_t) ((vpad.rightStick.x) * 0x7ff0);
		y2 = (int16_t) -((vpad.rightStick.y) * 0x7ff0);

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

		/* buttons */
		for(int i = 0; i < joystick->nbuttons; i++)
			if (vpad.trigger & vpad_button_map[i])
				SDL_PrivateJoystickButton(joystick, (Uint8)i, SDL_PRESSED);
		for(int i = 0; i < joystick->nbuttons; i++)
			if (vpad.release & vpad_button_map[i])
				SDL_PrivateJoystickButton(joystick, (Uint8)i, SDL_RELEASED);
	}
}

/* Function to close a joystick after use */
static void WIIU_JoystickClose(SDL_Joystick *joystick)
{
}

/* Function to perform any system-specific joystick related cleanup */
static void WIIU_JoystickQuit(void)
{
}

SDL_JoystickDriver SDL_WIIU_JoystickDriver =
{
	WIIU_JoystickInit,
	WIIU_JoystickGetCount,
	WIIU_JoystickDetect,
	WIIU_JoystickGetDeviceName,
	WIIU_JoystickGetDevicePlayerIndex,
	WIIU_JoystickGetDeviceGUID,
	WIIU_JoystickGetDeviceInstanceID,
	WIIU_JoystickOpen,
	WIIU_JoystickRumble,
	WIIU_JoystickUpdate,
	WIIU_JoystickClose,
	WIIU_JoystickQuit,
};

#endif
