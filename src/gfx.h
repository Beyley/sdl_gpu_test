#include <SDL3/SDL.h>

#include "app.h"
#include "stb_image.h"
#include "embed.h"
#include <SDL_gpu_shadercross.h>

bool gfx_init(app_state_t *app_state);

void gfx_deinit(app_state_t *app_state);

SDL_GPUGraphicsPipeline *gfx_create_graphics_pipeline(app_state_t *app_state);

SDL_GPUBuffer *gfx_create_geometry_buffer(app_state_t *app_state);

bool gfx_create_texture_sampler(app_state_t *app_state);

bool gfx_render(app_state_t *app_state);
