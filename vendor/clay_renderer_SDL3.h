// clay_renderer_SDL3.h  
//  
// Drop-in replacement for the SDL3_ttf + SDL3_image Clay renderer.  
// Uses stb_truetype (font atlas) and stb_image (image loading) instead.  
//  
// In exactly ONE .c file, before including this header:  
//   #define CLAY_SDL3_IMPLEMENTATION  
//   #include "clay_renderer_SDL3.h"  
//  
// All other files may include this header without the define.  
// made with deepwiki 
#ifndef CLAY_RENDERER_SDL3_H  
#define CLAY_RENDERER_SDL3_H  
  
#include <clay.h>  
#include <SDL3/SDL.h>  
#include <stdint.h>  
#include <stdlib.h>  
#include <string.h>  
  
// ── Atlas configuration ───────────────────────────────────────────────────────  
#define CLAY_STB_ATLAS_W      1024  
#define CLAY_STB_ATLAS_H      1024  
#define CLAY_STB_FIRST_CHAR   32  
#define CLAY_STB_NUM_CHARS    96  
#define CLAY_STB_MAX_ATLASES  64  
  
// ── Public types ──────────────────────────────────────────────────────────────  
  
// chardata stored as raw bytes so this header compiles without stb_truetype.h  
// in non-implementation translation units. sizeof(stbtt_packedchar) == 28.  
typedef struct {  
    int           font_id;  
    float         font_size;  
    unsigned char chardata[96 * 28];  
    SDL_Texture  *texture;  
} Clay_STB_FontAtlas;  
  
typedef struct {  
    const unsigned char *data;  
    int                  size;  
} Clay_STB_Font;  
  
typedef struct {  
    SDL_Renderer      *renderer;  
    Clay_STB_Font     *fonts;  
    int                num_fonts;  
    Clay_STB_FontAtlas atlases[CLAY_STB_MAX_ATLASES];  
    int                num_atlases;  
} Clay_SDL3RendererData;  
  
// ── Public API ────────────────────────────────────────────────────────────────  
SDL_Texture    *Clay_SDL3_LoadTextureFromFile(SDL_Renderer *renderer, const char *path);  
SDL_Texture    *Clay_SDL3_LoadTextureFromMemory(SDL_Renderer *renderer,  
                                                const unsigned char *data, int len);  
void            Clay_SDL3_DestroyRendererData(Clay_SDL3RendererData *rd);  
Clay_Dimensions Clay_STB_MeasureText(Clay_StringSlice text,  
                                     Clay_TextElementConfig *config,  
                                     void *userData);  
void            SDL_Clay_RenderClayCommands(Clay_SDL3RendererData *rendererData,  
                                            Clay_RenderCommandArray *rcommands);  
  
#endif // CLAY_RENDERER_SDL3_H  
  
  
// -----------------------------------------  
// IMPLEMENTATION --------------------------  
// -----------------------------------------  
#ifdef CLAY_SDL3_IMPLEMENTATION  
#undef CLAY_SDL3_IMPLEMENTATION  
  
#define STB_TRUETYPE_IMPLEMENTATION  
#include "stb_truetype.h"  
#define STB_IMAGE_IMPLEMENTATION  
#include "stb_image.h"  
#include <SDL3/SDL_main.h>  
  
static int NUM_CIRCLE_SEGMENTS = 16;  
  
