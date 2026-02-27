#ifndef GAME_LIB
#define GAME_LIB

#include "SDL3/SDL_init.h"
#include <stdbool.h>

SDL_AppResult Core_SDL_AppInit(void **appstate, int argc, char **argv, bool reloaded);
SDL_AppResult Core_SDL_AppIterate(void *appstate, bool reloaded);
SDL_AppResult Core_SDL_AppEvent(void *appstate, SDL_Event *event, bool reloaded);
void Core_SDL_AppQuit(void *appstate, SDL_AppResult result, bool reloaded);

#endif
