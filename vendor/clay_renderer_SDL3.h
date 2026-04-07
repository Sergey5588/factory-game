#include "clay.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------
// Glyph cache entry
// ------------------------------------------------------------
typedef struct GlyphCacheEntry {
    uint32_t fontId;
    uint32_t fontSize;
    uint32_t codepoint;
    SDL_Texture *texture;
    int width, height;
    int bearingX, bearingY;   // offset from baseline
    int advance;              // advance width in pixels (scaled)
    struct GlyphCacheEntry *next;
} GlyphCacheEntry;

// ------------------------------------------------------------
// Renderer data (extended for stb_truetype)
// ------------------------------------------------------------
typedef struct {
    SDL_Renderer *renderer;
    stbtt_fontinfo *fontInfos;    // array indexed by fontId
    int numFonts;
    GlyphCacheEntry *glyphCache;  // simple linked list cache
} Clay_SDL3RendererData;

// ------------------------------------------------------------
// Global configuration
// ------------------------------------------------------------
static int NUM_CIRCLE_SEGMENTS = 16;
static SDL_Rect currentClippingRectangle;

// ------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------
static SDL_Texture* get_or_create_glyph_texture(Clay_SDL3RendererData *data,
                                                 uint32_t fontId,
                                                 uint32_t fontSize,
                                                 uint32_t codepoint,
                                                 int *out_width, int *out_height,
                                                 int *out_bearingX, int *out_bearingY,
                                                 int *out_advance);

// ------------------------------------------------------------
// Rounded rectangle rendering (unchanged)
// ------------------------------------------------------------
static void SDL_Clay_RenderFillRoundedRect(Clay_SDL3RendererData *rendererData,
                                           const SDL_FRect rect,
                                           const float cornerRadius,
                                           const Clay_Color _color) {
    const SDL_FColor color = { _color.r/255.0f, _color.g/255.0f, _color.b/255.0f, _color.a/255.0f };
    int indexCount = 0, vertexCount = 0;
    const float minRadius = SDL_min(rect.w, rect.h) / 2.0f;
    const float clampedRadius = SDL_min(cornerRadius, minRadius);
    const int numCircleSegments = SDL_max(NUM_CIRCLE_SEGMENTS, (int)clampedRadius * 0.5f);
    int totalVertices = 4 + (4 * (numCircleSegments * 2)) + 2*4;
    int totalIndices = 6 + (4 * (numCircleSegments * 3)) + 6*4;
    SDL_Vertex vertices[totalVertices];
    int indices[totalIndices];

    // center rectangle
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y + clampedRadius}, color, {0,0} };
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + clampedRadius}, color, {1,0} };
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + rect.h - clampedRadius}, color, {1,1} };
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y + rect.h - clampedRadius}, color, {0,1} };
    indices[indexCount++] = 0; indices[indexCount++] = 1; indices[indexCount++] = 3;
    indices[indexCount++] = 1; indices[indexCount++] = 2; indices[indexCount++] = 3;

    // rounded corners (triangle fans)
    const float step = (SDL_PI_F/2) / numCircleSegments;
    for (int i = 0; i < numCircleSegments; i++) {
        const float angle1 = (float)i * step;
        const float angle2 = ((float)i + 1.0f) * step;
        for (int j = 0; j < 4; j++) {
            float cx, cy, signX, signY;
            switch (j) {
                case 0: cx = rect.x + clampedRadius; cy = rect.y + clampedRadius; signX = -1; signY = -1; break;
                case 1: cx = rect.x + rect.w - clampedRadius; cy = rect.y + clampedRadius; signX =  1; signY = -1; break;
                case 2: cx = rect.x + rect.w - clampedRadius; cy = rect.y + rect.h - clampedRadius; signX =  1; signY =  1; break;
                case 3: cx = rect.x + clampedRadius; cy = rect.y + rect.h - clampedRadius; signX = -1; signY =  1; break;
                default: return;
            }
            vertices[vertexCount++] = (SDL_Vertex){ {cx + SDL_cosf(angle1) * clampedRadius * signX,
                                                     cy + SDL_sinf(angle1) * clampedRadius * signY}, color, {0,0} };
            vertices[vertexCount++] = (SDL_Vertex){ {cx + SDL_cosf(angle2) * clampedRadius * signX,
                                                     cy + SDL_sinf(angle2) * clampedRadius * signY}, color, {0,0} };
            indices[indexCount++] = j;
            indices[indexCount++] = vertexCount - 2;
            indices[indexCount++] = vertexCount - 1;
        }
    }

    // edge rectangles
    // top
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y}, color, {0,0} };
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y}, color, {1,0} };
    indices[indexCount++] = 0; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;
    indices[indexCount++] = 1; indices[indexCount++] = 0; indices[indexCount++] = vertexCount-1;
    // right
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w, rect.y + clampedRadius}, color, {1,0} };
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w, rect.y + rect.h - clampedRadius}, color, {1,1} };
    indices[indexCount++] = 1; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;
    indices[indexCount++] = 2; indices[indexCount++] = 1; indices[indexCount++] = vertexCount-1;
    // bottom
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + rect.w - clampedRadius, rect.y + rect.h}, color, {1,1} };
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x + clampedRadius, rect.y + rect.h}, color, {0,1} };
    indices[indexCount++] = 2; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;
    indices[indexCount++] = 3; indices[indexCount++] = 2; indices[indexCount++] = vertexCount-1;
    // left
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x, rect.y + rect.h - clampedRadius}, color, {0,1} };
    vertices[vertexCount++] = (SDL_Vertex){ {rect.x, rect.y + clampedRadius}, color, {0,0} };
    indices[indexCount++] = 3; indices[indexCount++] = vertexCount-2; indices[indexCount++] = vertexCount-1;
    indices[indexCount++] = 0; indices[indexCount++] = 3; indices[indexCount++] = vertexCount-1;

    SDL_RenderGeometry(rendererData->renderer, NULL, vertices, vertexCount, indices, indexCount);
}

