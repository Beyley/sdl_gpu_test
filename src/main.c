#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include <SDL3/SDL.h>

#include "app.h"
#include "gfx.h"

SDL_AppResult SDL_AppInit(void **app_state_ptr, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Initializing app...");

    app_state_t *app_state = (*app_state_ptr) = SDL_malloc(sizeof(app_state_t)); // alloc the app state
    *app_state = (app_state_t){};                                                // init the app state to 0

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL, error %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetAppMetadata("SDL GPU Test", NULL, "moe.beyleyisnot.sdl_gpu_test"))
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to set app metadata, error %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    app_state->window = SDL_CreateWindow(
        SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING),
        1920,
        1080,
        SDL_WINDOW_RESIZABLE);

    if (app_state->window == NULL)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window, error %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!gfx_init(app_state))
        return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *app_state_ptr)
{
    app_state_t *app_state = (app_state_t *)app_state_ptr;

    if (!gfx_render(app_state))
        return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *app_state_ptr, SDL_Event *event)
{
    app_state_t *app_state = (app_state_t *)app_state_ptr;
    (void)app_state;

    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *app_state_ptr)
{
    app_state_t *app_state = (app_state_t *)app_state_ptr;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Closing app...");

    gfx_deinit(app_state);

    SDL_Quit();

    SDL_free(app_state); // Free the app state pointer

    if (SDL_GetNumAllocations() > 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%d memory allocations leaked!", SDL_GetNumAllocations());
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "App closed...");
}
