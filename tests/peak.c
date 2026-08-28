/* Peak stress test. Headless filesystem/proc first; window if peak_init works. */

#include "peak.h"
#include "../Peak/peak.c"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static int g_fails;

static void expect(int ok, const char *what);
static void test_time(void);
static void test_file(void);
static void test_fs(void);
static void test_mirror(void);
static void test_job(void);
static void test_pty(void);
static void test_sock(void);
static void test_wait(void);
static void test_clip(void);
static void test_log_mem(void);
static void test_window(void);
static void test_load(void);
static int tick_stop(PeakWindow *win, void *ud);

void
expect(int ok, const char *what)
{
	if (ok) {
		printf("  ok   %s\n", what);
		return;
	}
	printf("  FAIL %s\n", what);
	g_fails++;
}

void
test_time(void)
{
	uint64_t a, b;

	printf("time\n");
	a = peak_get_time();
	peak_sleep_ns(1000000);
	b = peak_get_time();
	expect(b >= a, "monotonic");
}

void
test_file(void)
{
	const char *path;
	unsigned long n = 0;
	void *p;
	const char *msg = "peak";

	printf("file\n");
	path = "tests/peak.c";
	expect(peak_file_exists(path) == 1, "exists");
	expect(peak_file_exists("no/such/peak/file") == 0, "missing");
	expect(peak_file_exists(NULL) == 0, "null path");
	p = peak_file_alloc(path, &n);
	expect(p != NULL && n > 0, "alloc");
	free(p);
	expect(peak_file_write("tests/peak_tmp.bin", msg, 4) == 1, "write");
	expect(peak_file_exists("tests/peak_tmp.bin") == 1, "wrote exists");
	peak_filesystem_rm("tests/peak_tmp.bin");
}

void
test_fs(void)
{
	char cwd[512];
	char here[512];

	printf("filesystem\n");
	expect(peak_filesystem_cwd(here, sizeof here) == 1, "cwd");
	expect(peak_filesystem_mkdir("tests/peak_dir") == 1, "mkdir");
	expect(peak_filesystem_mkdir("tests/peak_dir") == 0, "mkdir exists");
	expect(peak_file_write("tests/peak_dir/a.txt", "x", 1) == 1, "write in dir");
	expect(peak_filesystem_rename("tests/peak_dir/a.txt", "tests/peak_dir/b.txt") == 1, "rename");
	expect(peak_filesystem_rm("tests/peak_dir") == 0, "rm nonempty");
	expect(peak_filesystem_chdir("tests/peak_dir") == 1, "chdir");
	expect(peak_filesystem_cwd(cwd, sizeof cwd) == 1, "cwd nested");
	expect(peak_filesystem_chdir(here) == 1, "chdir back");
	expect(peak_filesystem_rm("tests/peak_dir/b.txt") == 1, "rm file");
	expect(peak_filesystem_rm("tests/peak_dir") == 1, "rm dir");
	expect(peak_filesystem_mkdir(NULL) == 0, "mkdir null");
	expect(peak_filesystem_rm(NULL) == 0, "rm null");
}

void
test_mirror(void)
{
	size_t pg;
	char *p;

	printf("mirror\n");
	pg = peak_page_size();
	expect(pg >= 4096 && (pg & (pg - 1)) == 0, "page size");
	expect(peak_mirror_map(0) == NULL, "map 0");
	expect(peak_mirror_map(pg - 1) == NULL, "unaligned");
	p = peak_mirror_map(pg);
	expect(p != NULL, "map");
	if (p) {
		p[0] = 42;
		expect(p[pg] == 42, "wrap");
		p[pg] = 7;
		expect(p[0] == 7, "wrap back");
		peak_mirror_unmap(p, pg);
	}
}