// ------------------------------------------------------------
// Arc rendering (unchanged)
// ------------------------------------------------------------
static void SDL_Clay_RenderArc(Clay_SDL3RendererData *rendererData,
                               const SDL_FPoint center,
                               const float radius,
                               const float startAngle,
                               const float endAngle,
                               const float thickness,
                               const Clay_Color color) {
    SDL_SetRenderDrawColor(rendererData->renderer, color.r, color.g, color.b, color.a);
    const float radStart = startAngle * (SDL_PI_F / 180.0f);
    const float radEnd = endAngle * (SDL_PI_F / 180.0f);
    const int numCircleSegments = SDL_max(NUM_CIRCLE_SEGMENTS, (int)(radius * 1.5f));
    const float angleStep = (radEnd - radStart) / (float)numCircleSegments;
    const float thicknessStep = 0.4f;

    for (float t = thicknessStep; t < thickness - thicknessStep; t += thicknessStep) {
        SDL_FPoint points[numCircleSegments + 1];
        const float clampedRadius = SDL_max(radius - t, 1.0f);
        for (int i = 0; i <= numCircleSegments; i++) {
            const float angle = radStart + i * angleStep;
            points[i] = (SDL_FPoint){
                SDL_roundf(center.x + SDL_cosf(angle) * clampedRadius),
                SDL_roundf(center.y + SDL_sinf(angle) * clampedRadius)
            };
        }
        SDL_RenderLines(rendererData->renderer, points, numCircleSegments + 1);
    }
}

