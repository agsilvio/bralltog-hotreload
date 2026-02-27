#include "game.h"

#include "SDL3_image/SDL_image.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_render.h"
#include "SDL3_mixer/SDL_mixer.h"
#include "SDL3_ttf/SDL_ttf.h" 
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_log.h"
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    Uint32 lastTime;
    int x, y;
    SDL_Window * window;
    SDL_Renderer * renderer;
    SDL_Texture * image;
    TTF_Font * font;
    MIX_Mixer * mixer;
    MIX_Track * sound;
    MIX_Track * music;
    bool musicMuted;
} GameContext;

#define VIEW_WIDTH 1000
#define VIEW_HEIGHT 600
#define DESIRED_FPS 60

SDL_Texture * loadTexture(char * path, SDL_Renderer * renderer) {
    SDL_Texture * newTexture = IMG_LoadTexture(renderer, path);
    if (!newTexture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load image at '%s' could not be loaded! SDL Error: %s", path, SDL_GetError());
        return NULL;
    }

    return newTexture;
}

MIX_Track * loadTrack(MIX_Mixer * mixer, char * path) {
    MIX_Audio * sound = MIX_LoadAudio(mixer, path, true);
    if (!sound) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load sound at '%s'. SDL Error: %s", path, SDL_GetError());
        return NULL;
    }

    MIX_Track * track = MIX_CreateTrack(mixer);
    MIX_SetTrackAudio(track, sound);
    MIX_DestroyAudio(sound);

    return track;
}

void rateLimitFps(Uint32 lastTime) {
    Uint32 frameTime = 1000 / DESIRED_FPS;
    Uint32 delay = frameTime - (SDL_GetTicks() - lastTime);
    if (delay > 0) {
        SDL_Delay(delay);
    }
}

void freeAllAssets(GameContext * ctx) {
    if (ctx->font) {
        TTF_CloseFont(ctx->font);
        ctx->font = NULL;
    }
    if (ctx->sound) {
        MIX_DestroyTrack(ctx->sound);
        ctx->sound = NULL;
    }
    if (ctx->music) {
        MIX_DestroyTrack(ctx->music);
        ctx->music = NULL;
    }
    if (ctx->image) {
        SDL_DestroyTexture(ctx->image);
        ctx->image = NULL;
    }
}

SDL_AppResult loadAllAssets(GameContext * ctx, SDL_Renderer * renderer) {
    //load image
    ctx->image = loadTexture("assets/image.png", renderer);
    if (!ctx->image) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load image. Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    //load sound
    ctx->sound = loadTrack(ctx->mixer, "assets/sound.wav");
    if (!ctx->sound) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load sound. Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    //load music
    ctx->music = loadTrack(ctx->mixer, "assets/music.wav");
    if (!ctx->music) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load music. Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }


    ctx->font = TTF_OpenFont("assets/font.ttf", 48);
    if (!ctx->font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Font could not be loaded! Error: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

// Handles asset reload, ensuring it only happens once per reload event
SDL_AppResult reloadAssetsIfNeeded(GameContext *ctx, bool reloaded) {
    static bool last_reload_handled = false;
    
    if (!reloaded) {
        last_reload_handled = false;
        return SDL_APP_CONTINUE;
    }
    
    if (last_reload_handled) {
        return SDL_APP_CONTINUE;
    }
    last_reload_handled = true;
    
    SDL_Log("Hot reload: reloading assets...");
    freeAllAssets(ctx);
    return loadAllAssets(ctx, ctx->renderer);
}

SDL_AppResult Core_SDL_AppInit(void **appstate, int argc, char **argv, bool reloaded)
{
    //this method of initialization is preferred for a couple of reasons:
    //1. I think having everything that's not a constant in GameContext may work better 
    //on systems like Android that really control the behaviour of your app.
    //2. It allows for more concise initialization of the GameContext. 
    //This is paired with the member-wise copy at the end of this function.
    GameContext * newAppState = (GameContext *)malloc(sizeof(GameContext));
    GameContext ctx = {
        .x = 100,
        .y = 100,
	.musicMuted = true
    };

    int result = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD);
    if(result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_InitSubSystem failed with code %d. Error: %s", result, SDL_GetError());
        return SDL_APP_FAILURE;
    }

    //gamepad input
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    SDL_Gamepad* gamepad = NULL;

    // Iterate over the list of gamepads
    for(int i = 0; i < count; i++) {
	    SDL_Gamepad* gamepd = SDL_OpenGamepad(ids[i]);
	    if(gamepad == NULL) {
		    gamepad = gamepd;
	    }

	    // Close the other gamepads
	    if(i > 0) {
		    SDL_CloseGamepad(gamepd);
	    }
    }
    if (!gamepad) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open gamepad. Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }


    //open window and renderer
    SDL_Window *window;
    SDL_Renderer *renderer;
    if (!SDL_CreateWindowAndRenderer("bralltog", VIEW_WIDTH, VIEW_HEIGHT, SDL_WINDOW_BORDERLESS, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer. Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    ctx.window = window;
    ctx.renderer = renderer;

    //initialize sound
    MIX_Init();
    ctx.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!ctx.mixer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL Mixer could not be initialized! Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load font. Error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }


    SDL_AppResult loadAssetsResult = loadAllAssets(&ctx, renderer);

    //playMusic (muted on init)
    MIX_SetTrackGain(ctx.sound, 1.0f);
    MIX_SetTrackGain(ctx.music, 0.0f);
    MIX_PlayTrack(ctx.music, 0);


    *newAppState = ctx;
    *appstate = newAppState;
    return SDL_APP_CONTINUE;
}