void
test_job(void)
{
	PeakProc job;
	int code = -1;
	int i;
	char buf[64];
	int n;

	printf("job\n");
	job = peak_job_run("", NULL);
	expect(job.fd == PEAK_HANDLE_INVALID, "empty cmd");
	job = peak_job_run("exit 0", NULL);
	expect(job.pid > 0, "run");
	for (i = 0; i < 200; i++) {
		n = peak_fd_read(job.fd, buf, sizeof buf);
		(void)n;
		if (peak_job_reap(&job, &code))
			break;
		peak_sleep_ns(10000000);
	}
	expect(code == 0, "exit 0");
	peak_job_kill(&job);
	job = peak_job_run("sleep 30", NULL);
	expect(job.pid > 0, "sleeper");
	peak_job_kill(&job);
	expect(job.pid == 0, "killed");
}

void
test_pty(void)
{
	PeakProc pty;
	const char *argv[4];
	int i;

	printf("pty\n");
	pty = peak_pty_spawn(NULL, NULL, 80, 24, 0, 0);
	expect(pty.fd == PEAK_HANDLE_INVALID, "null spawn");
	argv[0] = "sh";
	argv[1] = "-c";
	argv[2] = "exit 0";
	argv[3] = NULL;
	pty = peak_pty_spawn("sh", argv, 80, 24, 0, 0);
	if (pty.fd == PEAK_HANDLE_INVALID) {
		printf("  skip  pty\n");
		return;
	}
	expect(pty.pid > 0, "pty pid");
	peak_pty_resize(&pty, 40, 12, 0, 0);
	for (i = 0; i < 50; i++) {
		if (peak_pty_reap(&pty))
			break;
		peak_sleep_ns(10000000);
	}
	peak_pty_close(&pty);
	expect(pty.fd == PEAK_HANDLE_INVALID, "pty close");
}

void
test_sock(void)
{
	char dir[256];
	char path[300];
	PEAK_HANDLE listen, a, b;
	char buf[8];
	int n;

	printf("sock\n");
	expect(peak_runtime_dir(dir, sizeof dir, "peak-test") == 1, "runtime dir");
	snprintf(path, sizeof path, "%s/s", dir);
	listen = peak_sock_listen(path);
	expect(listen != PEAK_HANDLE_INVALID, "listen");
	b = peak_sock_connect(path);
	expect(b != PEAK_HANDLE_INVALID, "connect");
	a = peak_sock_accept(listen);
	expect(a != PEAK_HANDLE_INVALID, "accept");
	expect(peak_fd_write(b, "hi", 2) > 0, "write");
	n = peak_fd_read(a, buf, sizeof buf);
	expect(n == 2 && buf[0] == 'h' && buf[1] == 'i', "read");
	peak_fd_close(a);
	peak_fd_close(b);
	peak_fd_close(listen);
	expect(peak_fd_read(PEAK_HANDLE_INVALID, buf, 1) == 0, "invalid read");
}

void
test_wait(void)
{
	int r;

	printf("wait\n");
	r = peak_wait(NULL, NULL, 0, 0);
	expect(r == 0, "wait empty poll");
	r = peak_wait(NULL, NULL, 0, 1);
	expect(r == 0, "wait empty timeout");
}

void
test_clip(void)
{
	char buf[16];
	size_t n = 0;
	char *big;

	printf("clip\n");
	expect(peak_clip_set(NULL, (PeakClip)99, "x", 1) == 0, "bad which");
	expect(peak_clip_set(NULL, PEAK_CLIP_CLIPBOARD, "abc", 3) == 1, "set");
	expect(peak_clip_request(NULL, PEAK_CLIP_CLIPBOARD) == 1, "request");
	expect(peak_clip_take(NULL, buf, sizeof buf, &n) == 1 && n == 3, "take");
	expect(peak_text_take(NULL, buf, sizeof buf, &n) == 0, "text empty");
	expect(peak_drop_take(NULL, buf, sizeof buf, &n) == 0, "drop empty");
	big = malloc(1024u * 1024u);
	if (big) {
		memset(big, 'a', 1024u * 1024u);
		expect(peak_clip_set(NULL, PEAK_CLIP_CLIPBOARD, big, 1024u * 1024u) == 1, "1MiB");
		free(big);
	}
}

