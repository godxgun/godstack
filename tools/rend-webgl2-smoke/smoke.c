/* Integration smoke for the public legacy Rend.h API, executed by Chromium. */
#define _POSIX_C_SOURCE 200809L
#define PEAK_IMPLEMENTATION
#define REND_IMPLEMENTATION
#include "Rend.h"
#include <emscripten/emscripten.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <string.h>

enum { WIDTH=160, HEIGHT=120 };
static PeakCtx *peak;
static PeakWindow window;
static RendRenderer screen;

typedef struct Vertex { float position[2]; } Vertex;
typedef struct Instance { float offset[3], color[3]; } Instance;

static const char vertex_shader[] =
    "#version 300 es\n"
    "layout(location=0) in vec2 position;\n"
    "layout(location=1) in vec3 offset;\n"
    "layout(location=2) in vec3 color;\n"
    "out vec3 vColor;\n"
    "void main(){gl_Position=vec4(position+offset.xy,offset.z,1.);vColor=color;}\n";
static const char fragment_shader[] =
    "#version 300 es\nprecision mediump float;\n"
    "layout(std140) uniform RendPushConstants {vec4 tint;};\n"
    "layout(std140) uniform RendUniforms_0 {vec4 multiplier;};\n"
    "in vec3 vColor;out vec4 fragColor;\n"
    "void main(){fragColor=vec4(vColor,1.)*tint*multiplier;}\n";
static const char sample_vertex[] =
    "#version 300 es\nout vec2 uv;\n"
    "void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));"
    "uv=p;gl_Position=vec4(p*2.-1.,0.,1.);}\n";
static const char sample_fragment[] =
    "#version 300 es\nprecision mediump float;\n"
    "uniform sampler2D rend_texture_0;in vec2 uv;out vec4 fragColor;\n"
    "void main(){fragColor=texture(rend_texture_0,uv);}\n";

static int check(int condition, const char *message)
{
    if (!condition) fprintf(stderr,"REND_SMOKE_FAIL: %s\n",message);
    return condition;
}
#define CHECK(condition,message) do {if (!check((condition),(message))) goto done;} while (0)
#define GL_CHECK(message) do {GLenum error=glGetError();if(error){fprintf(stderr,"REND_SMOKE_FAIL: %s GL error 0x%x\n",message,error);goto done;}} while(0)

static int pixel_is(const unsigned char *pixels,int width,int x,int y,int r,int g,int b,int a)
{
    const unsigned char *p=pixels+4*(y*width+x);
    if (p[0]!=r||p[1]!=g||p[2]!=b||p[3]!=a) {
        fprintf(stderr,"REND_SMOKE_FAIL: pixel %d,%d = %u,%u,%u,%u expected %d,%d,%d,%d\n",
                x,y,p[0],p[1],p[2],p[3],r,g,b,a);
        return 0;
    }
    return 1;
}

EMSCRIPTEN_KEEPALIVE int rend_smoke_resize(int width,int height)
{
    RendTexture *target;
    if (!screen||!check(rend_renderer_frame_begin(screen),"resized frame begin")) return 0;
    target=rend_renderer_color_target(screen);
    if (!check(rend_texture_width(target)==(uint32_t)width&&rend_texture_height(target)==(uint32_t)height,
               "default target follows actual canvas dimensions")) return 0;
    rend_cmd_render_begin(screen,0.f,0.f,1.f,1.f);
    rend_cmd_render_end(screen);
    rend_renderer_frame_end(screen,NULL);
    puts("PASS legacy Rend window target resize");
    return 1;
}

EMSCRIPTEN_KEEPALIVE int rend_smoke_finish(void)
{
    RendRenderer replacement;
    rend_renderer_destroy(screen);screen=NULL;
    rend_quit();
    replacement=rend_renderer_create(&window,REND_BACKEND_AUTO,NULL,false,NULL);
    if (!check(replacement!=NULL,"renderer/context recreation after quit")) return 0;
    if (!check(rend_renderer_frame_begin(replacement),"recreated renderer frame begin")) return 0;
    rend_cmd_render_begin(replacement,1.f,0.f,0.f,1.f);
    rend_cmd_render_end(replacement);
    rend_renderer_frame_end(replacement,NULL);
    rend_renderer_destroy(replacement);
    rend_quit();
    peak_window_close(&window);
    peak_quit(peak);peak=NULL;
    puts("PASS legacy Rend complete teardown and recreation");
    return 1;
}

