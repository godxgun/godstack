#include "Poof.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
add_peak_link(Poof_CC *cc)
{
    poof_cc_append_linux(cc, "-ldl", "-lpthread", "-lutil");
    poof_cc_append_macos(cc, "-framework", "AppKit", "-framework", "AudioToolbox",
        "-framework", "QuartzCore", "-framework", "CoreGraphics", "-framework", "GameController");
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
    poof_cmd_append(&cc.libs, "m");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
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
		fprintf(stderr, "Snake requires Slang and SPIRV-Tools\n");
		return 0;
	}
	poof_mkdir("bin");
	for (i = 0; i < sizeof(shaders) / sizeof(*shaders); i++) {
		Poof_Cmd cmd = {0};
		int ok;

		poof_cmd_append(&cmd, "slangc", "demos/snake/shaders/snake.slang", "-target", "spirv", "-profile", "spirv_1_5",
			"-emit-spirv-directly", "-fvk-use-c-layout", "-matrix-layout-column-major",
			"-entry", shaders[i].entry, "-stage", shaders[i].stage,
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
	if (!strcmp(name, "grit")) return build_grit_demo() ? 0 : 1;
	if (!strcmp(name, "peak")) return build_peak_demo() ? 0 : 1;
	if (!strcmp(name, "multiplatform")) return build_multiplatform_demo() ? 0 : 1;
	if (!strcmp(name, "doc-generator")) return build_doc_generator_demo() ? 0 : 1;
	if (!strcmp(name, "compute")) return build_compute_demo() ? 0 : 1;
	if (!strcmp(name, "teapot")) return build_teapot_demo() ? 0 : 1;
	if (!strcmp(name, "dashboard")) return build_dashboard_demo() ? 0 : 1;
	if (!strcmp(name, "rend")) return build_rend_demo() ? 0 : 1;
	if (!strcmp(name, "rend-abi")) return build_rend_abi_demo() ? 0 : 1;
	if (!strcmp(name, "snake")) return build_snake() ? 0 : 1;
	if (!strcmp(name, "snake-web")) {
		Poof_Cmd cmd = {0};
		int ok;
		poof_cmd_append(&cmd, "python3", "tools/build_web_demos.py", "snake");
		ok = poof_cmd_run(&cmd);
		poof_cmd_free(&cmd);
		return ok ? 0 : 1;
	}
	if (!strcmp(name, "grit-web")) {
		Poof_Cmd cmd = {0};
		int ok;
		poof_cmd_append(&cmd, "python3", "tools/build_web_demos.py", "grit");
		ok = poof_cmd_run(&cmd);
		poof_cmd_free(&cmd);
		return ok ? 0 : 1;
	}
	fprintf(stderr, "unknown demo: %s\n", name);
	fprintf(stderr, "demos: grit peak multiplatform doc-generator compute teapot dashboard rend rend-abi snake snake-web grit-web\n");
	return 1;
}
