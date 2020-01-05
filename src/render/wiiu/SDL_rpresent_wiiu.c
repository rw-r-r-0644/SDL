/*
  Simple DirectMedia Layer
  Copyright (C) 2018-2019 Ash Logan <ash@heyquark.com>
  Copyright (C) 2018-2019 Roberto Van Eeden <r.r.qwertyuiop.r.r@gmail.com>

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

#if SDL_VIDEO_RENDER_WIIU

#include "../../video/wiiu/wiiu_shaders.h"
#include "../SDL_sysrender.h"
#include "SDL_render_wiiu.h"

#include <whb/gfx.h>
#include <gx2/registers.h>
#include <gx2/state.h>
#include <gx2/draw.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

static const WIIUVec4 u_viewSize = {.x = (float)SCREEN_WIDTH, .y = (float)SCREEN_HEIGHT};

static void render_scene(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) data->windowTex.driverdata;

    float tex_w = tdata->u_texSize.x;
    float tex_h = tdata->u_texSize.y;
    int win_x, win_y, win_w, win_h;
    GX2RBuffer *a_position, *a_texCoord;
    WIIUVec2 *a_position_vals, *a_texCoord_vals;

    /* Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), /* float x/y for each corner */
        .elemCount = 4, /* 4 corners */
    });
    a_texCoord = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });

    /* Calculate and save positions */
    if (SDL_GetWindowFlags(renderer->window) & SDL_WINDOW_FULLSCREEN) {
        win_x = 0;
        win_y = 0;
        win_w = SCREEN_WIDTH;
        win_h = SCREEN_HEIGHT;
    } else {
        /* Center */
        SDL_GetWindowSize(renderer->window, &win_w, &win_h);
        win_x = (SCREEN_WIDTH - win_w) / 2;
        win_y = (SCREEN_HEIGHT - win_h) / 2;
    }

    a_position_vals = GX2RLockBufferEx(a_position, 0);
    a_position_vals[0] = (WIIUVec2) {
        .x = win_x, .y = win_y
    };
    a_position_vals[1] = (WIIUVec2) {
        .x = win_x + win_w, .y = win_y
    };
    a_position_vals[2] = (WIIUVec2) {
        .x = win_x + win_w, .y = win_y + win_h
    };
    a_position_vals[3] = (WIIUVec2) {
        .x = win_x, .y = win_y + win_h
    };
    GX2RUnlockBufferEx(a_position, 0);

    /* Compute texture coords */
    a_texCoord_vals = GX2RLockBufferEx(a_texCoord, 0);
    a_texCoord_vals[0] = (WIIUVec2) {.x = 0.0f,  .y = tex_h};
    a_texCoord_vals[1] = (WIIUVec2) {.x = tex_w, .y = tex_h};
    a_texCoord_vals[2] = (WIIUVec2) {.x = tex_w, .y = 0.0f};
    a_texCoord_vals[3] = (WIIUVec2) {.x = 0.0f,  .y = 0.0f};
    GX2RUnlockBufferEx(a_texCoord, 0);

    /* Render the window */
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    wiiuSetTextureShader();

    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)&u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)&tdata->u_texSize);
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)&tdata->u_mod);

    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2RSetAttributeBuffer(a_texCoord, 1, a_texCoord->elemSize, 0);

    GX2SetPixelTexture(&tdata->texture, wiiuTextureShader.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&tdata->sampler, wiiuTextureShader.pixelShader->samplerVars[0].location);

    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE);

    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

void WIIU_SDL_RenderPresent(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    Uint32 flags = SDL_GetWindowFlags(renderer->window);

    if (renderer->info.flags & SDL_RENDERER_PRESENTVSYNC) {
    /*  NOTE watch libwhb's source to ensure this call only does vsync */
        WHBGfxBeginRender();
    }

    /* Only render to TV if the window is *not* drc-only */
    if (!(flags & SDL_WINDOW_WIIU_GAMEPAD_ONLY)) {
        WHBGfxBeginRenderTV();
        render_scene(renderer);
        WHBGfxFinishRenderTV();
    }

    if (!(flags & SDL_WINDOW_WIIU_TV_ONLY)) {
        WHBGfxBeginRenderDRC();
        render_scene(renderer);
        WHBGfxFinishRenderDRC();
    }

    WHBGfxFinishRender();

    /* Free the list of render and draw data */
    WIIU_FreeRenderData(data);
    WIIU_TextureDoneRendering(data);

    /* Restore SDL context state */
    GX2SetContextState(data->ctx);
}

#endif /* SDL_VIDEO_RENDER_WIIU */