// ------------------------------------------------------------
// Text rendering using stb_truetype and glyph cache
// ------------------------------------------------------------
static void SDL_Clay_RenderText(Clay_SDL3RendererData *data,
                                const SDL_FRect rect,
                                Clay_TextRenderData *config) {
    if (config->fontId >= data->numFonts) return;
    stbtt_fontinfo *fontInfo = &data->fontInfos[config->fontId];
    float scale = stbtt_ScaleForPixelHeight(fontInfo, (float)config->fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(fontInfo, &ascent, &descent, &lineGap);
    int baseline_y = (int)(rect.y + ascent * scale);

    float x = rect.x;
    const char *text_ptr = config->stringContents.chars;
    int remaining = config->stringContents.length;

    while (remaining > 0) {
        // Simple ASCII handling; extend to UTF-8 if needed
        uint32_t codepoint = (unsigned char)*text_ptr;
        if (codepoint == '\n') {
            x = rect.x;
            baseline_y += (ascent - descent + lineGap) * scale;
            text_ptr++; remaining--;
            continue;
        }
        int width, height, bearingX, bearingY, advance;
        SDL_Texture *tex = get_or_create_glyph_texture(data,
                                                       config->fontId,
                                                       config->fontSize,
                                                       codepoint,
                                                       &width, &height,
                                                       &bearingX, &bearingY,
                                                       &advance);
        if (tex) {
            SDL_FRect dst = {
                x + bearingX,
                baseline_y + bearingY - height,  // bearingY is offset from baseline (positive down)
                (float)width,
                (float)height
            };
            SDL_SetTextureColorMod(tex, config->textColor.r, config->textColor.g, config->textColor.b);
            SDL_SetTextureAlphaMod(tex, config->textColor.a);
            SDL_RenderTexture(data->renderer, tex, NULL, &dst);
        }
        x += advance;
        text_ptr++;
        remaining--;
    }
}

// ------------------------------------------------------------
// Glyph cache management
// ------------------------------------------------------------
static SDL_Texture* get_or_create_glyph_texture(Clay_SDL3RendererData *data,
                                                uint32_t fontId,
                                                uint32_t fontSize,
                                                uint32_t codepoint,
                                                int *out_width, int *out_height,
                                                int *out_bearingX, int *out_bearingY,
                                                int *out_advance) {
    // Look up in cache
    GlyphCacheEntry *entry = data->glyphCache;
    while (entry) {
        if (entry->fontId == fontId && entry->fontSize == fontSize && entry->codepoint == codepoint) {
            *out_width = entry->width;
            *out_height = entry->height;
            *out_bearingX = entry->bearingX;
            *out_bearingY = entry->bearingY;
            *out_advance = entry->advance;
            return entry->texture;
        }
        entry = entry->next;
    }

    // Not found – create new glyph bitmap and texture
    if (fontId >= data->numFonts) return NULL;
    stbtt_fontinfo *fontInfo = &data->fontInfos[fontId];
    float scale = stbtt_ScaleForPixelHeight(fontInfo, (float)fontSize);
    int advance, bearingX, bearingY, x0, y0, x1, y1;
    stbtt_GetCodepointHMetrics(fontInfo, codepoint, &advance, &bearingX);
    stbtt_GetCodepointBitmapBox(fontInfo, codepoint, scale, scale, &x0, &y0, &x1, &y1);
    int w = x1 - x0;
    int h = y1 - y0;
    if (w <= 0 || h <= 0) return NULL;

    unsigned char *bitmap = (unsigned char*)malloc(w * h);
    if (!bitmap) return NULL;
    stbtt_MakeCodepointBitmap(fontInfo, bitmap, w, h, w, scale, scale, codepoint);

    // Convert to SDL_Surface (RGBA)
    SDL_Surface *surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        free(bitmap);
        return NULL;
    }
    SDL_LockSurface(surface);
    Uint32 *pixels = (Uint32*)surface->pixels;
    for (int i = 0; i < w * h; i++) {
        Uint8 alpha = bitmap[i];
        pixels[i] = (alpha << 24) | (0xFFFFFF); // white with alpha from bitmap
    }
    SDL_UnlockSurface(surface);
    free(bitmap);

    SDL_Texture *texture = SDL_CreateTextureFromSurface(data->renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) return NULL;

    // Store in cache
    GlyphCacheEntry *newEntry = (GlyphCacheEntry*)malloc(sizeof(GlyphCacheEntry));
    newEntry->fontId = fontId;
    newEntry->fontSize = fontSize;
    newEntry->codepoint = codepoint;
    newEntry->texture = texture;
    newEntry->width = w;
    newEntry->height = h;
    newEntry->bearingX = x0;
    newEntry->bearingY = y0;
    newEntry->advance = (int)(advance * scale);
    newEntry->next = data->glyphCache;
    data->glyphCache = newEntry;

    *out_width = w;
    *out_height = h;
    *out_bearingX = x0;
    *out_bearingY = y0;
    *out_advance = newEntry->advance;
    return texture;
}

