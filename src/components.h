#include <flecs.h>
#include <SDL3/SDL.h>

typedef struct {
	float x,y;
} Position, Velocity;


typedef struct {
	SDL_Texture *texture;
	SDL_FRect rect;
} Sprite;
