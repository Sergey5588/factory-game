#include "components.h"
#include "app.h"

void RenderSprite(ecs_iter_t *it) {
	AppState *state =(AppState*)ecs_get_ctx(it->world);
	Sprite *s = ecs_field(it, Sprite, 0);
	Position *p = ecs_field(it, Position, 1);
	for(int i= 0; i < it->count; i++) {
		SDL_RenderTexture(
			state->renderer, 
			s[i].texture, 
			NULL, 
			&(SDL_FRect){
				(p[i].x - s[i].rect.w/2)/state->cam.zoom+state->window_w/2.0f+state->cam.x,
				(p[i].y - s[i].rect.h/2 )/state->cam.zoom + state->window_h/2.0f+state->cam.y,
				s[i].rect.w/state->cam.zoom,
				s[i].rect.h/state->cam.zoom
			}
		);

	}
}


