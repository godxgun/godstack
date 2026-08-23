# Rend 

Send data to the GPU in a stable cross-platform and cross-graphics-API way. 

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
