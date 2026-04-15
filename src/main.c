
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#define CLAY_IMPLEMENTATION
#include "clay.h"

#include <flecs.h>
#undef ECS_ALIGNOF  
#define ECS_ALIGNOF(T) ((int64_t)_Alignof(T))
#include <stdio.h>
#include "clay_renderer_SDL3.h"
// ----------------------------------------------------------------------------
// Constants and types
// ----------------------------------------------------------------------------
static const Uint32 FONT_ID = 0;
static const Clay_Color COLOR_ORANGE = (Clay_Color){225, 138, 50, 255};
static const Clay_Color COLOR_BLUE   = (Clay_Color){111, 173, 162, 255};
static const Clay_Color COLOR_LIGHT  = (Clay_Color){224, 215, 210, 255};

#include "app.h"
#include "components.h"
// ----------------------------------------------------------------------------
// STB image loading 
// ----------------------------------------------------------------------------
SDL_Texture* STB_LoadTexture(SDL_Renderer* renderer, const char* filename, int* w, int* h) {
    int width, height, channels;
    unsigned char* pixels = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        fprintf(stderr, "STB Error: %s [%s]\n", stbi_failure_reason(), filename);
        return NULL;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                             SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture) {
        SDL_UpdateTexture(texture, NULL, pixels, width * 4);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        if (w) *w = width;
        if (h) *h = height;
    } else {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
    }
    stbi_image_free(pixels);
    return texture;
}



SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    (void)argc; (void)argv;
	
    AppState *state = (AppState*)SDL_calloc(1, sizeof(AppState));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;
	state->cam.zoom = 1.0f;
    // Create window and renderer
    if (!SDL_CreateWindowAndRenderer("Factory game", 640, 480, 0,
                                     &state->window, &state->renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowResizable(state->window, true);

    // Load sample image
    state->sampleImage = STB_LoadTexture(state->renderer, "resources/cat.png", NULL, NULL);
    if (!state->sampleImage) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load image");
        return SDL_APP_FAILURE;
    }

	state->ecs = ecs_init();  
	if(!state->ecs) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "ecs_init() returned NULL");
		return SDL_APP_FAILURE;
	}
	ecs_world_t *ecs = state->ecs;
	ecs_set_ctx(ecs, state,NULL);
	ECS_COMPONENT(ecs, Sprite);
	ECS_COMPONENT(ecs, Position);
	ECS_SYSTEM(ecs, RenderSprite, EcsOnUpdate, Sprite, Position);
	ecs_entity_t e = ecs_insert(ecs,
		ecs_value(Sprite, {state->sampleImage, (SDL_FRect){0,0,100,100}}),
		ecs_value(Position, {-100,0})
	);
	ecs_entity_t e2 = ecs_insert(ecs,
		ecs_value(Sprite, {state->sampleImage, (SDL_FRect){0,0,100,100}}),
		ecs_value(Position, {0,0})
	);

    SDL_GetWindowSize(state->window, &state->window_w, &state->window_h);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *state = (AppState*)appstate;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
		case SDL_EVENT_WINDOW_RESIZED:
			SDL_GetWindowSize(state->window, &state->window_w, &state->window_h);
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			state->cam.zoom-=event->wheel.y/10.0f;
			break;
		case SDL_EVENT_MOUSE_MOTION:
			if (event->motion.state & SDL_BUTTON_LMASK) {
				state->cam.x+= event->motion.xrel;
				state->cam.y+= event->motion.yrel;
			}
			break;
        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *state = (AppState*)appstate;


    // Clear screen
    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);

	ecs_progress(state->ecs, 0);
    SDL_RenderPresent(state->renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    (void)result;
    AppState *state = (AppState*)appstate;

    if (state) {
        // Destroy image texture
        if (state->sampleImage) SDL_DestroyTexture(state->sampleImage);
        // Destroy renderer and window
        if (state->renderer) SDL_DestroyRenderer(state->renderer);
        if (state->window) SDL_DestroyWindow(state->window);
		if (state->ecs) ecs_fini(state->ecs);
        SDL_free(state);
    }
}
