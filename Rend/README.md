# Rend 

Send data to the GPU through Vulkan 1.4. Define `PEAK_VULKAN` when compiling.
`REND_BACKEND_AUTO` selects Vulkan; creation returns `NULL` if it is unavailable.

## Memory lifetimes

Vulkan manages its own host allocations using the default allocation callbacks.
Buffers and caller-created textures still use renderer-lifetime device-memory
arenas: destroying these objects does not reclaim their arena space. Avoid
unbounded resource replacement within one renderer. Internal depth images own
separate device allocations; replacement reclaims both the image and its memory
once GPU use has completed.

The unused frame-lifetime allocator and CPU backend have been removed in 2.0.
CPU shader callbacks and `rend_pipeline_create_graphics_c` are no longer supported.
For software-drawn pixels, use Peak's backbuffer and presentation API directly.

Run the offscreen Vulkan lifetime regression from the godstack directory with
`./build rend test` (requires Vulkan validation layers).

Here's an example render loop used in my game! 

```c
        if (rend_renderer_frame_begin(renderer)) {

            // First pass: draw to a texture representing the in-game pixel art dimentions.
            rend_cmd_render_begin_texture(renderer, &canvas); { // brackets for style
                rend_cmd_bind_pipeline(ui_pipeline);
                rend_cmd_push_constants(ui_pipeline, &push_constants, sizeof(push_constants));
                rend_cmd_draw(ui_pipeline, vert_count, 1); // a vertex buffer is not bound because it's passed via push constants
            } rend_cmd_render_end_texture(renderer, &canvas);

            // Second pass: apply post processing using the native swapchain
            rend_cmd_render_begin(renderer, 1.0, 0.5, 0.0, 1.0); { // RGBA clear colors
                rend_cmd_bind_pipeline(present_pipeline);
                rend_cmd_push_constants(present_pipeline, &present_pc, sizeof(present_pc));
                rend_cmd_draw(present_pipeline, 4, 1); // 4 vertices and one instance
            } rend_cmd_render_end(renderer);

            rend_renderer_frame_end(renderer, &delta);
        }

    }
```