// ── Internal: lazily bake and cache a font atlas ──────────────────────────────  
static Clay_STB_FontAtlas *Clay_STB_GetOrCreateAtlas(  
        Clay_SDL3RendererData *rd, int font_id, float font_size)  
{  
    for (int i = 0; i < rd->num_atlases; i++) {  
        if (rd->atlases[i].font_id == font_id &&  
            rd->atlases[i].font_size == font_size)  
            return &rd->atlases[i];  
    }  
  
    if (rd->num_atlases >= CLAY_STB_MAX_ATLASES) {  
        SDL_Log("Clay_STB: atlas cache full (increase CLAY_STB_MAX_ATLASES)");  
        return NULL;  
    }  
    if (font_id < 0 || font_id >= rd->num_fonts || !rd->fonts[font_id].data) {  
        SDL_Log("Clay_STB: invalid font_id %d", font_id);  
        return NULL;  
    }  
  
    Clay_STB_FontAtlas *atlas = &rd->atlases[rd->num_atlases++];  
    atlas->font_id   = font_id;  
    atlas->font_size = font_size;  
  
    unsigned char *a8 = (unsigned char *)SDL_calloc(CLAY_STB_ATLAS_W * CLAY_STB_ATLAS_H, 1);  
    if (!a8) { rd->num_atlases--; return NULL; }  
  
    stbtt_pack_context pc;  
    stbtt_PackBegin(&pc, a8, CLAY_STB_ATLAS_W, CLAY_STB_ATLAS_H,  
                    CLAY_STB_ATLAS_W, 1, NULL);  
    stbtt_PackSetOversampling(&pc, 2, 2);  
    stbtt_PackFontRange(&pc, rd->fonts[font_id].data, 0,  
                        font_size, CLAY_STB_FIRST_CHAR, CLAY_STB_NUM_CHARS,  
                        (stbtt_packedchar *)atlas->chardata);  
    stbtt_PackEnd(&pc);  
  
    unsigned char *rgba = (unsigned char *)SDL_malloc(CLAY_STB_ATLAS_W * CLAY_STB_ATLAS_H * 4);  
    if (!rgba) { SDL_free(a8); rd->num_atlases--; return NULL; }  
  
    for (int i = 0; i < CLAY_STB_ATLAS_W * CLAY_STB_ATLAS_H; i++) {  
        rgba[i * 4 + 0] = 255;  
        rgba[i * 4 + 1] = 255;  
        rgba[i * 4 + 2] = 255;  
        rgba[i * 4 + 3] = a8[i];  
    }  
    SDL_free(a8);  
  
    atlas->texture = SDL_CreateTexture(rd->renderer,  
                                       SDL_PIXELFORMAT_RGBA32,  
                                       SDL_TEXTUREACCESS_STATIC,  
                                       CLAY_STB_ATLAS_W, CLAY_STB_ATLAS_H);  
    if (!atlas->texture) { SDL_free(rgba); rd->num_atlases--; return NULL; }  
  
    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);  
    SDL_UpdateTexture(atlas->texture, NULL, rgba, CLAY_STB_ATLAS_W * 4);  
    SDL_free(rgba);  
  
    return atlas;  
}  
  
// ── Image helpers ─────────────────────────────────────────────────────────────  
SDL_Texture *Clay_SDL3_LoadTextureFromFile(SDL_Renderer *renderer, const char *path)  
{  
    int w, h, channels;  
    unsigned char *pixels = stbi_load(path, &w, &h, &channels, 4);  
    if (!pixels) {  
        SDL_Log("stbi_load('%s') failed: %s", path, stbi_failure_reason());  
        return NULL;  
    }  
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,  
                                          SDL_TEXTUREACCESS_STATIC, w, h);  
    if (tex) {  
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);  
        SDL_UpdateTexture(tex, NULL, pixels, w * 4);  
    }  
    stbi_image_free(pixels);  
    return tex;  
}  
  
SDL_Texture *Clay_SDL3_LoadTextureFromMemory(SDL_Renderer *renderer,  
                                              const unsigned char *data, int len)  
{  
    int w, h, channels;  
    unsigned char *pixels = stbi_load_from_memory(data, len, &w, &h, &channels, 4);  
    if (!pixels) {  
        SDL_Log("stbi_load_from_memory failed: %s", stbi_failure_reason());  
        return NULL;  
    }  
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,  
                                          SDL_TEXTUREACCESS_STATIC, w, h);  
    if (tex) {  
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);  
        SDL_UpdateTexture(tex, NULL, pixels, w * 4);  
    }  
    stbi_image_free(pixels);  
    return tex;  
}  
  
// ── Cleanup ───────────────────────────────────────────────────────────────────  
void Clay_SDL3_DestroyRendererData(Clay_SDL3RendererData *rd)  
{  
    for (int i = 0; i < rd->num_atlases; i++) {  
        if (rd->atlases[i].texture)  
            SDL_DestroyTexture(rd->atlases[i].texture);  
    }  
    rd->num_atlases = 0;  
}  
  
