#include "gfx.h"

bool gfx_init(app_state_t *app_state)
{
    if(!SDL_ShaderCross_Init())
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL_ShaderCross, error %s", SDL_GetError());
        return false;
    }

    // Init the GPU device with the shader formats supported by shadercross's SPIRV intake
    app_state->gpu_device = SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(), true, NULL);

    if (app_state->gpu_device == NULL)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GPU device, error %s", SDL_GetError());
        return false;
    }

    switch (SDL_GetGPUDriver(app_state->gpu_device))
    {
    case SDL_GPU_DRIVER_D3D11:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Using D3D11");
        break;
    case SDL_GPU_DRIVER_D3D12:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Using D3D12");
        break;
    case SDL_GPU_DRIVER_METAL:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Using Metal");
        break;
    case SDL_GPU_DRIVER_VULKAN:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Using Vulkan");
        break;
    case SDL_GPU_DRIVER_PRIVATE:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Using Secret more sinister third thing");
        break;
    case SDL_GPU_DRIVER_INVALID:
    default:
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "invalid GPU driver %d", SDL_GetGPUDriver(app_state->gpu_device));
        break;
    }

    if (!SDL_ClaimWindowForGPUDevice(app_state->gpu_device, app_state->window))
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to claim window for GPU device, error %s", SDL_GetError());
        return false;
    }

    SDL_GPUPresentMode presentMode;
    if (SDL_WindowSupportsGPUPresentMode(app_state->gpu_device, app_state->window, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Trying to use mailbox present");
        presentMode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    else if (SDL_WindowSupportsGPUPresentMode(app_state->gpu_device, app_state->window, SDL_GPU_PRESENTMODE_IMMEDIATE))
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Trying to use immediate present");
        presentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    }
    else
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Trying to use vsync present");
        presentMode = SDL_GPU_PRESENTMODE_VSYNC;
    }

    if (SDL_SetGPUSwapchainParameters(app_state->gpu_device, app_state->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, presentMode))
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Setting present mode succeeded");
    else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to set GPU swapchain params, error %s", SDL_GetError());

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "App initialized...");

    app_state->pipeline = gfx_create_graphics_pipeline(app_state);
    if (!app_state->pipeline)
        return false;

    app_state->geometry_buffer = gfx_create_geometry_buffer(app_state);
    if (!app_state->geometry_buffer)
        return false;

    if (!gfx_create_texture_sampler(app_state))
        return false;

    return true;
}

void gfx_deinit(app_state_t *app_state)
{
    SDL_ShaderCross_Quit();
    SDL_ReleaseGPUTexture(app_state->gpu_device, app_state->texture);
    SDL_ReleaseGPUSampler(app_state->gpu_device, app_state->sampler);
    SDL_ReleaseGPUBuffer(app_state->gpu_device, app_state->geometry_buffer);
    SDL_ReleaseGPUGraphicsPipeline(app_state->gpu_device, app_state->pipeline);
    SDL_ReleaseWindowFromGPUDevice(app_state->gpu_device, app_state->window); // Release the window from the GPU device
    SDL_DestroyGPUDevice(app_state->gpu_device);                              // Destroy the GPU device
    SDL_DestroyWindow(app_state->window);                                     // Destroy the window
}

