/* Standalone public-API stress workload for Peak's safe-I/O primitives.
 * Linux only: the API intentionally reports unsupported elsewhere. */
#define _POSIX_C_SOURCE 200809L
#define PEAK_NO_AUDIO
#define PEAK_NO_GAMEPAD
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include "Peak.h"
#define PEAK_IMPLEMENTATION
#include "Peak.h"

#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			fprintf(stderr, "safe-io demo:%d: %s\n", __LINE__, #x);                                \
			goto done;                                                                             \
		}                                                                                          \
	} while (0)
#define RACE_LOOPS 12000

typedef struct {
	char root[512], external[512], nested[512], link[512];
	pthread_mutex_t mutex;
	int run;
} Race;
static void *swapper(void *arg)
{
	Race *r = (Race *)arg;
	int run;
	do {
		pthread_mutex_lock(&r->mutex);
		run = r->run;
		pthread_mutex_unlock(&r->mutex);
		if (!run)
			break;
		(void)unlink(r->link);
		(void)symlink(r->external, r->link);
		(void)unlink(r->link);
		(void)symlink("nested-real", r->link);
	} while (run);
	return NULL;
}

static int remove_path(const char *path, int directory)
{
	int rc;
	if (!path || !path[0])
		return 1;
	rc = directory ? rmdir(path) : unlink(path);
	return rc == 0 || errno == ENOENT;
}

static int write_host(const char *path, const char *data)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	size_t n = strlen(data), done = 0;
	if (fd < 0)
		return 0;
	while (done < n) {
		ssize_t k = write(fd, data + done, n - done);
		if (k <= 0) {
			close(fd);
			return 0;
		}
		done += (size_t)k;
	}
	return close(fd) == 0;
}

static int read_peak(const PeakIORoot *root, const char *path, const char *expect)
{
	PeakIOFile f = PEAK_IO_FILE_INIT;
	char b[64];
	size_t n = 0;
	PeakIOResult x = peak_io_file_open(root, path, sizeof(b), &f);
	if (x.code != PEAK_IO_OK)
		return 0;
	x = peak_io_file_read(&f, 0, b, sizeof(b), &n);
	(void)peak_io_file_close(&f);
	return x.code == PEAK_IO_OK && n == strlen(expect) && !memcmp(b, expect, n);
}

static int remove_fixture(const Race *race)
{
	const char *directories[] = {race->nested, race->external};
	const char *files[] = {"existing", "stage-collision", "stage-fail", "stage-abort",
	                       "stage-commit"};
	char path[1024];
	int ok = 1;
	size_t i;
	if (!race->root[0])
		return 1;
	ok &= remove_path(race->link, 0);
	for (i = 0; i < sizeof directories / sizeof directories[0]; i++) {
		snprintf(path, sizeof path, "%s/item", directories[i]);
		ok &= remove_path(path, 0);
		ok &= remove_path(directories[i], 1);
	}
	for (i = 0; i < sizeof files / sizeof files[0]; i++) {
		snprintf(path, sizeof path, "%s/%s", race->root, files[i]);
		ok &= remove_path(path, 0);
	}
	ok &= remove_path(race->root, 1);
	return ok;
}

