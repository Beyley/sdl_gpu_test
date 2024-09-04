#include "gfx.h"

bool gfx_init(app_state_t *app_state)
{
    // Init the GPU device with the shader formats supported by shadercross
    app_state->gpu_device = SDL_CreateGPUDevice(SDL_ShaderCross_GetShaderFormats(), true, NULL);

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
        .codeSize = zigMainVertSize(),
        .entryPointName = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    };

    SDL_GPUShaderCreateInfo frag_create_info = {
        .code = zigMainFragPtr(),
        .codeSize = zigMainFragSize(),
        .entryPointName = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .samplerCount = 1,
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
        .vertexShader = vert_shader,
        .fragmentShader = frag_shader,
        .vertexInputState = {
            .vertexAttributeCount = 2,
            .vertexAttributes =
                (SDL_GPUVertexAttribute[2]){
                    (SDL_GPUVertexAttribute){
                        .location = 0,
                        .binding = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                        .offset = 0,
                    },
                    (SDL_GPUVertexAttribute){
                        .location = 1,
                        .binding = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                        .offset = sizeof(float) * 2,
                    },
                },
            .vertexBindingCount = 1,
            .vertexBindings = (SDL_GPUVertexBinding[1]){
                (SDL_GPUVertexBinding){
                    .binding = 0,
                    .inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                    .stride = sizeof(float) * 4,
                },
            },
        },
        .primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizerState = {
            .fillMode = SDL_GPU_FILLMODE_FILL,
            .cullMode = SDL_GPU_CULLMODE_NONE,
            .frontFace = SDL_GPU_FRONTFACE_CLOCKWISE,
        },
        .multisampleState = {
            .sampleCount = SDL_GPU_SAMPLECOUNT_1,
            .sampleMask = 0xF,
        },
        .depthStencilState = {
            .depthTestEnable = false,
            .depthWriteEnable = false,
        },
        .attachmentInfo = {
            .colorAttachmentCount = 1,
            .colorAttachmentDescriptions = &(SDL_GPUColorAttachmentDescription){
                .format = SDL_GetGPUSwapchainTextureFormat(app_state->gpu_device, app_state->window),
                .blendState = {
                    .blendEnable = false,
                    .alphaBlendOp = SDL_GPU_BLENDOP_ADD,
                    .colorBlendOp = SDL_GPU_BLENDOP_ADD,
                    .colorWriteMask = 0xF,
                    .srcAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
                    .dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ZERO,
                    .srcColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
                    .dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ZERO,
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
        .sizeInBytes = sizeof(vertex_data), // three vec2
        .usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX,
    };

    SDL_GPUBuffer *vert_buf = SDL_CreateGPUBuffer(app_state->gpu_device, &vert_buf_create_info);

    if (!vert_buf)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to create geometry buffer, error %s", SDL_GetError());
        return NULL;
    }

    SDL_SetGPUBufferName(app_state->gpu_device, vert_buf, "Static geometry");

    SDL_GPUTransferBufferCreateInfo transfer_buf_create_info = {
        .sizeInBytes = sizeof(vertex_data),
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
                              .transferBuffer = transfer_buffer,
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
        .layerCountOrDepth = 1,
        .levelCount = 1,
        .sampleCount = 1,
        .type = SDL_GPU_TEXTURETYPE_2D,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER,
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
                                                           .sizeInBytes = (Uint32)(width * height * 4),
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
                               .imageHeight = (Uint32)height,
                               .transferBuffer = transfer_buffer,
                               .imagePitch = (Uint32)width,
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
                                                  .addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                  .addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                  .addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                  .magFilter = SDL_GPU_FILTER_LINEAR,
                                                  .minFilter = SDL_GPU_FILTER_LINEAR,
                                                  .maxAnisotropy = 1,
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
        SDL_GPUColorAttachmentInfo colorAttachmentInfo;
        SDL_zero(colorAttachmentInfo);
        colorAttachmentInfo.texture = swapchain_texture;

        colorAttachmentInfo.clearColor.a = 1.0f;

        colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
        colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

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