SDL_GPUGraphicsPipeline *gfx_create_graphics_pipeline(app_state_t *app_state)
{
    SDL_GPUShaderCreateInfo vert_create_info = {
        .code = zigMainVertPtr(),
        .code_size = zigMainVertSize(),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    };

    SDL_GPUShaderCreateInfo frag_create_info = {
        .code = zigMainFragPtr(),
        .code_size = zigMainFragSize(),
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
    };

    SDL_GPUShader *vert_shader = SDL_ShaderCross_CompileFromSPIRV(app_state->gpu_device, &vert_create_info, false);
    if (!vert_shader)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create vertex shader, error %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUShader *frag_shader = SDL_ShaderCross_CompileFromSPIRV(app_state->gpu_device, &frag_create_info, false);
    if (!frag_shader)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create fragment shader, error %s", SDL_GetError());
        return NULL;
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {
        .vertex_shader = vert_shader,
        .fragment_shader = frag_shader,
        .vertex_input_state = {
            .num_vertex_attributes = 2,
            .vertex_attributes =
                (SDL_GPUVertexAttribute[2]){
                    (SDL_GPUVertexAttribute){
                        .location = 0,
                        .binding_index = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                        .offset = 0,
                    },
                    (SDL_GPUVertexAttribute){
                        .location = 1,
                        .binding_index = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                        .offset = sizeof(float) * 2,
                    },
                },
            .num_vertex_bindings = 1,
            .vertex_bindings = (SDL_GPUVertexBinding[1]){
                (SDL_GPUVertexBinding){
                    .index = 0,
                    .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                    .pitch = sizeof(float) * 4,
                },
            },
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
        },
        .multisample_state = {
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .sample_mask = 0xF,
        },
        .depth_stencil_state = {
            .enable_depth_test = false,
            .enable_depth_write = false,
        },
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = &(SDL_GPUColorTargetDescription){
                .format = SDL_GetGPUSwapchainTextureFormat(app_state->gpu_device, app_state->window),
                .blend_state = {
                    .enable_blend = false,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .color_write_mask = 0xF,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
                },
            },
        },
    };

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(app_state->gpu_device, &pipeline_create_info);

    if (!pipeline)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create graphics pipeline, error %s", SDL_GetError());
        return NULL;
    }

    SDL_ReleaseGPUShader(app_state->gpu_device, vert_shader);
    SDL_ReleaseGPUShader(app_state->gpu_device, frag_shader);

    return pipeline;
}

SDL_GPUBuffer *gfx_create_geometry_buffer(app_state_t *app_state)
{
    const float size = 0.8f;
    const float shrink = 0.4f;

    vertex_t vertex_data[6] = {
        {.pos = {-size + shrink, size}, .tex = {0, 0}}, // top left
        {.pos = {-size, -size}, .tex = {0, 1}},         // bottom left
        {.pos = {size, -size}, .tex = {1, 1}},          // bottom right

        {.pos = {-size + shrink, size}, .tex = {0, 0}}, // top left
        {.pos = {size - shrink, size}, .tex = {1, 0}},  // top right
        {.pos = {size, -size}, .tex = {1, 1}},          // bottom right
    };

    SDL_GPUBufferCreateInfo vert_buf_create_info = {
        .size = sizeof(vertex_data), // three vec2
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
    };

    SDL_GPUBuffer *vert_buf = SDL_CreateGPUBuffer(app_state->gpu_device, &vert_buf_create_info);

    if (!vert_buf)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create geometry buffer, error %s", SDL_GetError());
        return NULL;
    }

    SDL_SetGPUBufferName(app_state->gpu_device, vert_buf, "Static geometry");

    SDL_GPUTransferBufferCreateInfo transfer_buf_create_info = {
        .size = sizeof(vertex_data),
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    };

    SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(app_state->gpu_device, &transfer_buf_create_info);

    if (!transfer_buffer)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create transfer buffer, error %s", SDL_GetError());
        return NULL;
    }

    auto map_ptr = SDL_MapGPUTransferBuffer(app_state->gpu_device, transfer_buffer, false);
    SDL_memcpy(map_ptr, vertex_data, sizeof(vertex_data));
    SDL_UnmapGPUTransferBuffer(app_state->gpu_device, transfer_buffer);

    auto command_buffer = SDL_AcquireGPUCommandBuffer(app_state->gpu_device);
    auto copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_UploadToGPUBuffer(copy_pass,
                          &(SDL_GPUTransferBufferLocation){
                              .offset = 0,
                              .transfer_buffer = transfer_buffer,
                          },
                          &(SDL_GPUBufferRegion){
                              .buffer = vert_buf,
                              .offset = 0,
                              .size = sizeof(vertex_data),
                          },
                          false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);

    SDL_ReleaseGPUTransferBuffer(app_state->gpu_device, transfer_buffer);

    return vert_buf;
}

