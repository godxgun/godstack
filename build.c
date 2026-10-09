#include "Poof.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_rend_abi_demo(void);
static int build_rend2_demo(void);
static int build_snake(void);

static void
add_peak_config(Poof_CC *cc)
{
    poof_cmd_append(&cc->includes, ".");

#if defined(_WIN32)
    {
        const char *vulkan_sdk = getenv("VULKAN_SDK");
        if (vulkan_sdk) {
            char inc_buf[512], lib_buf[512];
            snprintf(inc_buf, sizeof(inc_buf), "-I%s/Include", vulkan_sdk);
            snprintf(lib_buf, sizeof(lib_buf), "-L%s/Lib", vulkan_sdk);
            poof_cc_append_win32(cc, strdup(inc_buf), strdup(lib_buf));
        }
    }
    poof_cc_append_win32(cc, "-lvulkan-1");
#else
    poof_cc_append_linux(cc, "-lvulkan");
#endif
}

static void
slangc_entry(Poof_Batch *batch, const char *src, const char *entry, const char *stage, const char *out)
{
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "slangc", src, "-target", "spirv", "-entry", entry, "-stage", stage, "-o", out);
    poof_batch_append_cmd(batch, cmd);
}

static void
add_rend_demo(Poof_Batch *batch, const char *src, const char *out, uint32_t opt, const char *define)
{
    Poof_CC cc;
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = opt;
    cc.output = out;
    poof_cmd_append(&cc.inputs, src);
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.defines, "PEAK_VULKAN");
    if (define) poof_cmd_append(&cc.defines, define);
    poof_cmd_append(&cc.libs, "m", "z");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    add_peak_config(&cc);
    poof_batch_append_cc(batch, &cc);
}

static void
add_peak_link(Poof_CC *cc)
{
    poof_cc_append_linux(cc, "-ldl", "-lpthread", "-lutil");
    poof_cc_append_macos(cc, "-framework", "AppKit", "-framework", "AudioToolbox",
        "-framework", "QuartzCore", "-framework", "CoreGraphics", "-framework", "GameController");
}

static bool
build_peak_native(const char *src, const char *out)
{
    Poof_CC cc = {0};
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = out;
    poof_cmd_append(&cc.inputs, src);
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations");
    add_peak_link(&cc);
    return poof_cc_run(&cc);
}

static bool
build_peak_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/peak");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/peak/demo";
    poof_cmd_append(&cc.inputs, "demos/peak/demo.c");
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror", "-Wno-deprecated-declarations");
    add_peak_config(&cc);
    add_peak_link(&cc);
    return poof_cc_run(&cc);
}

static bool
build_multiplatform_demo(void)
{
    poof_mkdir("demos/multiplatform");
    if (!build_peak_native("demos/multiplatform/demo.c", "demos/multiplatform/demo")) return false;
    return build_peak_native("demos/multiplatform/demo_run.c", "demos/multiplatform/demo_run");
}

static bool
build_fuse_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/fuse");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/fuse/demo";
    poof_cmd_append(&cc.inputs, "demos/fuse/demo.c");
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.defines, "FUSE_DEBUG");
    poof_cmd_append(&cc.libs, "m");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_grit_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/grit");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/grit/demo";
    poof_cmd_append(&cc.inputs, "demos/grit/demo.c");
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_doc_generator_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/doc-generator");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/doc-generator/doc_generator";
    poof_cmd_append(&cc.inputs, "demos/doc-generator/doc_generator.c");
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static int
build_compute_demo(void)
{
	Poof_Batch batch = {0};

	poof_mkdir("demos/compute");
	slangc_entry(&batch, "demos/compute/rend_compute.slang", "particleMain", "compute", "demos/compute/particle.spv");
	slangc_entry(&batch, "demos/compute/rend_compute.slang", "sandMain", "compute", "demos/compute/sand.spv");
	slangc_entry(&batch, "demos/compute/rend_compute.slang", "vertMain", "vertex", "demos/compute/vert.spv");
	slangc_entry(&batch, "demos/compute/rend_compute.slang", "fragMain", "fragment", "demos/compute/frag.spv");
	add_rend_demo(&batch, "demos/compute/rend_compute.c", "demos/compute/rend_compute_demo", POOF_O0, "REND_DEBUG");
	return poof_batch_run(&batch, "compute");
}

