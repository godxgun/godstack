/* Poof output naming regression tests; command generation only, no cross compiler. */
/* Poof sets POSIX feature macros before libc headers. */
#include "../Poof/poof.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_output(unsigned compiler, unsigned target, const char *output, const char *flag, int platform_flag, const char *expected);

static void
test_output(unsigned compiler, unsigned target, const char *output, const char *flag, int platform_flag, const char *expected)
{
	Poof_CC cc = {0};
	Poof_Batch batch = {0};
	int found = 0;

	cc.compiler = compiler;
	cc.target_platform = target;
	cc.output = output;
	poof_cmd_append(&cc.inputs, "consumer.c");
	if (flag) poof_cmd_append(platform_flag ? &cc.win32_flags : &cc.extra_flags, flag);
	poof_batch_append_cc(&batch, &cc);
	assert(batch.count > 0);
	for (size_t i = 0; i < batch.count; i++) {
		Poof_Cmd *cmd = &batch.cmds[i];
		for (size_t j = 0; j < cmd->count; j++) {
			if (compiler == POOF_CC_MSVC) {
				if (!strcmp(cmd->items[j], expected)) found++;
			} else if (!strcmp(cmd->items[j], "-o")) {
				assert(j + 1 < cmd->count);
				if (!strcmp(cmd->items[j + 1], expected)) found++;
			}
		}
	}
	assert(found == 1);
	poof_batch_free(&batch);
	poof_cc_free(&cc);
}

int
main(void)
{
	test_output(POOF_CC_GCC, POOF_TARGET_WIN32, "peak.o", "-c", 0, "peak.o");
	test_output(POOF_CC_CLANG, POOF_TARGET_WIN32, "peak.o", "-c", 0, "peak.o");
	test_output(POOF_CC_GCC, POOF_TARGET_WIN32, "peak.o", "-c", 1, "peak.o");
	test_output(POOF_CC_GCC, POOF_TARGET_WIN32, "consumer", NULL, 0, "consumer.exe");
	test_output(POOF_CC_GCC, POOF_TARGET_WIN32, "consumer.exe", NULL, 0, "consumer.exe");
	test_output(POOF_CC_GCC, POOF_TARGET_LINUX, "peak.o", "-c", 0, "peak.o");
	test_output(POOF_CC_GCC, POOF_TARGET_MACOS, "peak.o", "-c", 0, "peak.o");
	test_output(POOF_CC_MSVC, POOF_TARGET_WIN32, "peak.obj", "/c", 0, "/Fo:peak.obj");
	test_output(POOF_CC_MSVC, POOF_TARGET_WIN32, "consumer", NULL, 0, "/Fe:consumer.exe");
	test_output(POOF_CC_GCC, POOF_TARGET_WIN32 | POOF_TARGET_LINUX, "peak.o", "-c", 0, "peak.o_win32");
	/* A Windows-only -c must not change a Linux command into compile-only. */
	test_output(POOF_CC_MSVC, POOF_TARGET_LINUX, "consumer", "/c", 1, "/Fe:consumer");
	puts("Poof output naming: PASS");
	return 0;
}
