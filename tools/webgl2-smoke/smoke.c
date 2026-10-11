#define REND2_IMPLEMENTATION
#include "../../Rend2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

enum { WIDTH = 160, HEIGHT = 120 };
typedef struct Vertex { float position[2]; } Vertex;
typedef struct Instance { float offset[2]; float color[3]; } Instance;
typedef struct Allocation { void *raw; void *aligned; size_t size; } Allocation;

#if defined(__EMSCRIPTEN__)
static const char vertex_source[] =
    "#version 300 es\nlayout(location=0) in vec2 position;\n"
    "layout(location=1) in vec2 offset; layout(location=2) in vec3 color;\n"
    "uniform mat4 transform; out vec3 vColor;\n"
    "void main(){ vec4 p=transform*vec4(position+offset,0.,1.);"
    "gl_Position=vec4(p.x,-p.y,2.*p.z-p.w,p.w);vColor=color;}\n";
static const char fragment_source[] =
    "#version 300 es\nprecision mediump float;in vec3 vColor;"
    "out vec4 fragColor;void main(){fragColor=vec4(vColor,1.);}\n";
#endif

static int allocate(size_t size, size_t alignment, Allocation *a) {
  uintptr_t p;
  if (!alignment || (alignment & (alignment - 1)) || size > SIZE_MAX - alignment)
    return 0;
  a->raw = malloc(size + alignment - 1);
  if (!a->raw) return 0;
  p = ((uintptr_t)a->raw + alignment - 1) & ~(uintptr_t)(alignment - 1);
  a->aligned = (void *)p; a->size = size; return 1;
}
static void release(Allocation *a) { free(a->raw); memset(a, 0, sizeof(*a)); }
static int diagnostic_is(RendDiagnosticCode code) {
  RendDiagnostic d; return rend_last_diagnostic(&d) && d.code == code;
}
static int read_blob(const char *path, unsigned char **out, size_t *size) {
  FILE *f; long n; unsigned char *p;
  *out = NULL; *size = 0; f = fopen(path, "rb"); if (!f) return 0;
  if (fseek(f, 0, SEEK_END) || (n = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return 0; }
  p = (unsigned char *)malloc((size_t)n); if (!p) { fclose(f); return 0; }
  if (fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); fclose(f); return 0; }
  fclose(f); *out = p; *size = (size_t)n; return 1;
}
int main(int argc, char **argv) {
  RendBackend backend = NULL, backend2 = NULL; RendQueue queue = NULL; RendCommandPool pool = NULL;
  RendCommandList commands = NULL; RendPipeline pipeline = NULL, pipeline2 = NULL;
  RendHeap data_heap = NULL, image_heap = NULL; RendTexture texture = NULL;
  RendTextureView view = NULL; RendTimeline timeline = NULL;
  RendDeviceInfo const *device = NULL; RendDiscovery *discovery = NULL;
  RendQueueParams qp; RendCommandPoolParams cp; RendParams params;
  RendMemoryRequirements req; RendStorageRequirements storage_req;
  RendHeapClassInfo heap_class; RendTextureDesc td; RendTextureViewDesc vd;
  RendHeapDesc hd; RendHeapInfo hi; RendGraphicsPipelineDesc pd;
  RendVertexAttribute attrs[3]; RendRenderAttachment color;
  RendRenderDesc render; RendViewport viewport; RendScissor scissor;
  RendDepthState depth; RendUniformDesc uniform; RendDrawDesc draw;
  RendSubmitDesc submit; RendCommandList submitted[1]; RendDiagnostic diag;
  Allocation backend_mem = {0}, backend_mem2 = {0}, discovery_mem = {0};
  RendQueueFamilyInfo families[128]; uint32_t family_count = 0, i;
  uint64_t texture_storage_size = 0;
  Vertex vertices[3] = {{{-.32f,-.34f}},{{.32f,-.34f}},{{0,.36f}}};
  Instance instances[2] = {{{-.48f,0},{1,.1f,.1f}},{{.48f,0},{.1f,.9f,.2f}}};
  const float identity[16] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  const float clear[4] = {.02f,.03f,.04f,1};
  unsigned char *vertex_bytes = NULL, *fragment_bytes = NULL;
  size_t vertex_size = 0, fragment_size = 0; unsigned char *pixels = NULL;
  int result = 1, backend_ready = 0, list_ready = 0;
  (void)argc;
  memset(&params, 0, sizeof params);
  memset(&qp, 0, sizeof qp); qp.submission_count=1; qp.max_submit_command_lists=1;
  memset(&cp, 0, sizeof cp); cp.command_list_count=1;
#if defined(__EMSCRIPTEN__)
  params.backend = REND_BACKEND_WEBGL2;
  params.canvas_selector = "#canvas"; params.canvas_width=WIDTH; params.canvas_height=HEIGHT;
  params.profile=REND_PROFILE_GRAPHICS_COMPUTE; params.queues=&qp; params.queue_count=1;
  params.command_pools=&cp; params.command_pool_count=1;
  params.heap_count=2; params.texture_count=1; params.texture_view_count=1;
  params.pipeline_count=1; params.timeline_count=1;
  vertex_bytes=(unsigned char *)vertex_source; vertex_size=sizeof(vertex_source)-1;
  fragment_bytes=(unsigned char *)fragment_source; fragment_size=sizeof(fragment_source)-1;
#else
  {
    uint32_t k;
    if (argc != 3 || !read_blob(argv[1], &vertex_bytes, &vertex_size) ||
        !read_blob(argv[2], &fragment_bytes, &fragment_size)) {
      fprintf(stderr,"usage: %s vertex.spv fragment.spv\n",argv[0]); goto done;
    }
    if (!rend_discovery_memory_requirements(&req) || !allocate(req.size,req.alignment,&discovery_mem) ||
        !rend_discovery_create(discovery_mem.aligned,discovery_mem.size,families,128,&discovery)) goto done;
    for (k=0;k<rend_discovery_device_count(discovery);++k) {
      const RendDeviceInfo *candidate=rend_discovery_device(discovery,k);
      if (rend_device_supports_profile(candidate,REND_PROFILE_GRAPHICS_COMPUTE)) { device=candidate; break; }
    }
    if (!device) { fprintf(stderr,"no Vulkan device supports Rend2 graphics-compute\n"); goto done; }
    {
      const RendQueueFamilyInfo *f=rend_device_queue_families(device,&family_count); int found=0;
      for(i=0;i<family_count;++i) if(f[i].capabilities&REND_QUEUE_GRAPHICS) { qp.family_index=f[i].family_index;found=1;break; }
      if(!found) goto done;
    }
    qp.max_submit_waits=0; cp.family_index=qp.family_index;
    params.profile=REND_PROFILE_GRAPHICS_COMPUTE;params.device=device;params.queues=&qp;params.queue_count=1;
    params.command_pools=&cp;params.command_pool_count=1;params.heap_count=2;params.texture_count=1;
    params.texture_view_count=1;params.pipeline_count=1;params.timeline_count=1;
  }
#endif
  if (!rend_memory_requirements(&params,&req) || !allocate(req.size,req.alignment,&backend_mem) ||
      !rend_place_in_memory(backend_mem.aligned,backend_mem.size,&params,&backend) ||
      ((params.backend!=REND_BACKEND_NONE) && rend_backend_kind(backend)!=params.backend) ||
      !rend_queue_get(backend,0,&queue) || !rend_command_pool_get(backend,0,&pool)) goto done;
  backend_ready=1;
  {
    RendParams wrong=params; RendMemoryRequirements ignored;
#if defined(__EMSCRIPTEN__)
    wrong.backend=REND_BACKEND_VULKAN;
#else
    wrong.backend=REND_BACKEND_WEBGL2;
#endif
    if(rend_memory_requirements(&wrong,&ignored)||!diagnostic_is(REND_DIAG_UNSUPPORTED))goto done;
  }
  {
    RendParams second=params; RendMemoryRequirements second_req;
    second.canvas_selector="#canvas2"; second.canvas_width=8; second.canvas_height=8;
    if(!rend_memory_requirements(&second,&second_req)||!allocate(second_req.size,second_req.alignment,&backend_mem2)||
       !rend_place_in_memory(backend_mem2.aligned,backend_mem2.size,&second,&backend2))goto done;
  }
#if !defined(__EMSCRIPTEN__)
  if (rend_backend_kind(backend)!=REND_BACKEND_VULKAN) goto done;
#else
  if (rend_backend_kind(backend)!=REND_BACKEND_WEBGL2) goto done;
#endif
  memset(&pd,0,sizeof pd); pd.vertex=(RendShaderCode){vertex_bytes,vertex_size};
  pd.fragment=(RendShaderCode){fragment_bytes,fragment_size};pd.color_format=REND_FORMAT_RGBA8_UNORM;
  pd.samples=1;pd.abi_tag=REND_SHADER_ABI_PORTABLE_V1;pd.attributes=attrs;pd.attribute_count=3;pd.uniform_bytes=64;
  attrs[0]=(RendVertexAttribute){0,2,sizeof(Vertex),offsetof(Vertex,position),0};
  attrs[1]=(RendVertexAttribute){1,2,sizeof(Instance),offsetof(Instance,offset),1};
  attrs[2]=(RendVertexAttribute){2,3,sizeof(Instance),offsetof(Instance,color),1};
  {
    RendVertexAttribute saved=attrs[0]; RendPipeline rejected=NULL;
    attrs[0].components=5;
    if(rend_graphics_pipeline_create(backend,&pd,&rejected)||rejected||!diagnostic_is(REND_DIAG_INVALID_ARGUMENT))goto done;
    attrs[0]=saved;
  }
#if defined(__EMSCRIPTEN__)
  {
    static const char invalid_shader[]="#version 300 es\nnot valid GLSL";
    RendGraphicsPipelineDesc bad=pd; RendPipeline rejected=NULL;
    bad.vertex=(RendShaderCode){invalid_shader,sizeof(invalid_shader)-1};
    if(rend_graphics_pipeline_create(backend,&bad,&rejected)||rejected||!diagnostic_is(REND_DIAG_NATIVE))goto done;
  }
#endif
  if (!rend_graphics_pipeline_create(backend,&pd,&pipeline) ||
      !rend_graphics_pipeline_create(backend2,&pd,&pipeline2)) goto done;
  if(rend_pipeline_destroy(backend,pipeline2)||!diagnostic_is(REND_DIAG_INVALID_ARGUMENT))goto done;
  if(!rend_pipeline_destroy(backend2,pipeline2))goto done;
  pipeline2=NULL;
  if(!rend_backend_destroy(backend2))goto done;
  backend2=NULL; release(&backend_mem2);
#if defined(__EMSCRIPTEN__)
  {
    RendParams bad=params; RendMemoryRequirements bad_req; RendBackend failed=NULL;
    Allocation bad_mem={0}; bad.canvas_selector="#rend2-missing-canvas";
    if(!rend_memory_requirements(&bad,&bad_req)||!allocate(bad_req.size,bad_req.alignment,&bad_mem))goto done;
    if(rend_place_in_memory(bad_mem.aligned,bad_mem.size,&bad,&failed)||failed||!diagnostic_is(REND_DIAG_NATIVE)) { release(&bad_mem); goto done; }
    release(&bad_mem);
  }
#endif
  memset(&hd,0,sizeof hd);hd.kind=REND_HEAP_DATA;hd.size=sizeof(vertices)+sizeof(instances)+WIDTH*HEIGHT*4;
  {
    RendHeapClassInfo choices[32]; uint32_t n=0;
    if(!rend_heap_classes(backend,REND_HEAP_DATA,NULL,0,&n)||!n||n>32||
       !rend_heap_classes(backend,REND_HEAP_DATA,choices,32,&n))goto done;
    for(i=0;i<n;++i)if((choices[i].memory_properties&REND_MEMORY_HOST_VISIBLE)!=0)break;
    if(i==n)goto done;
    heap_class=choices[i];
  }
  hd.memory_class=heap_class.id;
  if(!rend_heap_memory_requirements(backend,&hd,&storage_req)||!rend_heap_create(backend,&hd,&data_heap)||
     !rend_heap_get_info(backend,data_heap,&hi)||!hi.cpu_base)goto done;
  memcpy(hi.cpu_base,vertices,sizeof vertices);memcpy((char *)hi.cpu_base+sizeof vertices,instances,sizeof instances);
  if(rend_heap_flush(backend,(RendHeapRange){data_heap,UINT64_MAX,1})||!diagnostic_is(REND_DIAG_INVALID_ARGUMENT))goto done;
  memset(&hd,0,sizeof hd);hd.kind=REND_HEAP_TEXTURE_STORAGE;hd.size=(uint64_t)WIDTH*HEIGHT*4;
  memset(&td,0,sizeof td);td.width=WIDTH;td.height=HEIGHT;td.depth=td.mip_levels=td.array_layers=td.samples=1;
  td.format=REND_FORMAT_RGBA8_UNORM;td.usage=REND_TEXTURE_COLOR_ATTACHMENT|REND_TEXTURE_TRANSFER_SRC;
  {
    RendHeapClassInfo choices[32]; uint32_t n=0;
    if(!rend_heap_classes(backend,REND_HEAP_TEXTURE_STORAGE,NULL,0,&n)||!n||n>32||
       !rend_heap_classes(backend,REND_HEAP_TEXTURE_STORAGE,choices,32,&n))goto done;
    for(i=0;i<n;++i)if(rend_texture_memory_requirements(backend,&td,choices[i].id,&storage_req))break;
    if(i==n)goto done;
    heap_class=choices[i]; texture_storage_size=storage_req.size;
  }
  hd.size=texture_storage_size; hd.memory_class=heap_class.id;
  if(!rend_heap_memory_requirements(backend,&hd,&storage_req)||!rend_heap_create(backend,&hd,&image_heap)||
     !rend_texture_create(backend,&td,image_heap,0,hd.size,&texture))goto done;
  memset(&vd,0,sizeof vd);vd.format=REND_FORMAT_RGBA8_UNORM;vd.view_type=REND_VIEW_2D;vd.aspect=REND_ASPECT_COLOR;vd.mip_count=vd.layer_count=1;
  if(!rend_texture_view_create(backend,texture,&vd,&view))goto done;
  if(!rend_timeline_create(queue,0,&timeline))goto done;
  if(!rend_command_list_begin(pool,0,&commands))goto done;
  list_ready=1;
  color.view=view;memcpy(color.clear,clear,sizeof clear);
  memset(&render,0,sizeof render);render.colors=&color;render.color_count=1;render.width=WIDTH;render.height=HEIGHT;
  if(!rend_cmd_begin_render(commands,&render))goto done;
  memset(&viewport,0,sizeof viewport);viewport.width=WIDTH;viewport.height=HEIGHT;viewport.max_depth=1;
  scissor=(RendScissor){0,0,WIDTH,HEIGHT};depth=(RendDepthState){0,0,REND_COMPARE_ALWAYS};
  if(!rend_cmd_set_viewport(commands,&viewport)||!rend_cmd_set_scissor(commands,&scissor)||
     !rend_cmd_set_depth_state(commands,&depth)||!rend_cmd_set_pipeline(commands,pipeline))goto done;
  uniform=(RendUniformDesc){"transform",0,REND_UNIFORM_MATRIX4,identity};
  if(!rend_cmd_set_uniform(commands,&uniform))goto done;
  draw.vertices=(RendHeapRange){data_heap,0,sizeof vertices};
  draw.instances=(RendHeapRange){data_heap,sizeof vertices,sizeof instances};draw.vertex_count=3;draw.instance_count=2;
  if(!rend_heap_flush(backend,(RendHeapRange){data_heap,0,sizeof(vertices)+sizeof(instances)})||
     !rend_cmd_draw(commands,&draw))goto done;
  if(rend_pipeline_destroy(backend,pipeline)||!diagnostic_is(REND_DIAG_INVALID_ARGUMENT))goto done;
  if(!rend_cmd_end_render(commands)||
     !rend_cmd_copy_texture_to_data(commands,texture,(RendHeapRange){data_heap,sizeof(vertices)+sizeof(instances),(uint64_t)WIDTH*HEIGHT*4},WIDTH,HEIGHT)||
     !rend_command_list_end(commands))goto done;
  submitted[0]=commands;memset(&submit,0,sizeof submit);submit.commands=submitted;submit.command_count=1;
  submit.completion=(RendTimelinePoint){timeline,1};if(!rend_queue_submit(queue,&submit)||!rend_backend_wait_idle(backend))goto done;
  if(!rend_heap_invalidate(backend,(RendHeapRange){data_heap,sizeof(vertices)+sizeof(instances),(uint64_t)WIDTH*HEIGHT*4}))goto done;
  pixels=(unsigned char *)hi.cpu_base+sizeof(vertices)+sizeof(instances);
  {
    unsigned char *p=pixels+(size_t)60*WIDTH*4+(size_t)116*4;
    if(p[1]<100||p[0]<10) { fprintf(stderr,"pixel mismatch %u,%u,%u,%u\n",p[0],p[1],p[2],p[3]);goto done; }
    printf("COMMON_SMOKE_PASS backend=%d rgba=%u,%u,%u,%u; negative checks passed\n",(int)rend_backend_kind(backend),p[0],p[1],p[2],p[3]);
  }
  /* Fixed pipeline capacity must fail deterministically after its only slot is occupied. */
  { RendPipeline extra=NULL;
    if(rend_graphics_pipeline_create(backend,&pd,&extra)||extra||!diagnostic_is(REND_DIAG_CAPACITY))goto done;
  }
#if defined(__EMSCRIPTEN__)
  if(rend_webgl2_present_texture(backend,NULL)||!diagnostic_is(REND_DIAG_INVALID_ARGUMENT)||
     !rend_webgl2_present_texture(backend,texture))goto done;
#endif
  result=0;
done:
  if(result) { if(rend_last_diagnostic(&diag))fprintf(stderr,"COMMON_SMOKE_FAIL: %s (code %d native %d)\n",diag.message,diag.code,diag.native_code); }
  if(backend2) {
    if(pipeline2)rend_pipeline_destroy(backend2,pipeline2);
    rend_backend_destroy(backend2);
  }
  if(backend && backend_ready) {
    if(list_ready) rend_command_pool_reset(pool);
    if(view)rend_texture_view_destroy(backend,view);
    if(texture)rend_texture_destroy(backend,texture);
    if(image_heap)rend_heap_destroy(backend,image_heap);
    if(data_heap)rend_heap_destroy(backend,data_heap);
    if(pipeline)rend_pipeline_destroy(backend,pipeline);
    if(timeline)rend_timeline_destroy(timeline);
    rend_backend_destroy(backend);
  }
  if(discovery)rend_discovery_destroy(discovery);
  release(&backend_mem);release(&backend_mem2);release(&discovery_mem);
#if !defined(__EMSCRIPTEN__)
  free(vertex_bytes);free(fragment_bytes);
#endif
  return result;
}