// ------------------------------------------------------------
// Main rendering loop (replaces SDL_ttf calls with stb_truetype)
// ------------------------------------------------------------
static void SDL_Clay_RenderClayCommands(Clay_SDL3RendererData *rendererData,
                                        Clay_RenderCommandArray *rcommands) {
    for (size_t i = 0; i < rcommands->length; i++) {
        Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(rcommands, i);
        const Clay_BoundingBox bounding_box = rcmd->boundingBox;
        const SDL_FRect rect = {
            (int)bounding_box.x,
            (int)bounding_box.y,
            (int)bounding_box.width,
            (int)bounding_box.height
        };

        switch (rcmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;
                SDL_SetRenderDrawBlendMode(rendererData->renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(rendererData->renderer,
                                       config->backgroundColor.r,
                                       config->backgroundColor.g,
                                       config->backgroundColor.b,
                                       config->backgroundColor.a);
                if (config->cornerRadius.topLeft > 0) {
                    SDL_Clay_RenderFillRoundedRect(rendererData, rect,
                                                   config->cornerRadius.topLeft,
                                                   config->backgroundColor);
                } else {
                    SDL_RenderFillRect(rendererData->renderer, &rect);
                }
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_TextRenderData *config = &rcmd->renderData.text;
                SDL_Clay_RenderText(rendererData, rect, config);
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                Clay_BorderRenderData *config = &rcmd->renderData.border;
                const float minRadius = SDL_min(rect.w, rect.h) / 2.0f;
                const Clay_CornerRadius clampedRadii = {
                    .topLeft = SDL_min(config->cornerRadius.topLeft, minRadius),
                    .topRight = SDL_min(config->cornerRadius.topRight, minRadius),
                    .bottomLeft = SDL_min(config->cornerRadius.bottomLeft, minRadius),
                    .bottomRight = SDL_min(config->cornerRadius.bottomRight, minRadius)
                };
                SDL_SetRenderDrawColor(rendererData->renderer,
                                       config->color.r,
                                       config->color.g,
                                       config->color.b,
                                       config->color.a);
                // left edge
                if (config->width.left > 0) {
                    SDL_FRect line = {
                        rect.x - 1,
                        rect.y + clampedRadii.topLeft,
                        config->width.left,
                        rect.h - clampedRadii.topLeft - clampedRadii.bottomLeft
                    };
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                // right edge
                if (config->width.right > 0) {
                    SDL_FRect line = {
                        rect.x + rect.w - config->width.right + 1,
                        rect.y + clampedRadii.topRight,
                        config->width.right,
                        rect.h - clampedRadii.topRight - clampedRadii.bottomRight
                    };
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                // top edge
                if (config->width.top > 0) {
                    SDL_FRect line = {
                        rect.x + clampedRadii.topLeft,
                        rect.y - 1,
                        rect.w - clampedRadii.topLeft - clampedRadii.topRight,
                        config->width.top
                    };
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                // bottom edge
                if (config->width.bottom > 0) {
                    SDL_FRect line = {
                        rect.x + clampedRadii.bottomLeft,
                        rect.y + rect.h - config->width.bottom + 1,
                        rect.w - clampedRadii.bottomLeft - clampedRadii.bottomRight,
                        config->width.bottom
                    };
                    SDL_RenderFillRect(rendererData->renderer, &line);
                }
                // corners
                if (config->cornerRadius.topLeft > 0) {
                    SDL_Clay_RenderArc(rendererData,
                                       (SDL_FPoint){ rect.x + clampedRadii.topLeft - 1,
                                                     rect.y + clampedRadii.topLeft - 1 },
                                       clampedRadii.topLeft, 180.0f, 270.0f,
                                       config->width.top, config->color);
                }
                if (config->cornerRadius.topRight > 0) {
                    SDL_Clay_RenderArc(rendererData,
                                       (SDL_FPoint){ rect.x + rect.w - clampedRadii.topRight,
                                                     rect.y + clampedRadii.topRight - 1 },
                                       clampedRadii.topRight, 270.0f, 360.0f,
                                       config->width.top, config->color);
                }
                if (config->cornerRadius.bottomLeft > 0) {
                    SDL_Clay_RenderArc(rendererData,
                                       (SDL_FPoint){ rect.x + clampedRadii.bottomLeft - 1,
                                                     rect.y + rect.h - clampedRadii.bottomLeft },
                                       clampedRadii.bottomLeft, 90.0f, 180.0f,
                                       config->width.bottom, config->color);
                }
                if (config->cornerRadius.bottomRight > 0) {
                    SDL_Clay_RenderArc(rendererData,
                                       (SDL_FPoint){ rect.x + rect.w - clampedRadii.bottomRight,
                                                     rect.y + rect.h - clampedRadii.bottomRight },
                                       clampedRadii.bottomRight, 0.0f, 90.0f,
                                       config->width.bottom, config->color);
                }
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                currentClippingRectangle = (SDL_Rect){
                    .x = (int)bounding_box.x,
                    .y = (int)bounding_box.y,
                    .w = (int)bounding_box.width,
                    .h = (int)bounding_box.height
                };
                SDL_SetRenderClipRect(rendererData->renderer, &currentClippingRectangle);
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                SDL_SetRenderClipRect(rendererData->renderer, NULL);
                break;
            }

            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                SDL_Texture *texture = (SDL_Texture*)rcmd->renderData.image.imageData;
                SDL_FRect dest = { rect.x, rect.y, rect.w, rect.h };
                SDL_RenderTexture(rendererData->renderer, texture, NULL, &dest);
                break;
            }

            default:
                SDL_Log("Unknown render command type: %d", rcmd->commandType);
        }
    }
}