// ── Rounded rect ──────────────────────────────────────────────────────────────  
static void SDL_Clay_RenderFillRoundedRect(  
        Clay_SDL3RendererData *rendererData,  
        const SDL_FRect rect, const float cornerRadius, const Clay_Color _color)  
{  
    const SDL_FColor color = {  
        _color.r / 255.0f, _color.g / 255.0f,  
        _color.b / 255.0f, _color.a / 255.0f  
    };  
  
    int indexCount = 0, vertexCount = 0;  
  
    const float minRadius     = SDL_min(rect.w, rect.h) / 2.0f;  
    const float clampedRadius = SDL_min(cornerRadius, minRadius);  
    const int   numCircleSegs = SDL_max(NUM_CIRCLE_SEGMENTS, (int)(clampedRadius * 0.5f));  
  
    int totalVertices = 4 + (4 * (numCircleSegs * 2)) + 2 * 4;  
    int totalIndices  = 6 + (4 * (numCircleSegs * 3)) + 6 * 4;  
  
    SDL_Vertex vertices[totalVertices];  
    int        indices[totalIndices];  
  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius,          rect.y + clampedRadius},          color, {0,0} };  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + clampedRadius},          color, {1,0} };  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + rect.h - clampedRadius}, color, {1,1} };  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius,          rect.y + rect.h - clampedRadius}, color, {0,1} };  
  
    indices[indexCount++] = 0; indices[indexCount++] = 1; indices[indexCount++] = 3;  
    indices[indexCount++] = 1; indices[indexCount++] = 2; indices[indexCount++] = 3;  
  
    const float step = (SDL_PI_F / 2.0f) / numCircleSegs;  
    for (int i = 0; i < numCircleSegs; i++) {  
        const float a1 = (float)i * step;  
        const float a2 = ((float)i + 1.0f) * step;  
  
        for (int j = 0; j < 4; j++) {  
            float cx, cy, sx, sy;  
            switch (j) {  
                case 0: cx = rect.x + clampedRadius;          cy = rect.y + clampedRadius;          sx = -1; sy = -1; break;  
                case 1: cx = rect.x + rect.w - clampedRadius; cy = rect.y + clampedRadius;          sx =  1; sy = -1; break;  
                case 2: cx = rect.x + rect.w - clampedRadius; cy = rect.y + rect.h - clampedRadius; sx =  1; sy =  1; break;  
                case 3: cx = rect.x + clampedRadius;          cy = rect.y + rect.h - clampedRadius; sx = -1; sy =  1; break;  
                default: return;  
            }  
            vertices[vertexCount++] = (SDL_Vertex){ {cx + SDL_cosf(a1) * clampedRadius * sx, cy + SDL_sinf(a1) * clampedRadius * sy}, color, {0,0} };  
            vertices[vertexCount++] = (SDL_Vertex){ {cx + SDL_cosf(a2) * clampedRadius * sx, cy + SDL_sinf(a2) * clampedRadius * sy}, color, {0,0} };  
            indices[indexCount++] = j;  
            indices[indexCount++] = vertexCount - 2;  
            indices[indexCount++] = vertexCount - 1;  
        }  
    }  
  
    // Top edge  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius,          rect.y}, color, {0,0} };  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y}, color, {1,0} };  
    indices[indexCount++] = 0; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;  
    indices[indexCount++] = 1; indices[indexCount++] = 0;             indices[indexCount++] = vertexCount-1;  
  
    // Right edge  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w, rect.y + clampedRadius},          color, {1,0} };  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w, rect.y + rect.h - clampedRadius}, color, {1,1} };  
    indices[indexCount++] = 1; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;  
    indices[indexCount++] = 2; indices[indexCount++] = 1;             indices[indexCount++] = vertexCount-1;  
  
    // Bottom edge  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + rect.h}, color, {1,1} };  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius,          rect.y + rect.h}, color, {0,1} };  
    indices[indexCount++] = 2; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;  
    indices[indexCount++] = 3; indices[indexCount++] = 2;             indices[indexCount++] = vertexCount-1;  
  
    // Left edge  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x, rect.y + rect.h - clampedRadius}, color, {0,1} };  
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x, rect.y + clampedRadius},          color, {0,0} };  
    indices[indexCount++] = 3; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;  
    indices[indexCount++] = 0; indices[indexCount++] = 3;             indices[indexCount++] = vertexCount-1;  
  
    SDL_RenderGeometry(rendererData->renderer, NULL, vertices, vertexCount, indices, indexCount);  
}  
  
