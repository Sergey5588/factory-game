#ifndef APP_H
#define APP_H
#include <flecs.h>
#include <SDL3/SDL.h>


typedef struct camera {
	float x,y; // pos
	float zoom;
} Camera;
typedef struct app_state {
    SDL_Window *window;
	int window_w, window_h;
	SDL_Renderer *renderer;
    SDL_Texture *sampleImage;
	ecs_world_t *ecs;
	Camera cam;
} AppState;
#endif
