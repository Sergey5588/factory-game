#include "components.h"
#include "app.h"

void RenderSprite(ecs_iter_t *it) {
	AppState *state =(AppState*)ecs_get_ctx(it->world);
	Sprite *s = ecs_field(it, Sprite, 0);
	Position *p = ecs_field(it, Position, 1);
	for(int i= 0; i < it->count; i++) {
		SDL_RenderTexture(state->renderer, s->texture, NULL, &(SDL_FRect){p->x-s->rect.w/2, p->y-s->rect.h/2, s->rect.w, s->rect.h});

	}
}


