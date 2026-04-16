
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#define CLAY_IMPLEMENTATION
#include <clay.h>

#include <flecs.h>
#undef ECS_ALIGNOF  
#define ECS_ALIGNOF(T) ((int64_t)_Alignof(T))
#include <stdio.h>
// ----------------------------------------------------------------------------
// Constants and types
// ----------------------------------------------------------------------------
static const Uint32 FONT_ID = 0;
static const Clay_Color COLOR_ORANGE = (Clay_Color){225, 138, 50, 255};
static const Clay_Color COLOR_BLUE   = (Clay_Color){111, 173, 162, 255};
static const Clay_Color COLOR_WHITE  = (Clay_Color){255,255,255,255};


static const unsigned char s_font_roboto[] = {  
#embed "static/RobotoMono.ttf"  
};
#define CLAY_SDL3_IMPLEMENTATION
#include "app.h"
#include "components.h"


static inline Clay_String Clay__FormatString(char *buf, int size, const char *fmt, ...) {  
    va_list args;  
    va_start(args, fmt);  
    int len = vsnprintf(buf, size, fmt, args);  
    va_end(args);  
    return (Clay_String){ .isStaticallyAllocated = false, .length = len, .chars = buf };  
}  
  
#define CLAY_FSTRING(size, fmt, ...) \
    Clay__FormatString((char[size]){0}, size, fmt, ##__VA_ARGS__)
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
    if (!SDL_CreateWindowAndRenderer("Factory game", 640, 480, SDL_WINDOW_FULLSCREEN,
                                     &state->window, &state->renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowResizable(state->window, true);
	if (!SDL_SetRenderVSync(state->renderer, 1)) {
		SDL_Log("Failed to enable VSync: %s", SDL_GetError());
	}
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
	    static Clay_STB_Font clay_fonts[] = {  
        { s_font_roboto, sizeof(s_font_roboto) },   // fontId = 0  
    };  
    state->clayRenderer = (Clay_SDL3RendererData){  
        .renderer   = state->renderer,  
        .fonts      = clay_fonts,  
        .num_fonts  = 1,  
        .num_atlases = 0,  
    };  
  
    uint64_t clay_mem_size = Clay_MinMemorySize();  
    state->clayMemory = SDL_malloc(clay_mem_size);  
    Clay_Arena clay_arena = Clay_CreateArenaWithCapacityAndMemory(  
        clay_mem_size, state->clayMemory);  
    Clay_Initialize(clay_arena,  
        (Clay_Dimensions){ state->window_w, state->window_h },  
        (Clay_ErrorHandler){ NULL });  
    Clay_SetMeasureTextFunction(Clay_STB_MeasureText, &state->clayRenderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *state = (AppState*)appstate;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
		case SDL_EVENT_WINDOW_EXPOSED:
			SDL_AppIterate(state);
			break;
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			state->window_w = event->window.data1;
			state->window_h = event->window.data2;
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			if(state->cam.zoom > 0.09)
				state->cam.zoom-=event->wheel.y/10.0f; // 0.1 or -0.1
			if(state->cam.zoom <= 0.1) {
				state->cam.zoom = 0.1;
			}
			break;
		case SDL_EVENT_MOUSE_MOTION:
			if (event->motion.state & SDL_BUTTON_LMASK) {
				state->cam.x+= event->motion.xrel;
				state->cam.y+= event->motion.yrel;
			}
			break;
		case SDL_EVENT_KEY_DOWN:  
			if (event->key.key == SDLK_F11) {  
				bool is_fullscreen = (SDL_GetWindowFlags(state->window) & SDL_WINDOW_FULLSCREEN) != 0;  
				SDL_SetWindowFullscreen(state->window, !is_fullscreen);  
			}  
			break;
        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *state = (AppState*)appstate;
	//clay UI
	Clay_SetLayoutDimensions((Clay_Dimensions){ state->window_w, state->window_h });  
    Clay_BeginLayout();  
	CLAY(CLAY_ID("TopBar"), {  
		.layout = {  
			.sizing  = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },  
			.padding = CLAY_PADDING_ALL(8),  
		},  
		.backgroundColor = {0},  
	})
	 {  
        CLAY_TEXT(  
            CLAY_FSTRING(64,"resolution: %d %d", state->window_w, state->window_h),  
            CLAY_TEXT_CONFIG({  
                .fontId    = FONT_ID,  
                .fontSize  = 24,  
                .textColor = COLOR_WHITE,  
            })  
        );  
    }  
    // Clear screen
    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);

	ecs_progress(state->ecs, 0);
	Clay_RenderCommandArray clay_cmds = Clay_EndLayout(0);  
    SDL_Clay_RenderClayCommands(&state->clayRenderer, &clay_cmds);
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
		if (state->clayMemory) SDL_free(state->clayMemory);
        SDL_free(state);
    }
}