static int
build_teapot_demo(void)
{
	Poof_Batch batch = {0};

	poof_mkdir("demos/teapot");
	slangc_entry(&batch, "demos/teapot/shaders/basic.slang", "vertMain", "vertex", "demos/teapot/basic.vert.spv");
	slangc_entry(&batch, "demos/teapot/shaders/basic.slang", "fragMain", "fragment", "demos/teapot/basic.frag.spv");
	slangc_entry(&batch, "demos/teapot/shaders/texture.slang", "vertMain", "vertex", "demos/teapot/texture.vert.spv");
	slangc_entry(&batch, "demos/teapot/shaders/texture.slang", "fragMain", "fragment", "demos/teapot/texture.frag.spv");
	add_rend_demo(&batch, "demos/teapot/rend_teapot.c", "demos/teapot/rend_teapot", POOF_O0, "REND_DEBUG");
	return poof_batch_run(&batch, "teapot");
}

static int
build_dashboard_demo(void)
{
	Poof_Batch batch = {0};

	poof_mkdir("demos/dashboard");
	slangc_entry(&batch, "demos/dashboard/fuse_ui.slang", "vertMain", "vertex", "demos/dashboard/fuse_ui.vert.spv");
	slangc_entry(&batch, "demos/dashboard/fuse_ui.slang", "fragMain", "fragment", "demos/dashboard/fuse_ui.frag.spv");
	slangc_entry(&batch, "demos/dashboard/fuse_text.slang", "vertMain", "vertex", "demos/dashboard/fuse_text.vert.spv");
	slangc_entry(&batch, "demos/dashboard/fuse_text.slang", "fragMain", "fragment", "demos/dashboard/fuse_text.frag.spv");
	add_rend_demo(&batch, "demos/dashboard/dashboard.c", "demos/dashboard/dashboard", POOF_O0, "REND_DEBUG");
	return poof_batch_run(&batch, "dashboard");
}

static bool
build_cast_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/cast");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/cast/demo";
    poof_cmd_append(&cc.inputs, "demos/cast/demo.c");
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.defines, "CAST_DEBUG");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_rend_demo(void)
{
    Poof_CC cc = {0};

    poof_mkdir("demos/rend");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/rend/demo";
    poof_cmd_append(&cc.inputs, "demos/rend/demo.c");
    poof_cmd_append(&cc.includes, ".");
    poof_cmd_append(&cc.defines, "PEAK_VULKAN", "REND_DEBUG");
    poof_cmd_append(&cc.libs, "m");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    add_peak_link(&cc);
    add_peak_config(&cc);
    return poof_cc_run(&cc);
}

static int
build_rend_abi_demo(void)
{
	const struct { const char *name, *entry, *stage; } shaders[] = {
		{ "compute", "computeMain", "compute" },
		{ "consume", "consumeMain", "compute" },
		{ "vertex", "vertexMain", "vertex" },
		{ "fragment", "fragmentMain", "fragment" },
	};
	Poof_CC cc = {0};
	size_t i;

	if (!poof_has_cmd("slangc") || !poof_has_cmd("spirv-val")) {
		fprintf(stderr, "Rend ABI checks require Slang 2026.14.1+ and SPIRV-Tools 2026.3+\n");
		return 0;
	}
	poof_mkdir("bin");
	for (i = 0; i < sizeof(shaders) / sizeof(*shaders); i++) {
		char artifact[128], reflection[128];
		Poof_Cmd cmd = {0};
		int ok;

		snprintf(artifact, sizeof(artifact), "bin/rend_abi.%s.spv", shaders[i].name);
		snprintf(reflection, sizeof(reflection), "bin/rend_abi.%s.json", shaders[i].name);
		poof_cmd_append(&cmd, "slangc", "demos/rend-abi/rend_abi.slang", "-target", "spirv", "-profile", "spirv_1_5",
			"-emit-spirv-directly", "-fvk-use-entrypoint-name", "-fvk-use-c-layout", "-matrix-layout-row-major",
			"-capability", "spvDescriptorHeapEXT", "-entry", shaders[i].entry, "-stage", shaders[i].stage,
			"-reflection-json", reflection, "-o", artifact);
		ok = poof_cmd_run(&cmd);
		poof_cmd_free(&cmd);
		if (!ok)
			return 0;
		poof_cmd_append(&cmd, "spirv-val", "--target-env", "vulkan1.4", "--scalar-block-layout", artifact);
		ok = poof_cmd_run(&cmd);
		poof_cmd_free(&cmd);
		if (!ok)
			return 0;
	}
	poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
	cc.debug_mode = 1;
	cc.optimization = POOF_O0;
	cc.output = "bin/rend_abi_vk";
	poof_cmd_append(&cc.inputs, "demos/rend-abi/rend_abi_vk.c");
	poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wextra", "-Werror");
	poof_cc_append_linux(&cc, "-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=aligned_alloc,--wrap=posix_memalign");
	add_peak_config(&cc);
	return poof_cc_run(&cc);
}