int main(void)
{
	char temp[] = "/tmp/peak-safe-io.XXXXXX";
	char p[1024], file[512], data[64];
	Race race;
	pthread_t thread;
	int threaded = 0, mutex_ready = 0, ok = 0, i;
	PeakIORoot root = PEAK_IO_ROOT_INIT, child = PEAK_IO_ROOT_INIT;
	PeakIOFile f = PEAK_IO_FILE_INIT;
	PeakIOStage stage = PEAK_IO_STAGE_INIT;
	PeakIOResult io;
	PeakIOPublishResult pub;
	size_t n;

	memset(&race, 0, sizeof(race));
	CHECK(pthread_mutex_init(&race.mutex, NULL) == 0);
	mutex_ready = 1;
	CHECK(mkdtemp(temp) != NULL);
	snprintf(race.root, sizeof(race.root), "%s", temp);
	snprintf(race.external, sizeof(race.external), "%s/outside", temp);
	snprintf(race.nested, sizeof(race.nested), "%s/nested-real", temp);
	snprintf(race.link, sizeof(race.link), "%s/nested", temp);
	CHECK(mkdir(race.nested, 0700) == 0);
	CHECK(mkdir(race.external, 0700) == 0);
	snprintf(p, sizeof(p), "%s/item", race.nested);
	CHECK(write_host(p, "inside"));
	snprintf(p, sizeof(p), "%s/item", race.external);
	CHECK(write_host(p, "outside"));
	snprintf(file, sizeof(file), "%s/existing", temp);
	CHECK(write_host(file, "original"));
	snprintf(p, sizeof(p), "%s/stage-collision", temp);
	CHECK(write_host(p, "reserved"));

	io = peak_io_root_open(race.root, &root);
	CHECK(io.code == PEAK_IO_OK);
	io = peak_io_root_open_at(&root, "nested-real", &child);
	CHECK(io.code == PEAK_IO_OK);
	CHECK(read_peak(&child, "item", "inside"));
	CHECK(peak_io_file_open(&root, "../outside/item", 64, &f).code != PEAK_IO_OK);
	CHECK(peak_io_file_open(&root, "nested-real", 64, &f).code == PEAK_IO_NOT_REGULAR);

	/* This private 0700 fixture is cooperatively controlled. O_EXCL staging
	 * must reject a pre-existing temporary name without changing either file. */
	CHECK(peak_io_stage_begin(&root, "existing", "stage-collision", 32, &stage).code != PEAK_IO_OK);
	CHECK(stage.handle == PEAK_HANDLE_INVALID && stage.parent == PEAK_HANDLE_INVALID);
	CHECK(read_peak(&root, "existing", "original"));
	CHECK(read_peak(&root, "stage-collision", "reserved"));

	/* A bounded-write fault before commit must leave the previous destination intact. */
	io = peak_io_stage_begin(&root, "existing", "stage-fail", 4, &stage);
	CHECK(io.code == PEAK_IO_OK);
	io = peak_io_stage_write(&stage, "toolarge", 8, &n);
	CHECK(io.code != PEAK_IO_OK);
	pub = peak_io_stage_commit(&stage, PEAK_IO_DURABILITY_NONE);
	CHECK(pub.published == 0);
	CHECK(read_peak(&root, "existing", "original"));

	/* Abort/cleanup consumes ownership and does not alter an existing file. */
	stage = (PeakIOStage)PEAK_IO_STAGE_INIT;
	CHECK(peak_io_stage_begin(&root, "existing", "stage-abort", 32, &stage).code == PEAK_IO_OK);
	CHECK(peak_io_stage_write(&stage, "discard", 7, &n).code == PEAK_IO_OK && n == 7);
	pub = peak_io_stage_abort(&stage);
	CHECK(pub.published == 0 && pub.cleanup.code == PEAK_IO_OK);
	CHECK(read_peak(&root, "existing", "original"));

	/* Symlink at a nested component must never expose the outside file. */
	CHECK(symlink(race.external, race.link) == 0);
	CHECK(peak_io_file_open(&root, "nested/item", 64, &f).code != PEAK_IO_OK);
	CHECK(unlink(race.link) == 0);
	pthread_mutex_lock(&race.mutex);
	race.run = 1;
	pthread_mutex_unlock(&race.mutex);
	CHECK(pthread_create(&thread, NULL, swapper, &race) == 0);
	threaded = 1;
	for (i = 0; i < RACE_LOOPS; i++) {
		sched_yield();
		io = peak_io_file_open(&root, "nested/item", 64, &f);
		if (io.code == PEAK_IO_OK) {
			memset(data, 0, sizeof(data));
			n = 0;
			io = peak_io_file_read(&f, 0, data, sizeof(data), &n);
			CHECK(peak_io_file_close(&f).code == PEAK_IO_OK);
			CHECK(io.code == PEAK_IO_OK);
			CHECK(n == 6 && !memcmp(data, "inside", 6));
		} else {
			CHECK(io.code == PEAK_IO_SYSTEM || io.code == PEAK_IO_UNSUPPORTED);
		}
	}
	pthread_mutex_lock(&race.mutex);
	race.run = 0;
	pthread_mutex_unlock(&race.mutex);
	pthread_join(thread, NULL);
	threaded = 0;
	(void)unlink(race.link);

	/* Publication is the cancellation boundary: once committed, abort is not rollback. */
	stage = (PeakIOStage)PEAK_IO_STAGE_INIT;
	CHECK(peak_io_stage_begin(&root, "existing", "stage-commit", 32, &stage).code == PEAK_IO_OK);
	CHECK(peak_io_stage_write(&stage, "published", 9, &n).code == PEAK_IO_OK && n == 9);
	pub = peak_io_stage_commit(&stage, PEAK_IO_DURABILITY_FILE_AND_DIRECTORY);
	CHECK(pub.published == 1 && pub.operation.code == PEAK_IO_OK);
	CHECK(read_peak(&root, "existing", "published"));

	printf("Peak safe-I/O public API checks passed (%d nested lookup races)\n", RACE_LOOPS);
	ok = 1;
done:
	if (threaded) {
		pthread_mutex_lock(&race.mutex);
		race.run = 0;
		pthread_mutex_unlock(&race.mutex);
		pthread_join(thread, NULL);
	}
	if (stage.handle != PEAK_HANDLE_INVALID)
		(void)peak_io_stage_abort(&stage);
	if (f.handle != PEAK_HANDLE_INVALID)
		(void)peak_io_file_close(&f);
	if (child.handle != PEAK_HANDLE_INVALID)
		(void)peak_io_root_close(&child);
	if (root.handle != PEAK_HANDLE_INVALID)
		(void)peak_io_root_close(&root);
	ok &= remove_fixture(&race);
	if (mutex_ready)
		pthread_mutex_destroy(&race.mutex);
	return ok ? 0 : 1;
}