// ── Arc ───────────────────────────────────────────────────────────────────────  
static void SDL_Clay_RenderArc(  
        Clay_SDL3RendererData *rendererData,  
        const SDL_FPoint center, const float radius,  
        const float startAngle, const float endAngle,  
        const float thickness, const Clay_Color color)  
{  
    SDL_SetRenderDrawColor(rendererData->renderer,  
                           color.r, color.g, color.b, color.a);  
  
    const float radStart  = startAngle * (SDL_PI_F / 180.0f);  
    const float radEnd    = endAngle   * (SDL_PI_F / 180.0f);  
    const int   numSegs   = SDL_max(NUM_CIRCLE_SEGMENTS, (int)(radius * 1.5f));  
    const float angleStep = (radEnd - radStart) / (float)numSegs;  
    const float thicknessStep = 0.4f;  
  
    for (float t = thicknessStep; t < thickness - thicknessStep; t += thicknessStep) {  
        SDL_FPoint points[numSegs + 1];  
        const float r = SDL_max(radius - t, 1.0f);  
        for (int i = 0; i <= numSegs; i++) {  
            const float angle = radStart + i * angleStep;  
            points[i] = (SDL_FPoint){  
                SDL_roundf(center.x + SDL_cosf(angle) * r),  
                SDL_roundf(center.y + SDL_sinf(angle) * r)  
            };  
        }  
        SDL_RenderLines(rendererData->renderer, points, numSegs + 1);  
    }  
}  
  
// ── Text rendering ────────────────────────────────────────────────────────────  
static void Clay_STB_RenderText(  
        Clay_SDL3RendererData *rd,  
        const char *text, int text_len,  
        float x, float y,  
        int font_id, float font_size,  
        Clay_Color clay_color)  
{  
    Clay_STB_FontAtlas *atlas = Clay_STB_GetOrCreateAtlas(rd, font_id, font_size);  
    if (!atlas) return;  
  
    stbtt_fontinfo info;  
    stbtt_InitFont(&info, rd->fonts[font_id].data,  
                   stbtt_GetFontOffsetForIndex(rd->fonts[font_id].data, 0));  
    int ascent_raw, descent_raw, line_gap_raw;  
    stbtt_GetFontVMetrics(&info, &ascent_raw, &descent_raw, &line_gap_raw);  
    float scale    = stbtt_ScaleForPixelHeight(&info, font_size);  
    float baseline = y + ascent_raw * scale;  
  
    enum { STACK_MAX = 512 };  
    SDL_Vertex  stack_verts[STACK_MAX * 6];  
    SDL_Vertex *verts      = stack_verts;  
    SDL_Vertex *heap_verts = NULL;  
  
    if (text_len > STACK_MAX) {  
        heap_verts = (SDL_Vertex *)SDL_malloc(text_len * 6 * sizeof(SDL_Vertex));  
        if (!heap_verts) return;  
        verts = heap_verts;  
    }  
  
    SDL_FColor color = {  
        clay_color.r / 255.0f, clay_color.g / 255.0f,  
        clay_color.b / 255.0f, clay_color.a / 255.0f  
    };  
  
    int   nv = 0;  
    float cx = x, cy = baseline;  
  
    for (int i = 0; i < text_len; i++) {  
        int c = (unsigned char)text[i];  
        if (c < CLAY_STB_FIRST_CHAR || c >= CLAY_STB_FIRST_CHAR + CLAY_STB_NUM_CHARS)  
            continue;  
  
        stbtt_aligned_quad q;  
        stbtt_GetPackedQuad((stbtt_packedchar *)atlas->chardata,  
                            CLAY_STB_ATLAS_W, CLAY_STB_ATLAS_H,  
                            c - CLAY_STB_FIRST_CHAR,  
                            &cx, &cy, &q, 1);  
  
        SDL_Vertex tl = { {q.x0, q.y0}, color, {q.s0, q.t0} };  
        SDL_Vertex tr = { {q.x1, q.y0}, color, {q.s1, q.t0} };  
        SDL_Vertex br = { {q.x1, q.y1}, color, {q.s1, q.t1} };  
        SDL_Vertex bl = { {q.x0, q.y1}, color, {q.s0, q.t1} };  
  
        verts[nv++] = tl; verts[nv++] = tr; verts[nv++] = br;  
        verts[nv++] = tl; verts[nv++] = br; verts[nv++] = bl;  
    }  
  
    SDL_SetTextureColorMod(atlas->texture, clay_color.r, clay_color.g, clay_color.b);  
    SDL_SetTextureAlphaMod(atlas->texture, clay_color.a);  
    SDL_RenderGeometry(rd->renderer, atlas->texture, verts, nv, NULL, 0);  
  
    if (heap_verts) SDL_free(heap_verts);  
}  
  