void
test_log_mem(void)
{
	void *p;

	printf("log mem\n");
	PINFO("peak test");
	p = peak_debug_malloc_impl(32, __FILE__, __LINE__, __func__);
	expect(p != NULL, "debug malloc");
	p = peak_debug_realloc_impl(p, 64, __FILE__, __LINE__, __func__);
	expect(p != NULL, "debug realloc");
	peak_debug_free_impl(p, __FILE__, __LINE__, __func__);
	peak_debug_memory_report();
}

int
tick_stop(PeakWindow *win, void *ud)
{
	int *n;

	n = ud;
	(*n)++;
	(void)win;
	return *n < 3;
}

void
test_window(void)
{
	PeakWindow w, w2;
	PeakEvent ev;
	size_t width = 0, height = 0;
	uint32_t *buf;
	int ticks;
	int i;

	printf("window\n");
	if (!peak_init()) {
		printf("  skip  no display\n");
		return;
	}
	w = peak_window_open("peak-test", 320, 240, 0);
	expect(w.internal.w != NULL, "open");
	buf = peak_window_backbuffer(&w, &width, &height);
	expect(buf != NULL && width == 320 && height == 240, "buffer");
	peak_window_clear(&w, 1, 0, 0, 1);
	peak_window_present(&w);
	while (peak_window_epoll(&w, &ev))
		;
	expect(peak_window_pending(&w) >= 0, "pending");
	peak_window_set_title(&w, "peak-test-2");
	peak_window_set_size(&w, 400, 300);
	peak_window_cursor(&w, 0);
	peak_window_cursor(&w, 1);
	peak_window_pointer_relative(&w, 1);
	peak_window_pointer_relative(&w, 0);
	expect(peak_window_scale(&w) >= 1.f, "scale");
	peak_window_fullscreen(&w, 1);
	peak_window_fullscreen(&w, 0);
	w2 = peak_window_open("peak-test-b", 160, 120, PEAK_WINDOW_TRANSPARENT);
	expect(w2.internal.w != NULL, "open 2");
	peak_window_present(&w2);
	peak_window_close(&w2);
	ticks = 0;
	peak_window_run(&w, tick_stop, &ticks);
	expect(ticks == 3, "run ticks");
	for (i = 0; i < 16; i++) {
		w2 = peak_window_open("cycle", 64, 64, 0);
		if (w2.internal.w)
			peak_window_present(&w2);
		peak_window_close(&w2);
	}
	peak_window_close(&w);
	peak_quit();
}

static void
audio_zero(int16_t *out, size_t frames, void *ud)
{
	(void)ud;
	memset(out, 0, frames * 2 * sizeof *out);
}

void
test_load(void)
{
	int i;
	int n;
	PeakProc job;
	size_t pg;
	void *p;

	printf("load\n");
	pg = peak_page_size();
	n = 0;
	for (i = 0; i < 32; i++) {
		p = peak_mirror_map(pg);
		if (p) {
			n++;
			peak_mirror_unmap(p, pg);
		}
	}
	expect(n == 32, "mirror x32");
	n = 0;
	for (i = 0; i < 64; i++) {
		job = peak_job_run("exit 0", NULL);
		if (job.pid > 0)
			n++;
		peak_job_kill(&job);
	}
	expect(n == 64, "job x64");
	for (i = 0; i < 256; i++)
		peak_wait(NULL, NULL, 0, 0);
	expect(1, "wait x256");
	n = 0;
	for (i = 0; i < 32; i++) {
		char name[64];
		snprintf(name, sizeof name, "tests/peak_ld_%d", i);
		if (peak_filesystem_mkdir(name) && peak_filesystem_rm(name))
			n++;
	}
	expect(n == 32, "mkdir/rm x32");
}

int
main(void)
{
	test_time();
	test_file();
	test_fs();
	test_mirror();
	test_job();
	test_pty();
	test_sock();
	test_wait();
	test_clip();
	test_log_mem();
	if (peak_audio_start(2, 48000, audio_zero, NULL)) {
		printf("audio\n");
		expect(1, "start");
		peak_audio_stop();
	} else {
		printf("audio\n  skip  no device\n");
	}
	test_load();
	test_window();
	if (g_fails) {
		printf("%d failed\n", g_fails);
		return 1;
	}
	printf("all ok\n");
	return 0;
}
