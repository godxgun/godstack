#include "Poof/poof.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int build_rend_abi_demo(void);
static int build_rend2_demo(void);
static int build_snake(void);
static int package_version(const char *header, const char *prefix, char version[64]);
static int package_header(const char *src, const char *dst, const char *peak_header);
static int build_package(void);

static void
add_peak_config(Poof_CC *cc)
{
    poof_cmd_append(&cc->includes, "Peak");

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
    poof_cmd_append(&cc.includes, ".", "Rend", "Peak", "Fuse", "Grit");
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
    poof_cmd_append(&cc.includes, "Peak");
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
    poof_cmd_append(&cc.includes, ".", "Peak");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror", "-Wno-deprecated-declarations");
    add_peak_config(&cc);
    add_peak_link(&cc);
    return poof_cc_run(&cc);
}

static bool
build_peak_demos(void)
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
    poof_cmd_append(&cc.includes, ".", "Fuse");
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
    poof_cmd_append(&cc.includes, ".", "Grit");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_cool_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/doc-generator");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/doc-generator/doc_generator";
    poof_cmd_append(&cc.inputs, "demos/doc-generator/doc_generator.c");
    poof_cmd_append(&cc.includes, ".", "Cool", "Wire");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_rend_demos(void)
{
    poof_mkdir("demos/compute");
    poof_mkdir("demos/teapot");
    poof_mkdir("demos/snake");
    poof_mkdir("demos/dashboard");

    Poof_Batch batch = {0};

    slangc_entry(&batch, "demos/compute/rend_compute.slang", "particleMain", "compute", "demos/compute/particle.spv");
    slangc_entry(&batch, "demos/compute/rend_compute.slang", "sandMain", "compute", "demos/compute/sand.spv");
    slangc_entry(&batch, "demos/compute/rend_compute.slang", "vertMain", "vertex", "demos/compute/vert.spv");
    slangc_entry(&batch, "demos/compute/rend_compute.slang", "fragMain", "fragment", "demos/compute/frag.spv");

    slangc_entry(&batch, "demos/teapot/shaders/basic.slang", "vertMain", "vertex", "demos/teapot/basic.vert.spv");
    slangc_entry(&batch, "demos/teapot/shaders/basic.slang", "fragMain", "fragment", "demos/teapot/basic.frag.spv");
    slangc_entry(&batch, "demos/teapot/shaders/texture.slang", "vertMain", "vertex", "demos/teapot/texture.vert.spv");
    slangc_entry(&batch, "demos/teapot/shaders/texture.slang", "fragMain", "fragment", "demos/teapot/texture.frag.spv");

    add_rend_demo(&batch, "demos/compute/rend_compute.c", "demos/compute/rend_compute_demo", POOF_O0, "REND_DEBUG");
    add_rend_demo(&batch, "demos/teapot/rend_teapot.c", "demos/teapot/rend_teapot", POOF_O0, "REND_DEBUG");

    slangc_entry(&batch, "demos/dashboard/fuse_ui.slang", "vertMain", "vertex", "demos/dashboard/fuse_ui.vert.spv");
    slangc_entry(&batch, "demos/dashboard/fuse_ui.slang", "fragMain", "fragment", "demos/dashboard/fuse_ui.frag.spv");
    slangc_entry(&batch, "demos/dashboard/fuse_text.slang", "vertMain", "vertex", "demos/dashboard/fuse_text.vert.spv");
    slangc_entry(&batch, "demos/dashboard/fuse_text.slang", "fragMain", "fragment", "demos/dashboard/fuse_text.frag.spv");
    add_rend_demo(&batch, "demos/dashboard/dashboard.c", "demos/dashboard/dashboard", POOF_O0, "REND_DEBUG");

    if (!poof_batch_run(&batch, "Rend Demos")) return 0;
    return build_snake();
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
    poof_cmd_append(&cc.includes, ".", "Cast");
    poof_cmd_append(&cc.defines, "CAST_DEBUG");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_cool_transpiler(void)
{
    Poof_CC cc = {0};
    poof_mkdir("bin");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "bin/cool_transpiler";
    poof_cmd_append(&cc.inputs, "Cool/cool_transpiler.c");
    poof_cmd_append(&cc.includes, "Cast");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_codeanalizer(void)
{
    Poof_CC cc = {0};

    poof_mkdir("tools");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "tools/codeanalizer";
    poof_cmd_append(&cc.inputs, "tools/codeanalizer.c");
    poof_cmd_append(&cc.includes, ".", "Cast", "Peak", "Rend");
    poof_cmd_append(&cc.libs, "m");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror", "-Wno-deprecated-declarations");
    add_peak_link(&cc);
    poof_cc_append_linux(&cc, "-ldl");
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
    poof_cmd_append(&cc.includes, ".", "Rend", "Peak", "Fuse", "Grit");
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

static bool
run_one(Poof_Cmd *cmd)
{
#if defined(_WIN32)
    Poof_Cmd wrapped = {0};
    bool ok;
    poof_cmd_append(&wrapped, "cmd.exe", "/c");
    for (size_t i = 0; i < cmd->count; ++i)
        poof_cmd_append(&wrapped, cmd->items[i]);
    ok = poof_cmd_run(&wrapped);
    poof_cmd_free(&wrapped);
    poof_cmd_free(cmd);
    return ok;
#else
    bool ok = poof_cmd_run(cmd);
    poof_cmd_free(cmd);
    return ok;
#endif
}

static void
append_demo(Poof_Cmd *cmd, const char *path)
{
#if defined(_WIN32)
    char exe[256];
    size_t length = strlen(path);
    if (length + sizeof(".exe") > sizeof(exe)) {
        fprintf(stderr, "demo executable path too long: %s\n", path);
        exit(1);
    }
    memcpy(exe, path, length);
    memcpy(exe + length, ".exe", sizeof(".exe"));
    poof_cmd_append(cmd, exe);
#else
    poof_cmd_append(cmd, path);
#endif
}

static bool
run_demos(void)
{
    Poof_Cmd cmd = {0};
    const char *cpu[] = {"demos/cast/demo", "demos/fuse/demo", "demos/grit/demo", "demos/peak/demo"};
    size_t i;
    for (i = 0; i < sizeof(cpu) / sizeof(*cpu); ++i) {
        append_demo(&cmd, cpu[i]);
        if (!run_one(&cmd)) return false;
    }
    append_demo(&cmd, "demos/doc-generator/doc_generator");
#if defined(_WIN32)
    poof_cmd_append(&cmd, "Cool/cool.h", ">", "NUL");
#else
    poof_cmd_append(&cmd, "Cool/cool.h", ">", "/dev/null");
#endif
    if (!run_one(&cmd)) return false;
    append_demo(&cmd, "demos/compute/rend_compute_demo");
    poof_cmd_append(&cmd, "--headless");
    if (!run_one(&cmd)) return false;
    append_demo(&cmd, "demos/teapot/rend_teapot");
    poof_cmd_append(&cmd, "--headless");
    if (!run_one(&cmd)) return false;
    append_demo(&cmd, "demos/snake/snake");
    poof_cmd_append(&cmd, "--headless");
    if (!run_one(&cmd)) return false;
    append_demo(&cmd, "demos/dashboard/dashboard");
    poof_cmd_append(&cmd, "--headless");
    return run_one(&cmd);
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
	poof_cmd_append(&cc.includes, ".", "Rend2", "Peak", "Grit");
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
	poof_cmd_append(&cc.includes, ".", "Rend2");
	poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wextra", "-Werror");
	add_peak_config(&cc);
	return poof_cc_run(&cc);
}

int
package_version(const char *header, const char *prefix, char version[64])
{
	const char *parts[] = {"MAJOR", "MINOR", "PATCH"};
	char values[3][16] = {{0}}, line[1024], key[128], value[128], wanted[128];
	FILE *file;
	int i;

	if (!(file = fopen(header, "r"))) return 0;
	while (fgets(line, sizeof(line), file)) {
		if (sscanf(line, "#define %127s %127s", key, value) != 2) continue;
		for (i = 0; i < 3; i++) {
			const char *p = value;
			size_t n = 0;
			snprintf(wanted, sizeof(wanted), "%s_%s", prefix, parts[i]);
			if (strcmp(key, wanted)) continue;
			if (*p == '"') p++;
			while (p[n] >= '0' && p[n] <= '9') n++;
			if (!n || n >= sizeof(values[i]) || (p[n] && p[n] != '"')) continue;
			memcpy(values[i], p, n);
			values[i][n] = 0;
		}
	}
	i = !ferror(file) && values[0][0] && values[1][0] && values[2][0];
	fclose(file);
	if (!i) {
		fprintf(stderr, "Cannot read package version from %s\n", header);
		return 0;
	}
	snprintf(version, 64, "%s.%s.%s", values[0], values[1], values[2]);
	return 1;
}

static int
package_header(const char *src, const char *dst, const char *peak_header)
{
	FILE *in, *out;
	char line[4096];
	int ok = 1;

	if (!(in = fopen(src, "r"))) return 0;
	if (!(out = fopen(dst, "w"))) {
		fclose(in);
		return 0;
	}
	while (fgets(line, sizeof(line), in)) {
		/* The source-relative dependency must resolve inside the flat package. */
		if (strstr(line, "#include \"../Peak/peak.h\"")) {
			if (fprintf(out, "#include \"%s\"\n", peak_header) < 0) ok = 0;
		} else if (fputs(line, out) == EOF) {
			ok = 0;
		}
	}
	if (ferror(in)) ok = 0;
	fclose(in);
	if (fclose(out)) ok = 0;
	return ok;
}

static int
build_package(void)
{
	const char *dirs[] = {"Peak", "Fuse", "Rend", "Type"};
	const char *names[] = {"peak", "fuse", "rend", "type"};
	const char *prefixes[] = {"PEAK", "FUSE", "REND", "TYPE"};
	char versions[4][64], root[128], bin[160], include[160], date[32], hash[64];
	char header[128], source[128], output[256], dest[256];
	char peak_header[128];
	FILE *git;
	time_t now = time(NULL);
	struct tm *today = localtime(&now);
	int i, ok;

	if (!today || !strftime(date, sizeof(date), "%Y-%m-%d", today)) return 0;
#if defined(_WIN32)
	git = _popen("git rev-parse --short HEAD", "r");
#else
	git = popen("git rev-parse --short HEAD", "r");
#endif
	if (!git) return 0;
	ok = fgets(hash, sizeof(hash), git) != NULL;
#if defined(_WIN32)
	if (_pclose(git) != 0) ok = 0;
#else
	if (pclose(git) != 0) ok = 0;
#endif
	if (ok) hash[strcspn(hash, "\r\n")] = 0;
	if (!ok || !hash[0] || strspn(hash, "0123456789abcdef") != strlen(hash)) {
		fprintf(stderr, "Cannot read package commit hash from Git\n");
		return 0;
	}
	for (i = 0; i < 4; i++) {
		snprintf(header, sizeof(header), "%s/%s.h", dirs[i], names[i]);
		if (!package_version(header, prefixes[i], versions[i])) return 0;
	}
	snprintf(root, sizeof(root), "package/%s-%s", date, hash);
	snprintf(bin, sizeof(bin), "%s/bin", root);
	snprintf(include, sizeof(include), "%s/include", root);
	snprintf(peak_header, sizeof(peak_header), "peak-%s.h", versions[0]);
	if (!poof_mkdir("package") || !poof_mkdir(root) || !poof_mkdir(bin) || !poof_mkdir(include)) return 0;
	for (i = 0; i < 4; i++) {
		Poof_CC cc = {0};
		snprintf(source, sizeof(source), "%s/%s.c", dirs[i], names[i]);
		snprintf(header, sizeof(header), "%s/%s.h", dirs[i], names[i]);
		snprintf(output, sizeof(output), "%s/%s-%s.o", bin, names[i], versions[i]);
		poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
		cc.optimization = POOF_O2;
		cc.debug_mode = false;
		cc.output = output;
		poof_cmd_append(&cc.inputs, source);
		poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-c");
#if defined(__APPLE__)
		poof_cmd_append(&cc.extra_flags, "-Wno-deprecated-declarations");
#elif !defined(_WIN32)
		poof_cmd_append(&cc.defines, "_POSIX_C_SOURCE=200809L");
#endif
		if (i == 0 || i == 2) {
			poof_cmd_append(&cc.defines, "PEAK_VULKAN");
			add_peak_config(&cc);
		}
		ok = poof_cc_run(&cc);
		poof_cc_free(&cc);
		if (!ok) return 0;
		snprintf(dest, sizeof(dest), "%s/%s-%s.h", include, names[i], versions[i]);
		if (!package_header(header, dest, peak_header)) return 0;
	}
	snprintf(dest, sizeof(dest), "%s/poof.h", include);
	if (!poof_copy_file("Poof/poof.h", dest)) return 0;
	snprintf(dest, sizeof(dest), "%s/LICENSE", root);
	if (!poof_copy_file("LICENSE", dest)) return 0;
	printf("Package ready: %s\n", root);
	return 1;
}

int
main(int argc, char **argv)
{
    int run = 0, cpu = 0, peak = 0, rend = 0, rend2 = 0, snake = 0, abi = 0, tools = 0;
    int i;
    POOF_GO_REBUILD_URSELF(argc, argv);
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "package")) continue;
        if (argc != 2) {
            fprintf(stderr, "usage: ./build package\n");
            return 1;
        }
        return build_package() ? 0 : 1;
    }
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "run") || !strcmp(argv[i], "test")) run = 1;
        else if (!strcmp(argv[i], "cpu")) cpu = 1;
        else if (!strcmp(argv[i], "peak")) peak = 1;
        else if (!strcmp(argv[i], "rend")) rend = 1;
        else if (!strcmp(argv[i], "rend2")) rend2 = 1;
        else if (!strcmp(argv[i], "snake")) snake = 1;
        else if (!strcmp(argv[i], "abi")) abi = 1;
        else if (!strcmp(argv[i], "tools")) tools = 1;
        else {
            fprintf(stderr, "unknown build option: %s\n", argv[i]);
            return 1;
        }
    }
    if (cpu + peak + rend + rend2 + snake + tools > 1 || (abi && !rend)) {
        fprintf(stderr, "usage: ./build [cpu|peak|rend [abi]|rend2|snake|tools] [run]\n");
        return 1;
    }
    if (tools) return build_codeanalizer() ? 0 : 1;