static int
build_snake(void)
{
	const struct { const char *entry, *stage, *artifact, *reflection; } shaders[] = {
		{ "vertMain", "vertex", "demos/snake/snake.vert.spv", "bin/snake.vertex.json" },
		{ "fragMain", "fragment", "demos/snake/snake.frag.spv", "bin/snake.fragment.json" },
	};
	Poof_CC cc = {0};
	size_t i;

	if (!poof_has_cmd("slangc") || !poof_has_cmd("spirv-val")) {
		fprintf(stderr, "Rend2 snake requires Slang with descriptor-heap support and SPIRV-Tools\n");
		return 0;
	}
	poof_mkdir("bin");
	for (i = 0; i < sizeof(shaders) / sizeof(*shaders); i++) {
		Poof_Cmd cmd = {0};
		int ok;

		poof_cmd_append(&cmd, "slangc", "demos/snake/shaders/snake.slang", "-target", "spirv", "-profile", "spirv_1_5",
			"-emit-spirv-directly", "-fvk-use-entrypoint-name", "-fvk-use-c-layout", "-matrix-layout-row-major",
			"-capability", "spvDescriptorHeapEXT", "-entry", shaders[i].entry, "-stage", shaders[i].stage,
			"-reflection-json", shaders[i].reflection, "-o", shaders[i].artifact);
		ok = poof_cmd_run(&cmd);
		poof_cmd_free(&cmd);
		if (!ok) return 0;
		poof_cmd_append(&cmd, "spirv-val", "--target-env", "vulkan1.4", "--scalar-block-layout", shaders[i].artifact);
		ok = poof_cmd_run(&cmd);
		poof_cmd_free(&cmd);
		if (!ok) return 0;
	}
	poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
	cc.debug_mode = true;
	cc.optimization = POOF_O0;
	cc.output = "demos/snake/snake";
	poof_cmd_append(&cc.inputs, "demos/snake/snake.c");
	poof_cmd_append(&cc.includes, ".");
	poof_cmd_append(&cc.defines, "PEAK_VULKAN");
	poof_cmd_append(&cc.libs, "m");
	poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
	add_peak_link(&cc);
	add_peak_config(&cc);
	return poof_cc_run(&cc);
}

static int
build_rend2_demo(void)
{
	Poof_CC cc = {0};

	poof_mkdir("demos/rend2");
	poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
	cc.debug_mode = true;
	cc.optimization = POOF_O0;
	cc.output = "demos/rend2/demo";
	poof_cmd_append(&cc.inputs, "demos/rend2/demo.c");
	poof_cmd_append(&cc.includes, ".");
	poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wextra", "-Werror");
	add_peak_config(&cc);
	return poof_cc_run(&cc);
}

int
main(int argc, char **argv)
{
	const char *name;

	POOF_GO_REBUILD_URSELF(argc, argv);
	if (argc != 3 || strcmp(argv[1], "demo")) {
		fprintf(stderr, "usage: ./build demo <name>\n");
		return 1;
	}
	name = argv[2];
	if (!strcmp(name, "cast")) return build_cast_demo() ? 0 : 1;
	if (!strcmp(name, "fuse")) return build_fuse_demo() ? 0 : 1;
	if (!strcmp(name, "grit")) return build_grit_demo() ? 0 : 1;
	if (!strcmp(name, "peak")) return build_peak_demo() ? 0 : 1;
	if (!strcmp(name, "multiplatform")) return build_multiplatform_demo() ? 0 : 1;
	if (!strcmp(name, "doc-generator")) return build_doc_generator_demo() ? 0 : 1;
	if (!strcmp(name, "compute")) return build_compute_demo() ? 0 : 1;
	if (!strcmp(name, "teapot")) return build_teapot_demo() ? 0 : 1;
	if (!strcmp(name, "dashboard")) return build_dashboard_demo() ? 0 : 1;
	if (!strcmp(name, "rend")) return build_rend_demo() ? 0 : 1;
	if (!strcmp(name, "rend-abi")) return build_rend_abi_demo() ? 0 : 1;
	if (!strcmp(name, "rend2")) return build_rend2_demo() ? 0 : 1;
	if (!strcmp(name, "snake")) return build_snake() ? 0 : 1;
	fprintf(stderr, "unknown demo: %s\n", name);
	fprintf(stderr, "demos: cast fuse grit peak multiplatform doc-generator compute teapot dashboard rend rend-abi rend2 snake\n");
	return 1;
}