// ── Main render loop ──────────────────────────────────────────────────────────  
static SDL_Rect currentClippingRectangle;  
  
void SDL_Clay_RenderClayCommands(  
        Clay_SDL3RendererData *rendererData,  
        Clay_RenderCommandArray *rcommands)  
{  
    for (size_t i = 0; i < rcommands->length; i++) {  
        Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);  
        const Clay_BoundingBox bb   = rcmd->boundingBox;  
        const SDL_FRect        rect = { bb.x, bb.y, bb.width, bb.height };  
  
        switch (rcmd->commandType) {  
  
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {  
                Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;  
                SDL_SetRenderDrawBlendMode(rendererData->renderer, SDL_BLENDMODE_BLEND);  
                SDL_SetRenderDrawColor(rendererData->renderer,  
                    config->backgroundColor.r, config->backgroundColor.g,  
                    config->backgroundColor.b, config->backgroundColor.a);  
                if (config->cornerRadius.topLeft > 0) {  
                    SDL_Clay_RenderFillRoundedRect(rendererData, rect,  
                        config->cornerRadius.topLeft, config->backgroundColor);  
                } else {  
                    SDL_RenderFillRect(rendererData->renderer, &rect);  
                }  
            } break;  
  
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {  
                Clay_TextRenderData *config = &rcmd->renderData.text;  
                Clay_STB_RenderText(  
                    rendererData,  
                    config->stringContents.chars,  
                    (int)config->stringContents.length,  
                    rect.x, rect.y,  
                    (int)config->fontId,  
                    (float)config->fontSize,  
                    config->textColor);  
            } break;  
  
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {  
                Clay_BorderRenderData *config = &rcmd->renderData.border;  
  
                const float minRadius = SDL_min(rect.w, rect.h) / 2.0f;  
                const Clay_CornerRadius cr = {  
                    .topLeft     = SDL_min(config->cornerRadius.topLeft,     minRadius),  
                    .topRight    = SDL_min(config->cornerRadius.topRight,    minRadius),  
                    .bottomLeft  = SDL_min(config->cornerRadius.bottomLeft,  minRadius),  
                    .bottomRight = SDL_min(config->cornerRadius.bottomRight, minRadius),  
                };  
  
                SDL_SetRenderDrawColor(rendererData->renderer,  
                    config->color.r, config->color.g,  
                    config->color.b, config->color.a);  
  
                if (config->width.left > 0) {  
                    SDL_FRect line = { rect.x - 1, rect.y + cr.topLeft,  
                                       config->width.left,  
                                       rect.h - cr.topLeft - cr.bottomLeft };  
                    SDL_RenderFillRect(rendererData->renderer, &line);  
                }  
                if (config->width.right > 0) {  
                    SDL_FRect line = { rect.x + rect.w - config->width.right + 1,  
                                       rect.y + cr.topRight,  
                                       config->width.right,  
                                       rect.h - cr.topRight - cr.bottomRight };  
                    SDL_RenderFillRect(rendererData->renderer, &line);  
                }  
                if (config->width.top > 0) {  
                    SDL_FRect line = { rect.x + cr.topLeft, rect.y - 1,  
                                       rect.w - cr.topLeft - cr.topRight,  
                                       config->width.top };  
                    SDL_RenderFillRect(rendererData->renderer, &line);  
                }  
                if (config->width.bottom > 0) {  
                    SDL_FRect line = { rect.x + cr.bottomLeft,  
                                       rect.y + rect.h - config->width.bottom + 1,  
                                       rect.w - cr.bottomLeft - cr.bottomRight,  
                                       config->width.bottom };  
                    SDL_RenderFillRect(rendererData->renderer, &line);  
                }  
  
                if (config->cornerRadius.topLeft > 0)  
                    SDL_Clay_RenderArc(rendererData,  
                        (SDL_FPoint){ rect.x + cr.topLeft - 1, rect.y + cr.topLeft - 1 },  
                        cr.topLeft, 180.0f, 270.0f, config->width.top, config->color);  
                if (config->cornerRadius.topRight > 0)  
                    SDL_Clay_RenderArc(rendererData,  
                        (SDL_FPoint){ rect.x + rect.w - cr.topRight, rect.y + cr.topRight - 1 },  
                        cr.topRight, 270.0f, 360.0f, config->width.top, config->color);  
                if (config->cornerRadius.bottomLeft > 0)  
                    SDL_Clay_RenderArc(rendererData,  
                        (SDL_FPoint){ rect.x + cr.bottomLeft - 1, rect.y + rect.h - cr.bottomLeft },  
                        cr.bottomLeft, 90.0f, 180.0f, config->width.bottom, config->color);  
                if (config->cornerRadius.bottomRight > 0)  
                    SDL_Clay_RenderArc(rendererData,  
                        (SDL_FPoint){ rect.x + rect.w - cr.bottomRight, rect.y + rect.h - cr.bottomRight },  
                        cr.bottomRight, 0.0f, 90.0f, config->width.bottom, config->color);  
            } break;  
  
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {  
                currentClippingRectangle = (SDL_Rect){  
                    .x = (int)bb.x, .y = (int)bb.y,  
                    .w = (int)bb.width, .h = (int)bb.height,  
                };  
                SDL_SetRenderClipRect(rendererData->renderer, &currentClippingRectangle);  
            } break;  
  
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {  
                SDL_SetRenderClipRect(rendererData->renderer, NULL);  
            } break;  
  
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {  
                SDL_Texture *texture = (SDL_Texture *)rcmd->renderData.image.imageData;  
                const SDL_FRect dest = { rect.x, rect.y, rect.w, rect.h };  
                SDL_RenderTexture(rendererData->renderer, texture, NULL, &dest);  
            } break;  
  
            default:  
                SDL_Log("Unknown render command type: %d", rcmd->commandType);  
                break;  
        }  
    }  
}  
  
