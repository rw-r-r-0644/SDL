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

#include <gfd.h>
#include <gx2/draw.h>
#include <gx2/shaders.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2r/draw.h>
#include <gx2r/buffer.h>
#include <whb/proc.h>
#include <whb/gfx.h>
#include <coreinit/memdefaultheap.h>
#include <string.h>

#include "texture_shader.h"

static int WIIU_VideoInit(_THIS);
static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void WIIU_VideoQuit(_THIS);
static void WIIU_PumpEvents(_THIS);
static int WIIU_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch);
static int WIIU_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects);
static void WIIU_DestroyWindowFramebuffer(_THIS, SDL_Window *window);

#define WIIU_DATA "_SDL_WiiUData"
#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

typedef struct
{
	SDL_Surface *surface;
	GX2Texture texture;
} WIIU_WindowData;

static const float position_vb[] =
{
   -1.0f, -1.0f,
    1.0f, -1.0f,
    1.0f,  1.0f,
   -1.0f,  1.0f,
};
static GX2RBuffer position_buffer = {
	GX2R_RESOURCE_BIND_VERTEX_BUFFER |
	GX2R_RESOURCE_USAGE_CPU_READ |
	GX2R_RESOURCE_USAGE_CPU_WRITE |
	GX2R_RESOURCE_USAGE_GPU_READ,
	2 * 4, 4, NULL
};

static const float tex_coord_vb[] =
{
   0.0f, 1.0f,
   1.0f, 1.0f,
   1.0f, 0.0f,
   0.0f, 0.0f,
};
static GX2RBuffer tex_coord_buffer = {
	GX2R_RESOURCE_BIND_VERTEX_BUFFER |
	GX2R_RESOURCE_USAGE_CPU_READ |
	GX2R_RESOURCE_USAGE_CPU_WRITE |
	GX2R_RESOURCE_USAGE_GPU_READ,
	2 * 4, 4, NULL
};

static WHBGfxShaderGroup group = { 0 };
static GX2Sampler sampler = {0};

static int WIIU_VideoInit(_THIS)
{
	SDL_DisplayMode mode;
	void *buffer = NULL;

	WHBProcInit();
	WHBGfxInit();

	// setup shader
	WHBGfxLoadGFDShaderGroup(&group, 0, texture_shader_gsh);
	WHBGfxInitShaderAttribute(&group, "position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
	WHBGfxInitShaderAttribute(&group, "tex_coord_in", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
	WHBGfxInitFetchShader(&group);

	// setup vertex position attribute
	GX2RCreateBuffer(&position_buffer);
	buffer = GX2RLockBufferEx(&position_buffer, 0);
	memcpy(buffer, position_vb, position_buffer.elemSize * position_buffer.elemCount);
	GX2RUnlockBufferEx(&position_buffer, 0);

	// setup vertex texture coordinates attribute
	GX2RCreateBuffer(&tex_coord_buffer);
	buffer = GX2RLockBufferEx(&tex_coord_buffer, 0);
	memcpy(buffer, tex_coord_vb, tex_coord_buffer.elemSize * tex_coord_buffer.elemCount);
	GX2RUnlockBufferEx(&tex_coord_buffer, 0);

	// initialize a sampler
	GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

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
	GX2RDestroyBufferEx(&position_buffer, 0);
	GX2RDestroyBufferEx(&tex_coord_buffer, 0);
	WHBGfxShutdown();
	WHBProcShutdown();
}

static int WIIU_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
	int bpp;
	Uint32 r, g, b, a;
	WIIU_WindowData *data;

	// hold a pointer to our stuff
	data = SDL_calloc(1, sizeof(WIIU_WindowData));

	// create a gx2 texture
	data->texture.surface.width    = window->w;
	data->texture.surface.height   = window->h;
	data->texture.surface.depth    = 1;
	data->texture.surface.dim      = GX2_SURFACE_DIM_TEXTURE_2D;
	data->texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	data->texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	data->texture.viewNumSlices    = 1;
	data->texture.compMap          = 0x00010203;
	GX2CalcSurfaceSizeAndAlignment(&data->texture.surface);
	GX2InitTextureRegs(&data->texture);

	data->texture.surface.image = MEMAllocFromDefaultHeapEx(data->texture.surface.imageSize, data->texture.surface.alignment);

	// create sdl surface framebuffer
	data->surface = SDL_CreateRGBSurfaceWithFormatFrom(
						data->texture.surface.image, // pixels
						window->w, window->h, // width, height
						bpp, window->w * 4, // depth, pitch
						SDL_PIXELFORMAT_RGBA8888 // format
					);

	*format = SDL_PIXELFORMAT_RGBA8888;
	*pixels = data->surface->pixels;
	*pitch = data->surface->pitch;

	SDL_SetWindowData(window, WIIU_DATA, data);

	// inform SDL we're ready to accept inputs
	SDL_SetKeyboardFocus(window);

	return 0;
}

static void render_scene(WIIU_WindowData *data) {
	WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	GX2SetFetchShader(&group.fetchShader);
	GX2SetVertexShader(group.vertexShader);
	GX2SetPixelShader(group.pixelShader);
	GX2RSetAttributeBuffer(&position_buffer, 0, position_buffer.elemSize, 0);
	GX2RSetAttributeBuffer(&tex_coord_buffer, 1, tex_coord_buffer.elemSize, 0);

	GX2SetPixelTexture(&data->texture, group.pixelShader->samplerVars[0].location);
	GX2SetPixelSampler(&sampler, group.pixelShader->samplerVars[0].location);

	GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

static int WIIU_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
	WIIU_WindowData *data = (WIIU_WindowData *) SDL_GetWindowData(window, WIIU_DATA);

	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, data->texture.surface.image, data->texture.surface.imageSize);

	WHBGfxBeginRender();

	WHBGfxBeginRenderTV();
	render_scene(data);
	WHBGfxFinishRenderTV();

	WHBGfxBeginRenderDRC();
	render_scene(data);
	WHBGfxFinishRenderDRC();

	WHBGfxFinishRender();

	return 0;
}

static void WIIU_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
	WIIU_WindowData *data = (WIIU_WindowData*) SDL_GetWindowData(window, WIIU_DATA);
	SDL_FreeSurface(data->surface);
	MEMFreeToDefaultHeap(data->texture.surface.image);
	SDL_free(data);
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
	device->CreateWindowFramebuffer = WIIU_CreateWindowFramebuffer;
	device->UpdateWindowFramebuffer = WIIU_UpdateWindowFramebuffer;
	device->DestroyWindowFramebuffer = WIIU_DestroyWindowFramebuffer;

	device->free = WIIU_DeleteDevice;

	return device;
}

VideoBootStrap WIIU_bootstrap = {
	"WiiU", "Video driver for Nintendo WiiU",
	WIIU_Available, WIIU_CreateDevice
};

#endif /* SDL_VIDEO_DRIVER_WIIU */
