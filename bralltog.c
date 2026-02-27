#include "SDL3/SDL_main.h"

#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>

#define DEV_MODE

#ifdef DEV_MODE
#include <stdio.h>
#include <stdbool.h>
#include "SDL3/SDL_timer.h"

#define RELOAD_CHECK_INTERVAL_MS 500

typedef enum {
    RELOAD_FAILED,
    RELOAD_NONE,
    RELOAD_SUCCESS
} ReloadResult;

const char* const APP_INIT_FUNC_NAME = "Core_SDL_AppInit";
const char* const APP_ITERATE_FUNC_NAME = "Core_SDL_AppIterate";
const char* const APP_EVENT_FUNC_NAME = "Core_SDL_AppEvent";
const char* const APP_QUIT_FUNC_NAME = "Core_SDL_AppQuit";
const char* const LIB_PATH = "./libgame.so";
const char* const ASSETS_PATH = "./assets";
void* handle = NULL;

SDL_AppResult (*Core_SDL_AppInit_ptr)(void **, int, char **, bool);
SDL_AppResult (*Core_SDL_AppIterate_ptr)(void *, bool);
SDL_AppResult (*Core_SDL_AppEvent_ptr)(void *, SDL_Event *, bool);
void (*Core_SDL_AppQuit_ptr)(void *, SDL_AppResult, bool);

#else

#include "game.h"
#include <stdbool.h>

SDL_AppResult (*Core_SDL_AppInit_ptr)(void **, int, char **, bool);
SDL_AppResult (*Core_SDL_AppIterate_ptr)(void *, bool);
SDL_AppResult (*Core_SDL_AppEvent_ptr)(void *, SDL_Event *, bool);
void (*Core_SDL_AppQuit_ptr)(void *, SDL_AppResult, bool);

#endif

#ifdef DEV_MODE
time_t get_mtime(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 ? st.st_mtime : -1;
}

int reload_all_symbols(void* h) {
    Core_SDL_AppInit_ptr = dlsym(h, APP_INIT_FUNC_NAME);
    Core_SDL_AppIterate_ptr = dlsym(h, APP_ITERATE_FUNC_NAME);
    Core_SDL_AppEvent_ptr = dlsym(h, APP_EVENT_FUNC_NAME);
    Core_SDL_AppQuit_ptr = dlsym(h, APP_QUIT_FUNC_NAME);

    if (!Core_SDL_AppInit_ptr || !Core_SDL_AppIterate_ptr ||
        !Core_SDL_AppEvent_ptr || !Core_SDL_AppQuit_ptr) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        return 0;
    }
    return 1;
}

ReloadResult reload_library_if_modified() {
    static time_t last_lib_mtime = 0;
    static time_t last_assets_mtime = 0;
    static Uint64 last_check_time = 0;
    static int load_counter = 0;

    Uint64 now = SDL_GetTicks();
    if (handle && now - last_check_time < RELOAD_CHECK_INTERVAL_MS) {
        return RELOAD_NONE;  // skip check, not enough time passed
    }
    last_check_time = now;

    time_t lib_mtime = get_mtime(LIB_PATH);
    time_t assets_mtime = get_mtime(ASSETS_PATH);

    bool lib_changed = lib_mtime > last_lib_mtime;
    bool assets_changed = assets_mtime > last_assets_mtime;

    if (!lib_changed && !assets_changed) {
        return RELOAD_NONE;
    }

    // Update assets mtime if changed
    if (assets_changed) {
        fprintf(stdout, "Assets changed, triggering reload...\n");
        last_assets_mtime = assets_mtime;
    }

    // Reload library if changed
    if (lib_changed) {
        // Copy library to temp file to avoid dlopen caching
        char temp_path[256];
        snprintf(temp_path, sizeof(temp_path), "/tmp/libgame_%d.so", load_counter++);
        
        FILE* src = fopen(LIB_PATH, "rb");
        FILE* dst = fopen(temp_path, "wb");
        if (!src || !dst) {
            fprintf(stderr, "Failed to copy library\n");
            if (src) fclose(src);
            if (dst) fclose(dst);
            return handle ? RELOAD_NONE : RELOAD_FAILED;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
            fwrite(buf, 1, n, dst);
        }
        fclose(src);
        fclose(dst);

        void* new_handle = dlopen(temp_path, RTLD_NOW);
        if (!new_handle) {
            fprintf(stderr, "dlopen: %s\n", dlerror());
            return handle ? RELOAD_NONE : RELOAD_FAILED;
        }
        fprintf(stdout, "Reloading library...\n");
        last_lib_mtime = lib_mtime;
        handle = new_handle;  // old handle intentionally leaked
        if (!reload_all_symbols(handle)) {
            return RELOAD_FAILED;
        }
    }

    return RELOAD_SUCCESS;
}
#endif


SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
#ifdef DEV_MODE
    ReloadResult r = reload_library_if_modified();
    if (r == RELOAD_FAILED) return SDL_APP_FAILURE;
    return Core_SDL_AppInit_ptr(appstate, argc, argv, r == RELOAD_SUCCESS);
#else
    Core_SDL_AppInit_ptr = Core_SDL_AppInit;
    return Core_SDL_AppInit_ptr(appstate, argc, argv, false);
#endif
}

SDL_AppResult SDL_AppIterate(void *appstate) {
#ifdef DEV_MODE
    ReloadResult r = reload_library_if_modified();
    if (r == RELOAD_FAILED) return SDL_APP_FAILURE;
    return Core_SDL_AppIterate_ptr(appstate, r == RELOAD_SUCCESS);
#else
    Core_SDL_AppIterate_ptr = Core_SDL_AppIterate;
    return Core_SDL_AppIterate_ptr(appstate, false);
#endif
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
#ifdef DEV_MODE
    ReloadResult r = reload_library_if_modified();
    if (r == RELOAD_FAILED) return SDL_APP_FAILURE;
    return Core_SDL_AppEvent_ptr(appstate, event, r == RELOAD_SUCCESS);
#else
    Core_SDL_AppEvent_ptr = Core_SDL_AppEvent;
    return Core_SDL_AppEvent_ptr(appstate, event, false);
#endif
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
#ifdef DEV_MODE
    ReloadResult r = reload_library_if_modified();
    if (r == RELOAD_FAILED) return;
    Core_SDL_AppQuit_ptr(appstate, result, r == RELOAD_SUCCESS);
#else
    Core_SDL_AppQuit_ptr = Core_SDL_AppQuit;
    Core_SDL_AppQuit_ptr(appstate, result, false);
#endif
}