// ── Text measurement ──────────────────────────────────────────────────────────  
Clay_Dimensions Clay_STB_MeasureText(  
        Clay_StringSlice text,  
        Clay_TextElementConfig *config,  
        void *userData)  
{  
    Clay_SDL3RendererData *rd = (Clay_SDL3RendererData *)userData;  
  
    Clay_STB_FontAtlas *atlas =  
        Clay_STB_GetOrCreateAtlas(rd, (int)config->fontId, (float)config->fontSize);  
    if (!atlas) return (Clay_Dimensions){0, 0};  
  
    stbtt_fontinfo info;  
    stbtt_InitFont(&info, rd->fonts[config->fontId].data,  
                   stbtt_GetFontOffsetForIndex(rd->fonts[config->fontId].data, 0));  
    int asc, desc, gap;  
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);  
    float scale  = stbtt_ScaleForPixelHeight(&info, (float)config->fontSize);  
    float height = (asc - desc) * scale;  
  
    float cx = 0, cy = 0;  
    for (int i = 0; i < text.length; i++) {  
        int c = (unsigned char)text.chars[i];  
        if (c < CLAY_STB_FIRST_CHAR || c >= CLAY_STB_FIRST_CHAR + CLAY_STB_NUM_CHARS)  
            continue;  
        stbtt_aligned_quad q;  
        stbtt_GetPackedQuad((stbtt_packedchar *)atlas->chardata,  
                            CLAY_STB_ATLAS_W, CLAY_STB_ATLAS_H,  
                            c - CLAY_STB_FIRST_CHAR,  
                            &cx, &cy, &q, 0);  
    }  
  
    return (Clay_Dimensions){ cx, height };  
}  
  
#endif // CLAY_SDL3_IMPLEMENTATION