bool gfx_create_texture_sampler(app_state_t *app_state)
{
    int width, height;
    stbi_uc *image = stbi_load_from_memory(zigTexturePtr(), (int)zigTextureSize(), &width, &height, NULL, 4);

    SDL_GPUTextureCreateInfo texture_create_info = {
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = (Uint32)width,
        .height = (Uint32)height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = 1,
        .type = SDL_GPU_TEXTURETYPE_2D,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    };

    app_state->texture = SDL_CreateGPUTexture(app_state->gpu_device, &texture_create_info);

    if (!app_state->texture)
    {
        stbi_image_free(image);
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GPU texture, error %s", SDL_GetError());
        return false;
    }

    auto transfer_buffer = SDL_CreateGPUTransferBuffer(app_state->gpu_device,
                                                       &(SDL_GPUTransferBufferCreateInfo){
                                                           .size = (Uint32)(width * height * 4),
                                                           .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                       });

    if (!transfer_buffer)
    {
        stbi_image_free(image);
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GPU transfer buffer, error %s", SDL_GetError());
        return false;
    }

    auto map = SDL_MapGPUTransferBuffer(app_state->gpu_device, transfer_buffer, false);
    SDL_memcpy(map, image, width * height * 4);
    SDL_UnmapGPUTransferBuffer(app_state->gpu_device, transfer_buffer);

    stbi_image_free(image);

    auto command_buffer = SDL_AcquireGPUCommandBuffer(app_state->gpu_device);
    auto copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_UploadToGPUTexture(copy_pass,
                           &(SDL_GPUTextureTransferInfo){
                               .offset = 0,
                               .rows_per_layer = (Uint32)height,
                               .transfer_buffer = transfer_buffer,
                               .pixels_per_row = (Uint32)width,
                           },
                           &(SDL_GPUTextureRegion){
                               .x = 0,
                               .y = 0,
                               .w = (Uint32)width,
                               .h = (Uint32)height,
                               .d = 1,
                               .texture = app_state->texture,
                           },
                           false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);

    SDL_ReleaseGPUTransferBuffer(app_state->gpu_device, transfer_buffer);

    app_state->sampler = SDL_CreateGPUSampler(app_state->gpu_device,
                                              &(SDL_GPUSamplerCreateInfo){
                                                  .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                  .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                  .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                  .mag_filter = SDL_GPU_FILTER_LINEAR,
                                                  .min_filter = SDL_GPU_FILTER_LINEAR,
                                                  .max_anisotropy = 1,
                                              });

    if (!app_state->sampler)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GPU sampler, error %s", SDL_GetError());
        return false;
    }

    return true;
}

bool gfx_render(app_state_t *app_state)
{
    uint32_t width, height;
    SDL_GPUCommandBuffer *cmd_buf = SDL_AcquireGPUCommandBuffer(app_state->gpu_device);

    if (cmd_buf == NULL)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    SDL_GPUTexture *swapchain_texture = SDL_AcquireGPUSwapchainTexture(cmd_buf, app_state->window, &width, &height);

    // If we have a swapchain texture, run our rendering code
    if (swapchain_texture)
    {
        SDL_GPUColorTargetInfo colorAttachmentInfo = {
            .texture = swapchain_texture,
            .clear_color = {
                .a = 1.0,
            },
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE,
        };

        SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(cmd_buf, &colorAttachmentInfo, 1, NULL);

        SDL_BindGPUGraphicsPipeline(render_pass, app_state->pipeline);
        SDL_BindGPUFragmentSamplers(render_pass,
                                    0,
                                    &(SDL_GPUTextureSamplerBinding){
                                        .sampler = app_state->sampler,
                                        .texture = app_state->texture,
                                    },
                                    1);
        SDL_BindGPUVertexBuffers(render_pass,
                                 0,
                                 &(SDL_GPUBufferBinding){.buffer = app_state->geometry_buffer, .offset = 0},
                                 1);
        SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);

        SDL_EndGPURenderPass(render_pass);
    }

    SDL_SubmitGPUCommandBuffer(cmd_buf);

    return true;
}
