#ifndef APP_H
#define APP_H
#include <flecs.h>
#include <SDL3/SDL.h>
typedef struct app_state {
    SDL_Window *window;
	SDL_Renderer *renderer;
    SDL_Texture *sampleImage;
	ecs_world_t *ecs;
} AppState;
#endif