int main(void)
{
    RendRenderer renderer=NULL;
    RendPipeline pipeline=NULL,sampling=NULL;
    RendBuffer vertices={0}, copied={0}, instances={0}, indices={0}, transfer={0},ubo={0};
    RendTexture texture={0}, small={0}, unsupported={0};
    unsigned char pixels[WIDTH*HEIGHT*4],texels[4*4*4],readback[4*4*4];
    const Vertex positions[3]={{{-.3f,-.35f}},{{.3f,-.35f}},{{0.f,.35f}}};
    /* Third instance overlaps green farther away; depth must reject its red. */
    const Instance draws[3]={{{-.5f,0.f,0.f},{1.f,0.f,0.f}},{{.5f,0.f,0.f},{0.f,1.f,0.f}},
                            {{.5f,0.f,.5f},{1.f,0.f,0.f}}};
    const uint16_t index_data[3]={0,1,2};
    const float tint[4]={1.f,1.f,1.f,1.f};
    const RendVertexBinding bindings[2]={{0,sizeof(Vertex),REND_INPUT_RATE_VERTEX},{1,sizeof(Instance),REND_INPUT_RATE_INSTANCE}};
    const RendVertexAttributes attributes[3]={{0,0,0,REND_FORMAT_R32G32_SFLOAT},
        {1,1,0,REND_FORMAT_R32G32B32_SFLOAT},{2,1,3*sizeof(float),REND_FORMAT_R32G32B32_SFLOAT}};
    const RendPushConstantInfo push={0,sizeof(tint)};
    RendBindingInfo descriptors={0};
    int ok=0,i;

    descriptors.texture_binding_count=1;descriptors.texture_bindings[0]=0;descriptors.texture_array_sizes[0]=1;
    descriptors.ubo_binding_count=1;descriptors.ubo_bindings[0]=0;descriptors.ubo_array_sizes[0]=1;
    peak=peak_init_legacy();
    CHECK(peak!=NULL,"Peak initialization");
    window=peak_window_open(peak,"canvas",WIDTH,HEIGHT,0);
    CHECK(window.running,"Peak canvas open");
    renderer=rend_renderer_create_offscreen(WIDTH,HEIGHT,REND_FORMAT_R8G8B8A8_UNORM,REND_BACKEND_AUTO,&descriptors);
    CHECK(renderer!=NULL,"AUTO selects legacy WebGL2");
    CHECK(!rend_renderer_create_offscreen(WIDTH,HEIGHT,REND_FORMAT_R8G8B8A8_UNORM,REND_BACKEND_VULKAN_14,NULL),"Vulkan cannot silently select WebGL2");
    CHECK(!rend_pipeline_create_compute_spirv(renderer,NULL,0,NULL,0),"compute rejected");
    CHECK(!rend_pipeline_create_graphics_spirv(renderer,NULL,0,NULL,0,NULL,0,NULL,0,NULL,0,
        REND_POLYGON_MODE_FILL,REND_CULL_MODE_NONE,REND_TOPOLOGY_TRIANGLE_LIST,REND_FORMAT_R8G8B8A8_UNORM,false),"SPIR-V rejected without GLSL");
    { RendBindingInfo bad={0};RendBuffer rejected;
      bad.ssbo_binding_count=1;bad.ssbo_bindings[0]=0;bad.ssbo_array_sizes[0]=1;
      CHECK(!rend_renderer_create_offscreen(4,4,REND_FORMAT_R8G8B8A8_UNORM,REND_BACKEND_WEBGL2,&bad),"storage descriptors rejected");
      memset(&bad,0,sizeof bad);bad.ubo_binding_count=1;bad.ubo_bindings[0]=7;bad.ubo_array_sizes[0]=1;
      CHECK(!rend_renderer_create_offscreen(4,4,REND_FORMAT_R8G8B8A8_UNORM,REND_BACKEND_WEBGL2,&bad),"reserved push UBO binding rejected");
      rejected=rend_buffer_create(renderer,16,REND_BUFFER_STORAGE,true);
      CHECK(!rejected.handle,"storage buffer creation rejected");
      rejected=rend_buffer_create(renderer,16,REND_BUFFER_INDIRECT,true);
      CHECK(!rejected.handle,"indirect buffer creation rejected");
    }
    CHECK(!rend_pipeline_create_graphics_glsl(renderer,sample_vertex,sample_fragment,NULL,0,NULL,0,NULL,0,
        REND_POLYGON_MODE_LINE,REND_CULL_MODE_NONE,REND_TOPOLOGY_TRIANGLE_LIST,REND_FORMAT_R8G8B8A8_UNORM,false),"polygon line mode rejected");
    unsupported=rend_texture_create(renderer,4,4,1,1,1,REND_FORMAT_B8G8R8A8_UNORM);
    CHECK(!unsupported.handle,"unsupported BGRA rejected");
    transfer=rend_buffer_create(renderer,sizeof(texels),REND_BUFFER_TRANSFER,false);
    CHECK(transfer.handle&&rend_buffer_mapped(&transfer),"host-visible transfer buffer");
    CHECK(rend_buffer_address(&transfer)==0,"GPU addresses unavailable on WebGL2");
    ubo=rend_buffer_create(renderer,sizeof(tint),REND_BUFFER_UNIFORM,false);
    CHECK(ubo.handle,"uniform buffer creation");
    rend_buffer_write(renderer,&ubo,tint,sizeof(tint),0);
    rend_descriptor_write_ubo(renderer,ubo,0,0);
    pipeline=rend_pipeline_create_graphics_glsl(renderer,vertex_shader,fragment_shader,
        bindings,2,attributes,3,&push,1,REND_POLYGON_MODE_FILL,REND_CULL_MODE_NONE,
        REND_TOPOLOGY_TRIANGLE_LIST,REND_FORMAT_R8G8B8A8_UNORM,true);
    CHECK(pipeline!=NULL,"GLSL ES 3 graphics pipeline");
    vertices=rend_buffer_create(renderer,sizeof(positions),REND_BUFFER_VERTEX,true);
    copied=rend_buffer_create(renderer,sizeof(positions),REND_BUFFER_VERTEX,true);
    instances=rend_buffer_create(renderer,sizeof(draws),REND_BUFFER_VERTEX,true);
    indices=rend_buffer_create(renderer,sizeof(index_data),REND_BUFFER_INDEX,true);
    CHECK(vertices.handle&&copied.handle&&instances.handle&&indices.handle,"vertex, instance and index buffers");
    rend_buffer_write(renderer,&vertices,positions,sizeof(positions),0);
    rend_buffer_copy(renderer,&copied,0,&vertices,0,sizeof(positions));
    rend_buffer_write(renderer,&instances,draws,sizeof(draws),0);
    rend_buffer_write(renderer,&indices,index_data,sizeof(index_data),0);
    GL_CHECK("buffer creation/write and descriptors");
    CHECK(rend_renderer_frame_begin(renderer),"GPU draw frame begin");
    rend_cmd_render_begin(renderer,0.f,0.f,0.f,1.f);
    rend_cmd_bind_pipeline(pipeline);
    rend_cmd_bind_vertex_buffer(pipeline,0,copied,0);
    rend_cmd_bind_vertex_buffer(pipeline,1,instances,0);
    GL_CHECK("vertex buffer binding");
    rend_cmd_bind_index_buffer(pipeline,indices,0,REND_INDEX_UINT16);
    GL_CHECK("index buffer binding");
    rend_cmd_push_constants(pipeline,(void *)tint,sizeof(tint));
    rend_cmd_draw_indexed(pipeline,3,0,0,3);
    GL_CHECK("indexed draw");
    rend_cmd_render_end(renderer);
    rend_renderer_frame_end(renderer,NULL);
    rend_renderer_read(renderer,pixels,sizeof(pixels));
    CHECK(pixel_is(pixels,WIDTH,40,60,255,0,0,255),"left indexed instance");
    CHECK(pixel_is(pixels,WIDTH,120,60,0,255,0,255),"right indexed instance");
    CHECK(pixel_is(pixels,WIDTH,80,5,0,0,0,255),"background clear");
    puts("PASS legacy Rend GPU vertex/index/copy/instancing/depth/UBO/push constants/readback");

    for (i=0;i<16;++i) {texels[4*i]=(unsigned char)(i*13);texels[4*i+1]=0;texels[4*i+2]=255;texels[4*i+3]=255;}
    texture=rend_texture_create_from_data(renderer,texels,4,4,REND_FORMAT_R8G8B8A8_UNORM);
    CHECK(texture.handle,"texture upload");
    CHECK(rend_texture_width(&texture)==4&&rend_texture_height(&texture)==4&&rend_texture_id(&texture)!=0,"texture metadata");
    rend_texture_read(renderer,&texture,readback,sizeof(readback));
    CHECK(!memcmp(texels,readback,sizeof(texels)),"asymmetric texture upload/readback orientation");
    rend_buffer_write(renderer,&transfer,texels,sizeof(texels),0);
    small=rend_texture_create(renderer,4,4,1,1,1,REND_FORMAT_R8G8B8A8_UNORM);
    CHECK(small.handle,"second texture creation");
    rend_texture_copy_buffer(renderer,&small,&transfer);
    rend_texture_read(renderer,&small,readback,sizeof(readback));
    CHECK(!memcmp(texels,readback,sizeof(texels)),"outside-frame buffer to texture");
    CHECK(rend_renderer_frame_begin(renderer),"blit frame begin");
    rend_cmd_render_begin(renderer,0.f,1.f,0.f,1.f);
    rend_cmd_render_end(renderer);
    rend_cmd_copy_buffer_to_texture(renderer,&small,&transfer);
    rend_cmd_blit(renderer,&small,rend_renderer_color_target(renderer),0,0,4,4,0,0,4,4);
    rend_cmd_render_begin_preserve(renderer);
    rend_cmd_render_end(renderer);
    rend_renderer_frame_end(renderer,NULL);
    rend_renderer_read(renderer,pixels,sizeof(pixels));
    for (i=0;i<4;++i) CHECK(!memcmp(pixels+i*WIDTH*4,texels+i*16,16),"top-left partial blit preserves uploaded row order");
    CHECK(pixel_is(pixels,WIDTH,10,10,0,255,0,255),"preserve pass retains unblitted pixels");
    puts("PASS legacy Rend texture upload/readback/buffer copy/blit/preserve");

    sampling=rend_pipeline_create_graphics_glsl(renderer,sample_vertex,sample_fragment,
        NULL,0,NULL,0,NULL,0,REND_POLYGON_MODE_FILL,REND_CULL_MODE_NONE,
        REND_TOPOLOGY_TRIANGLE_LIST,REND_FORMAT_R8G8B8A8_UNORM,false);
    CHECK(sampling!=NULL,"texture sampling pipeline");
    /* Uniform blue texels isolate sampler binding from filtering/orientation. */
    for(i=0;i<16;++i) texels[4*i]=0;
    rend_texture_copy_data(renderer,&texture,texels,sizeof(texels));
    rend_descriptor_write_texture(renderer,&texture,0,0);
    CHECK(rend_renderer_frame_begin(renderer),"texture pass frame begin");
    rend_cmd_render_begin_texture(renderer,&small);
    rend_cmd_bind_pipeline(sampling);
    rend_cmd_draw(sampling,3,1);
    rend_cmd_render_end_texture(renderer,&small);
    rend_renderer_frame_end(renderer,NULL);
    rend_texture_read(renderer,&small,readback,sizeof(readback));
    CHECK(pixel_is(readback,4,2,2,0,0,255,255),"GLSL sampler descriptor renders to caller texture");
    /* Transfer after a smaller texture pass must detach its old depth image. */
    CHECK(rend_renderer_frame_begin(renderer),"blit after differently sized texture pass");
    rend_cmd_blit(renderer,&small,rend_renderer_color_target(renderer),0,0,4,4,0,0,4,4);
    GL_CHECK("blit framebuffer after smaller texture pass");
    rend_cmd_render_begin_preserve(renderer);
    rend_cmd_render_end(renderer);
    rend_renderer_frame_end(renderer,NULL);
    rend_renderer_read(renderer,pixels,sizeof(pixels));
    CHECK(pixel_is(pixels,WIDTH,2,2,0,0,255,255),"small-pass depth attachment does not invalidate larger transfer target");
    puts("PASS legacy Rend sampler descriptors and offscreen texture passes");
    screen=rend_renderer_create(&window,REND_BACKEND_WEBGL2,NULL,false,NULL);
    CHECK(screen!=NULL,"windowed WebGL2 renderer");
    CHECK(rend_renderer_frame_begin(screen),"window frame begin");
    rend_cmd_render_begin(screen,0.f,1.f,0.f,1.f);
    rend_cmd_render_end(screen);
    rend_renderer_frame_end(screen,NULL);
    ok=1;
done:
    if (renderer) {
        if (texture.handle) rend_texture_destroy(renderer,&texture);
        if (small.handle) rend_texture_destroy(renderer,&small);
        rend_buffer_destroy(&vertices);rend_buffer_destroy(&copied);
        rend_buffer_destroy(&instances);rend_buffer_destroy(&indices);rend_buffer_destroy(&transfer);
        rend_buffer_destroy(&ubo);
        rend_renderer_destroy(renderer);
    }
    if (ok) puts("REND_SMOKE_PASS");
    else {rend_quit();peak_window_close(&window);peak_quit(peak);}
    return ok?0:1;
}