void playSound(GameContext *appstate) {
    GameContext * ctx = (GameContext *)appstate;
    MIX_PlayTrack(ctx->sound, 0);
}

void toggleMusic(GameContext *appstate) {
    GameContext * ctx = (GameContext *)appstate;
    if (ctx->musicMuted) {
        MIX_SetTrackGain(ctx->music, 0.1f);
	ctx->musicMuted = false;
    } else {
        MIX_SetTrackGain(ctx->music, 0.0f);
	ctx->musicMuted = true;
    }
}

void handleInput(GameContext *ctx) {
    const bool* keystates = SDL_GetKeyboardState(NULL);

    SDL_Gamepad *pad = SDL_GetGamepadFromPlayerIndex(0);

    if (keystates[SDL_SCANCODE_UP] || (pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP))) {
        ctx->y -= 2;
    }
    if (keystates[SDL_SCANCODE_DOWN] || (pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))) {
        ctx->y += 2;
    }
    if (keystates[SDL_SCANCODE_LEFT] || (pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))) {
        ctx->x -= 2;
    }
    if (keystates[SDL_SCANCODE_RIGHT] || (pad && SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))) {
        ctx->x += 2;
    }
}

SDL_AppResult Core_SDL_AppIterate(void *appstate, bool reloaded) {
    GameContext * ctx = (GameContext *)appstate;
    if (!ctx) { return SDL_APP_FAILURE; }

    SDL_AppResult reloadResult = reloadAssetsIfNeeded(ctx, reloaded);
    if (reloadResult != SDL_APP_CONTINUE) { return reloadResult; }

    ctx->lastTime = SDL_GetTicks();
    rateLimitFps(ctx->lastTime);

    handleInput(ctx);

    SDL_SetRenderDrawColor(ctx->renderer, 100, 150, 50, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(ctx->renderer);

    //simple rectangle
    SDL_SetRenderDrawColor(ctx->renderer, 200, 255, 200, SDL_ALPHA_OPAQUE);
    SDL_FRect square = {ctx->x, ctx->y, 75, 75};
    SDL_RenderFillRect(ctx->renderer, &square);

    //image
    SDL_FRect imageRect = { 200, 200, 50, 150 };
    SDL_RenderTexture(ctx->renderer, ctx->image, NULL, &imageRect);

    ////text
    SDL_Color color = { 200, 255, 100 };
    SDL_Surface * textSurface1 = TTF_RenderText_Blended(ctx->font, "Press S to play a sound", 0, color);
    SDL_Texture* textTexture1 = SDL_CreateTextureFromSurface( ctx->renderer, textSurface1 );
    SDL_FRect destRect1 = { 50, 50, textSurface1->w, textSurface1->h };
    SDL_RenderTexture(ctx->renderer, textTexture1, NULL, &destRect1);

    SDL_Surface * textSurface2 = TTF_RenderText_Blended(ctx->font, "Press M to toggle music", 0, color);
    SDL_Texture* textTexture2 = SDL_CreateTextureFromSurface( ctx->renderer, textSurface2 );
    SDL_FRect destRect2 = { 50, 100, textSurface2->w, textSurface2->h };
    SDL_RenderTexture(ctx->renderer, textTexture2, NULL, &destRect2);

    SDL_Surface * textSurface3 = TTF_RenderText_Blended(ctx->font, "Press Q to quit", 0, color);
    SDL_Texture* textTexture3 = SDL_CreateTextureFromSurface( ctx->renderer, textSurface3 );
    SDL_FRect destRect3 = { 50, 150, textSurface3->w, textSurface3->h };
    SDL_RenderTexture(ctx->renderer, textTexture3, NULL, &destRect3);

    //present
    SDL_RenderPresent(ctx->renderer);

    //this is needed, here, to avoid memory leak
    //one could create these surfaces and textures in the Init event
    //if the text never changed. This example would work for 
    //changing text (though not done here).
    SDL_DestroySurface(textSurface1); 
    SDL_DestroyTexture(textTexture1);
    SDL_DestroySurface(textSurface2); 
    SDL_DestroyTexture(textTexture2);
    SDL_DestroySurface(textSurface3); 
    SDL_DestroyTexture(textTexture3);

    return SDL_APP_CONTINUE;
}

SDL_AppResult Core_SDL_AppEvent(void *appstate, SDL_Event *event, bool reloaded) {
    GameContext * ctx = (GameContext *)appstate;
    if (!ctx) { return SDL_APP_FAILURE; }

    SDL_AppResult reloadResult = reloadAssetsIfNeeded(ctx, reloaded);
    if (reloadResult != SDL_APP_CONTINUE) { return reloadResult; }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch(event->key.key) {
            case SDLK_Q:  return SDL_APP_SUCCESS; break;
            case SDLK_S:  playSound(ctx); break;
            case SDLK_M:  toggleMusic(ctx); break;
        }
    }

    return SDL_APP_CONTINUE;
}

void Core_SDL_AppQuit(void *appstate, SDL_AppResult result, bool reloaded) {
    GameContext * ctx = (GameContext *)appstate;

    if (ctx) { 
        freeAllAssets(ctx);
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
    }

    TTF_Quit();
    MIX_Quit();
    SDL_Quit();
}
