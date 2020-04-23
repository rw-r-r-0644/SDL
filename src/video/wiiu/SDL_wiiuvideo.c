/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2018 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2018 rw-r-r-0644 <r.r.qwertyuiop.r.r@gmail.com>

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

/* This is basically just a stub at this point - all the magic happens in
 * SDL_Render, and the textureframebuffer stuff in SDL_video.c.
 * Potentially more could/should be done here, video modes and things.
 * Some design work will need to go into the responsibilities of render
 * vs video.
 */

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_WIIU

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_wiiuvideo.h"

#include <whb/proc.h>
#include <whb/gfx.h>
#include <string.h>
#include <stdint.h>

#include <proc_ui/procui.h>

#include "wiiu_shaders.h"

static int WIIU_VideoInit(_THIS);
static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void WIIU_VideoQuit(_THIS);
static void WIIU_PumpEvents(_THIS);

static int using_whb_proc = 0;

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

static int WIIU_VideoInit(_THIS)
{
	SDL_DisplayMode mode;

	if (!ProcUIIsRunning()) {
		WHBProcInit();
		using_whb_proc = 1;
	}
	WHBGfxInit();

	// setup shader
	wiiuInitTextureShader();

	// add default mode (1280x720)
	mode.format = SDL_PIXELFORMAT_RGBA8888;
	mode.w = SCREEN_WIDTH;
	mode.h = SCREEN_HEIGHT;
	mode.refresh_rate = 60;
	mode.driverdata = NULL;
	if (SDL_AddBasicVideoDisplay(&mode) < 0) {
		return -1;
	}
	SDL_AddDisplayMode(&_this->displays[0], &mode);

	return 0;
}

static void WIIU_VideoQuit(_THIS)
{
	wiiuFreeTextureShader();
	WHBGfxShutdown();
	if (using_whb_proc) WHBProcShutdown();
}

static int WIIU_CreateSDLWindow(_THIS, SDL_Window *window) {
	SDL_SetKeyboardFocus(window);
	return 0;
}

static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
	return 0;
}

static void WIIU_PumpEvents(_THIS)
{
}

static int WIIU_Available(void)
{
	return 1;
}

static void WIIU_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device);
}

static SDL_VideoDevice *WIIU_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	device = (SDL_VideoDevice*) SDL_calloc(1, sizeof(SDL_VideoDevice));
	if(!device) {
		SDL_OutOfMemory();
		return NULL;
	}

	device->VideoInit = WIIU_VideoInit;
	device->VideoQuit = WIIU_VideoQuit;
	device->SetDisplayMode = WIIU_SetDisplayMode;
	device->PumpEvents = WIIU_PumpEvents;
	device->CreateSDLWindow = WIIU_CreateSDLWindow;

	device->free = WIIU_DeleteDevice;

	return device;
}

VideoBootStrap WIIU_bootstrap = {
	"WiiU", "Video driver for Nintendo WiiU",
	WIIU_Available, WIIU_CreateDevice
};

#endif /* SDL_VIDEO_DRIVER_WIIU */
