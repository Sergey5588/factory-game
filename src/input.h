#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL.h>  
#include <stdbool.h>
typedef enum {  
    ACTION_TOGGLE_FULLSCREEN,  
    ACTION_PAN,          // held while dragging  
    ACTION_ZOOM_IN,  
    ACTION_ZOOM_OUT,  
    ACTION_COUNT  
} GameAction;  
  
typedef struct {  
    bool pressed;   // true only on the frame it was first pressed  
    bool held;      // true while key is down  
    bool released;  // true only on the frame it was released  
} ActionState;  
  
typedef struct {  
    ActionState actions[ACTION_COUNT];  
    // Raw mouse delta this frame (for panning)  
    float mouse_dx, mouse_dy;  
    float scroll_y;  
} InputState;

void Input_BeginFrame(InputState *input);
void Input_HandleEvent(InputState *input, const SDL_Event *event);
#endif
