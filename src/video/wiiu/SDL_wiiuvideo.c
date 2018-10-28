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

#include <coreinit/memheap.h>
#include <coreinit/cache.h>
#include <coreinit/memfrmheap.h>
#include <coreinit/screen.h>
#include <string.h>

#define FRAME_HEAP_TAG (0x000DECAF)

static int WIIU_VideoInit(_THIS);
static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void WIIU_VideoQuit(_THIS);
static void WIIU_PumpEvents(_THIS);
static int WIIU_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch);
static int WIIU_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects);
static void WIIU_DestroyWindowFramebuffer(_THIS, SDL_Window *window);

static int WIIU_Available(void) {
	return 1;
}

static void WIIU_DeleteDevice(SDL_VideoDevice *device) {
	SDL_free(device);
}

static SDL_VideoDevice *WIIU_CreateDevice(int devindex) {
	SDL_VideoDevice *device;

	device = (SDL_VideoDevice*) SDL_calloc(1, sizeof(SDL_VideoDevice));
	if(!device) {
		SDL_OutOfMemory();
		if(device) {
			SDL_free(device);
		}
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

static int WIIU_VideoInit(_THIS) {
	SDL_DisplayMode mode;

	OSScreenInit();

	mode.format = SDL_PIXELFORMAT_RGBA8888;
	mode.w = 1280;
	mode.h = 720;
	mode.refresh_rate = 60;
	mode.driverdata = NULL;
	if(SDL_AddBasicVideoDisplay(&mode) < 0) {
		return -1;
	}

	SDL_AddDisplayMode(&_this->displays[0], &mode);
	return 0;
}

static void WIIU_VideoQuit(_THIS) {
	OSScreenShutdown();
}

static int WIIU_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode) {
	return 0;
}

static void WIIU_PumpEvents(_THIS) {
	// TODO
}

#define WIIU_DATA "_SDL_WiiUData"

typedef struct {
	SDL_Surface *sdl_surface;
	void *wiiu_surface;
	int wiiu_work_fb;
} WIIU_WindowData;

static int WIIU_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch) {
	SDL_Surface *surface;
	const Uint32 surface_format = SDL_PIXELFORMAT_RGBA8888;
	int w, h;
	int bpp;
	Uint32 Rmask, Gmask, Bmask, Amask;

	SDL_PixelFormatEnumToMasks(surface_format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
	SDL_GetWindowSize(window, &w, &h);
	surface = SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);
	if(!surface) {
		return -1;
	}

	WIIU_WindowData *data = SDL_calloc(1, sizeof(WIIU_WindowData));
	data->sdl_surface = surface;

	// Allocate the framebuffers
	MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
	MEMRecordStateForFrmHeap(heap, FRAME_HEAP_TAG);

	// Write "Look at the TV Screen" on the gamepad
	void *sBufferDRC = MEMAllocFromFrmHeapEx(heap, OSScreenGetBufferSizeEx(SCREEN_DRC), 4);
	OSScreenSetBufferEx(SCREEN_DRC, sBufferDRC);
	OSScreenClearBufferEx(SCREEN_DRC, 0);
	OSScreenPutFontEx(SCREEN_DRC, 0, 0, "Look at the TV Screen.");
	DCFlushRange(sBufferDRC, OSScreenGetBufferSizeEx(SCREEN_DRC));
	OSScreenFlipBuffersEx(SCREEN_DRC);
	OSScreenEnableEx(SCREEN_DRC, 1);

	// Allocate the framebuffer (TV will be used)
	data->wiiu_work_fb = 1;
	data->wiiu_surface = MEMAllocFromFrmHeapEx(heap, OSScreenGetBufferSizeEx(SCREEN_TV), 4);
	OSScreenSetBufferEx(SCREEN_TV, data->wiiu_surface);

	// Prepare the framebuffer
	OSScreenClearBufferEx(SCREEN_TV, 0);

	// Flush framebuffer
	DCFlushRange(data->wiiu_surface, OSScreenGetBufferSizeEx(SCREEN_TV));
	OSScreenFlipBuffersEx(SCREEN_TV);
	data->wiiu_work_fb = !data->wiiu_work_fb;

	// Enable the framebuffer
	OSScreenEnableEx(SCREEN_TV, 1);

	// Set surface data
	SDL_SetWindowData(window, WIIU_DATA, data);
	*format = surface_format;
	*pixels = surface->pixels;
	*pitch = surface->pitch;

	// *hack*: if the window is not in focus joystick events will be
	// ignored, so set the focus for the keyboard (actually the wiiu
	// just doesn't have windows, so the focus concept doesn't really
	// apply)
	SDL_SetKeyboardFocus(window);

	return 0;
}

static void WIIU_FBCopy(uint32_t *_dst, uint32_t *_src, uint32_t _w, uint32_t _h) {
	register uint32_t *src = _src, *dst = _dst;
	register uint32_t w = _w, h = _h;
	register uint32_t a, b, c, d;
	register uint32_t y, *xf;

	for(y = 0; y < h; y++) {
		xf = dst + w;
		do {
			a = src[0];
			b = src[1];
			c = src[2];
			d = src[3];
			dst[0] = a;
			dst[1] = b;
			dst[2] = c;
			dst[3] = d;
			src += 4;
			dst += 4;
		} while(dst != xf);
		dst += 1280 - w;
	}
}

static int WIIU_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects) {
	WIIU_WindowData *data;

	data = (WIIU_WindowData*) SDL_GetWindowData(window, WIIU_DATA);
	if(!data) {
		return SDL_SetError("Couldn't find wiiu data for window");
	}

	SDL_Surface *sdl_surface = data->sdl_surface;

	// Get buffers
	uint32_t *src_buffer = (uint32_t *)sdl_surface->pixels;
	uint32_t *dst_buffer = (uint32_t *)(data->wiiu_surface + (data->wiiu_work_fb * 1280 * 720 * 4));
	WIIU_FBCopy(dst_buffer, src_buffer, sdl_surface->w, sdl_surface->h);

	// Flush framebuffer
	DCFlushRange(data->wiiu_surface, OSScreenGetBufferSizeEx(SCREEN_TV));
	OSScreenFlipBuffersEx(SCREEN_TV);
	data->wiiu_work_fb = !data->wiiu_work_fb;

	return 0;
}

static void WIIU_DestroyWindowFramebuffer(_THIS, SDL_Window *window) {
	WIIU_WindowData *data;

	data = (WIIU_WindowData*) SDL_GetWindowData(window, WIIU_DATA);
	SDL_FreeSurface(data->sdl_surface);
	SDL_free(data);

	MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
	OSScreenEnableEx(SCREEN_DRC, 0);
	OSScreenEnableEx(SCREEN_TV, 0);
	MEMFreeByStateToFrmHeap(heap, FRAME_HEAP_TAG);
}

#endif /* SDL_VIDEO_DRIVER_WIIU */
