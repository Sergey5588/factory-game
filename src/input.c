#include "input.h"  
  
// Keybind table: action → SDL scancode  
static SDL_Scancode keybinds[ACTION_COUNT] = {  
    [ACTION_TOGGLE_FULLSCREEN] = SDL_SCANCODE_F11,  
    [ACTION_PAN]               = SDL_SCANCODE_UNKNOWN, // mouse button, handled separately  
    [ACTION_ZOOM_IN]           = SDL_SCANCODE_UNKNOWN,  
    [ACTION_ZOOM_OUT]          = SDL_SCANCODE_UNKNOWN,  
};  
  
void Input_BeginFrame(InputState *input) {  
    // Clear per-frame states  
    for (int i = 0; i < ACTION_COUNT; i++) {  
        input->actions[i].pressed  = false;  
        input->actions[i].released = false;  
    }  
    input->mouse_dx = 0;  
    input->mouse_dy = 0;  
    input->scroll_y = 0;  
}  
  
void Input_HandleEvent(InputState *input, const SDL_Event *event) {  
    switch (event->type) {  
        case SDL_EVENT_KEY_DOWN:  
            if (event->key.repeat) break;  
            for (int i = 0; i < ACTION_COUNT; i++) {  
                if (keybinds[i] == event->key.scancode) {  
                    input->actions[i].pressed = true;  
                    input->actions[i].held    = true;  
                }  
            }  
            break;  
        case SDL_EVENT_KEY_UP:  
            for (int i = 0; i < ACTION_COUNT; i++) {  
                if (keybinds[i] == event->key.scancode) {  
                    input->actions[i].held     = false;  
                    input->actions[i].released = true;  
                }  
            }  
            break;  
        case SDL_EVENT_MOUSE_BUTTON_DOWN:  
            if (event->button.button == SDL_BUTTON_LEFT) {  
                input->actions[ACTION_PAN].pressed = true;  
                input->actions[ACTION_PAN].held    = true;  
            }  
            break;  
        case SDL_EVENT_MOUSE_BUTTON_UP:  
            if (event->button.button == SDL_BUTTON_LEFT) {  
                input->actions[ACTION_PAN].held     = false;  
                input->actions[ACTION_PAN].released = true;  
            }  
            break;  
        case SDL_EVENT_MOUSE_MOTION:  
            input->mouse_dx += event->motion.xrel;  
            input->mouse_dy += event->motion.yrel;  
            break;  
        case SDL_EVENT_MOUSE_WHEEL:  
            input->scroll_y += event->wheel.y;  
            break;  
        default:  
            break;  
    }  
}
