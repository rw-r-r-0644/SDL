/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2018 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2018 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>

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

#ifndef SDL_render_wiiu_h
#define SDL_render_wiiu_h

#include "../SDL_sysrender.h"

//Utility/helper functions
static Uint32 PixelFormatByteSizeWIIU(Uint32 format);
static GX2SurfaceFormat PixelFormatToWIIUFMT(Uint32 format);
static Uint32 TextureNextPow2(Uint32 w);

//Driver internal data structures
typedef struct
{
    GX2Sampler sampler;
    GX2ColorBuffer cbuf;
    GX2ContextState ctx;
    GX2RBuffer texPositionBuffer;
    GX2RBuffer texCoordBuffer;
    GX2RBuffer texColourBuffer;
} WIIU_RenderData;

//SDL_render API implementation
static SDL_Renderer *WIIU_CreateRenderer(SDL_Window * window, Uint32 flags);
static void WIIU_WindowEvent(SDL_Renderer * renderer,
                             const SDL_WindowEvent *event);
static int WIIU_GetOutputSize(SDL_Renderer * renderer, int *w, int *h);
static int WIIU_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
// SDL changes colour/alpha/blend values internally, this is just to notify us.
// We don't care yet. TODO: could update GX2RBuffers less frequently with these?
/*static int WIIU_SetTextureColorMod(SDL_Renderer * renderer,
                                   SDL_Texture * texture);
static int WIIU_SetTextureAlphaMod(SDL_Renderer * renderer,
                                   SDL_Texture * texture);
static int WIIU_SetTextureBlendMode(SDL_Renderer * renderer,
                                    SDL_Texture * texture);*/
static int WIIU_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                              const SDL_Rect * rect, const void *pixels,
                              int pitch);
static int WIIU_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect, void **pixels, int *pitch);
static void WIIU_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int WIIU_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture);
static int WIIU_UpdateViewport(SDL_Renderer * renderer);
static int WIIU_UpdateClipRect(SDL_Renderer * renderer);
static int WIIU_RenderClear(SDL_Renderer * renderer);
static int WIIU_RenderDrawPoints(SDL_Renderer * renderer,
                                 const SDL_FPoint * points, int count);
static int WIIU_RenderDrawLines(SDL_Renderer * renderer,
                                const SDL_FPoint * points, int count);
static int WIIU_RenderFillRects(SDL_Renderer * renderer,
                                const SDL_FRect * rects, int count);
static int WIIU_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                           const SDL_Rect * srcrect, const SDL_FRect * dstrect);
static int WIIU_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                             const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                             const double angle, const SDL_FPoint * center, const SDL_RendererFlip flip);
static int WIIU_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                                 Uint32 format, void * pixels, int pitch);
static void WIIU_RenderPresent(SDL_Renderer * renderer);
static void WIIU_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static void WIIU_DestroyRenderer(SDL_Renderer * renderer);

#endif //SDL_render_wiiu_h
