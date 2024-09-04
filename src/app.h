#include <SDL3/SDL.h>

#pragma once

typedef struct
{
    SDL_Window *window;
    SDL_GPUDevice *gpu_device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer *geometry_buffer;
    SDL_GPUTexture *texture;
    SDL_GPUSampler *sampler;
} app_state_t;

typedef struct
{
    float x;
    float y;
} vec2_t;

typedef struct
{
    vec2_t pos;
    vec2_t tex;
} vertex_t;