#if defined(_WIN32)
    /* Runtime demo dispatch depends on a POSIX-compatible command shell. */
    if (run && (cpu || peak || rend || rend2 || snake || abi)) {
        fprintf(stderr, "runtime demo execution currently requires a POSIX shell on Windows\n");
        return 1;
    }
#endif
    if (cpu || peak) {
        if (peak && !build_peak_demos()) return 1;
        if (!build_peak_demo()) return 1;
        if (cpu && (!build_cast_demo() || !build_fuse_demo() || !build_grit_demo())) return 1;
        if (run) {
            Poof_Cmd cmd = {0};
            if (cpu) {
                const char *bins[] = {"demos/cast/demo", "demos/fuse/demo", "demos/grit/demo"};
                for (i = 0; i < 3; ++i) { append_demo(&cmd, bins[i]); if (!run_one(&cmd)) return 1; }
            }
            append_demo(&cmd, "demos/peak/demo");
            if (!run_one(&cmd)) return 1;
        }
        return 0;
    }
    if (snake) {
        if (!build_snake()) return 1;
        if (run) { Poof_Cmd cmd = {0}; poof_cmd_append(&cmd, "python3", "demos/snake/stress.py"); return run_one(&cmd) ? 0 : 1; }
        return 0;
    }
    if (rend2) {
        if (!build_rend2_demo()) return 1;
        if (run) { Poof_Cmd cmd = {0}; append_demo(&cmd, "demos/rend2/demo"); return run_one(&cmd) ? 0 : 1; }
        return 0;
    }
    if (rend && abi) {
        if (!build_rend_abi_demo()) return 1;
        if (run) { Poof_Cmd cmd = {0}; poof_cmd_append(&cmd, "python3", "demos/rend_abi.py"); return run_one(&cmd) ? 0 : 1; }
        return 0;
    }
    if (rend) {
        if (!build_rend_demo()) return 1;
        if (run) { Poof_Cmd cmd = {0}; append_demo(&cmd, "demos/rend/demo"); return run_one(&cmd) ? 0 : 1; }
        return 0;
    }
    if (!build_peak_demos() || !build_peak_demo() || !build_cast_demo() || !build_fuse_demo() ||
        !build_grit_demo() || !build_cool_demo() || !build_rend_demos() || !build_cool_transpiler() ||
        !build_rend_demo() || !build_rend2_demo()) return 1;
    if (run) {
        if (!run_demos()) return 1;
        Poof_Cmd cmd = {0};
        append_demo(&cmd, "demos/rend/demo");
        if (!run_one(&cmd)) return 1;
        append_demo(&cmd, "demos/rend2/demo");
        return run_one(&cmd) ? 0 : 1;
    }
    return 0;
}
