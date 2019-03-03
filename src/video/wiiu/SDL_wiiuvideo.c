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

#include "wiiu_shaders.h"

static int WIIU_VideoInit(_THIS);
static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void WIIU_VideoQuit(_THIS);
static void WIIU_PumpEvents(_THIS);

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

static int WIIU_VideoInit(_THIS)
{
	SDL_DisplayMode mode;

	WHBProcInit();
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
	WHBProcShutdown();
}

static int WIIU_CreateSDLWindow(_THIS, SDL_Window *window) {
	SDL_SetKeyboardFocus(window);
	return 0;
}

<<<<<<< HEAD
=======
static void render_scene(WIIU_WindowData *data) {
	WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	wiiuSetTextureShader();

	GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)u_viewSize);
	GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)u_texSize);
	GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_mod);
	GX2RSetAttributeBuffer(&position_buffer, 0, position_buffer.elemSize, 0);
	GX2RSetAttributeBuffer(&tex_coord_buffer, 1, tex_coord_buffer.elemSize, 0);

	GX2SetPixelTexture(&data->texture, wiiuTextureShader.pixelShader->samplerVars[0].location);
	GX2SetPixelSampler(&sampler, wiiuTextureShader.pixelShader->samplerVars[0].location);

	GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

static int WIIU_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
	WIIU_WindowData *data = (WIIU_WindowData *) SDL_GetWindowData(window, WIIU_WINDOW_DATA);
	Uint32 flags = SDL_GetWindowFlags(window);
	float a_position[8];
	float* buffer;
	int x, y, w, h;

	SDL_GetWindowPosition(window, &x, &y);
	SDL_GetWindowSize(window, &w, &h);
	a_position[0] = (float)x;		a_position[1] = (float)y;
	a_position[2] = (float)(x + w);	a_position[3] = (float)y;
	a_position[4] = (float)(x + w);	a_position[5] = (float)(y + h);
	a_position[6] = (float)x;		a_position[7] = (float)(y + h);

	buffer = GX2RLockBufferEx(&position_buffer, 0);
	if (buffer) {
		memcpy(buffer, a_position, position_buffer.elemSize * position_buffer.elemCount);
	}
	GX2RUnlockBufferEx(&position_buffer, 0);

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, data->texture.surface.image, data->texture.surface.imageSize);

	WHBGfxBeginRender();

	if (!(flags & SDL_WINDOW_WIIU_GAMEPAD_ONLY)) {
		WHBGfxBeginRenderTV();
		render_scene(data);
		WHBGfxFinishRenderTV();
	}

	if (!(flags & SDL_WINDOW_WIIU_TV_ONLY)) {
		WHBGfxBeginRenderDRC();
		render_scene(data);
		WHBGfxFinishRenderDRC();
	}

	WHBGfxFinishRender();

	return 0;
}

static void WIIU_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
	WIIU_WindowData *data = (WIIU_WindowData*) SDL_GetWindowData(window, WIIU_WINDOW_DATA);
	SDL_FreeSurface(data->surface);
	MEMFreeToDefaultHeap(data->texture.surface.image);
	SDL_free(data);
}

>>>>>>> video/wiiu: Add flags to only draw on TV or Gamepad (#1)
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
