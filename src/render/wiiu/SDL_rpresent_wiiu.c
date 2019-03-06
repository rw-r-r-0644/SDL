#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_WIIU

#include "../../video/wiiu/SDL_wiiuvideo.h"
#include "../../video/wiiu/wiiu_shaders.h"
#include "../SDL_sysrender.h"
#include "SDL_hints.h"
#include "SDL_render_wiiu.h"

#include <whb/gfx.h>
#include <gx2/draw.h>
#include <gx2/state.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>

#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

static const float u_viewSize[4] = {(float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};

static void render_scene(SDL_Renderer * renderer) {
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    WIIU_TextureData *tdata = (WIIU_TextureData *) data->windowTex.driverdata;

    float tex_w = tdata->u_texSize[0];
    float tex_h = tdata->u_texSize[1];
    int win_x, win_y, win_w, win_h;
    GX2RBuffer *a_position, *a_texCoord;
    WIIUVec2 *a_position_vals, *a_texCoord_vals;
    float u_mod[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /*  Allocate attribute buffers */
    a_position = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });
    a_texCoord = WIIU_AllocRenderData(data, (GX2RBuffer) {
        .flags =
            GX2R_RESOURCE_BIND_VERTEX_BUFFER |
            GX2R_RESOURCE_USAGE_CPU_WRITE,
        .elemSize = sizeof(WIIUVec2), // float x/y for each corner
        .elemCount = 4, // 4 corners
    });

/*  Calculate and save positions */
    if (SDL_GetWindowFlags(renderer->window) & SDL_WINDOW_FULLSCREEN) {
        win_x = 0;
        win_y = 0;
        win_w = SCREEN_WIDTH;
        win_h = SCREEN_HEIGHT;
    } else {
    /*  Center */
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

/*  Compute texture coords */
    a_texCoord_vals = GX2RLockBufferEx(a_texCoord, 0);
    a_texCoord_vals[0] = (WIIUVec2) {.x = 0.0f,  .y = tex_h};
    a_texCoord_vals[1] = (WIIUVec2) {.x = tex_w, .y = tex_h};
    a_texCoord_vals[2] = (WIIUVec2) {.x = tex_w, .y = 0.0f};
    a_texCoord_vals[3] = (WIIUVec2) {.x = 0.0f,  .y = 0.0f};
    GX2RUnlockBufferEx(a_position, 0);

    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    wiiuSetTextureShader();

    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[0].offset, 4, (uint32_t *)u_viewSize);
    GX2SetVertexUniformReg(wiiuTextureShader.vertexShader->uniformVars[1].offset, 4, (uint32_t *)tdata->u_texSize);
    GX2SetPixelUniformReg(wiiuTextureShader.pixelShader->uniformVars[0].offset, 4, (uint32_t*)u_mod);

    GX2RSetAttributeBuffer(a_position, 0, a_position->elemSize, 0);
    GX2RSetAttributeBuffer(a_texCoord, 1, a_texCoord->elemSize, 0);

    GX2SetPixelTexture(&tdata->texture, wiiuTextureShader.pixelShader->samplerVars[0].location);
    GX2SetPixelSampler(&tdata->sampler, wiiuTextureShader.pixelShader->samplerVars[0].location);

    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

void WIIU_SDL_RenderPresent(SDL_Renderer * renderer)
{
    WIIU_RenderData *data = (WIIU_RenderData *) renderer->driverdata;
    //SDL_Window *window = renderer->window;

    //GX2Flush();

    WHBGfxBeginRender();

    WHBGfxBeginRenderTV();
    render_scene(renderer);
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    render_scene(renderer);
    WHBGfxFinishRenderDRC();

    WHBGfxFinishRender();

    WIIU_FreeRenderData(data);
}

#endif //SDL_VIDEO_RENDER_WIIU
