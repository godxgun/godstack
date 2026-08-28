#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#ifdef __linux__
#include <sys/syscall.h>
long syscall(long number, ...);
#endif

#define PEAK_WAIT_MAX 64

static int peak_internal_nb(int fd);
static PeakProc peak_internal_proc_fail(void);
static int peak_internal_memfd(void);
static size_t peak_internal_io_n(size_t n);

static int
peak_internal_nb(int fd)
{
	int flags;

	if (fd < 0)
		return -1;
	flags = fcntl(fd, F_GETFL);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(fd, F_GETFD);
	if (flags >= 0)
		fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	return fd;
}

static PeakProc
peak_internal_proc_fail(void)
{
	PeakProc p;

	p.fd = PEAK_HANDLE_INVALID;
	p.pid = 0;
	return p;
}

static int
peak_internal_memfd(void)
{
#ifdef __linux__
	return (int)syscall(SYS_memfd_create, "peak", 0);
#else
	char name[64];
	int fd;

	snprintf(name, sizeof name, "/peak.%d.%d", (int)getpid(), rand());
	fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd >= 0)
		shm_unlink(name);
	return fd;
#endif
}

static size_t
peak_internal_io_n(size_t n)
{
	if (n > (size_t)0x40000000)
		return (size_t)0x40000000;
	return n;
}

PeakProc
peak_pty_spawn(const char *file, const char **argv, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	PeakProc p;
	struct winsize ws;
	int master, slave, pid;

	if (!file || !argv)
		return peak_internal_proc_fail();
	memset(&ws, 0, sizeof ws);
	ws.ws_row = (unsigned short)rows;
	ws.ws_col = (unsigned short)cols;
	ws.ws_xpixel = (unsigned short)xpixel;
	ws.ws_ypixel = (unsigned short)ypixel;
	if (openpty(&master, &slave, NULL, NULL, &ws) < 0)
		return peak_internal_proc_fail();
	pid = fork();
	if (pid < 0) {
		close(master);
		close(slave);
		return peak_internal_proc_fail();
	}
	if (pid == 0) {
		close(master);
		setsid();
		if (ioctl(slave, TIOCSCTTY, NULL) < 0)
			_Exit(1);
		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO);
		if (slave > STDERR_FILENO)
			close(slave);
		execvp(file, (char *const *)argv);
		_Exit(127);
	}
	close(slave);
	p.fd = peak_internal_nb(master);
	p.pid = pid;
	return p;
}

void
peak_pty_resize(PeakProc *pty, uint32_t cols, uint32_t rows, uint32_t xpixel, uint32_t ypixel)
{
	struct winsize ws;

	if (!pty || pty->fd < 0)
		return;
	memset(&ws, 0, sizeof ws);
	ws.ws_row = (unsigned short)rows;
	ws.ws_col = (unsigned short)cols;
	ws.ws_xpixel = (unsigned short)xpixel;
	ws.ws_ypixel = (unsigned short)ypixel;
	ioctl(pty->fd, TIOCSWINSZ, &ws);
}

int
peak_pty_reap(PeakProc *pty)
{
	int r;

	if (!pty || pty->pid <= 0)
		return 0;
	r = waitpid(pty->pid, NULL, WNOHANG);
	if (r <= 0)
		return 0;
	pty->pid = 0;
	return 1;
}

void
peak_pty_close(PeakProc *pty)
{
	if (!pty)
		return;
	if (pty->fd >= 0) {
		close(pty->fd);
		pty->fd = PEAK_HANDLE_INVALID;
	}
	if (pty->pid > 0) {
		waitpid(pty->pid, NULL, 0);
		pty->pid = 0;
	}
}

int
peak_wait(PeakWindow *win, const PEAK_HANDLE *fds, uint32_t n, int timeout_ms)
{
	struct pollfd pfd[PEAK_WAIT_MAX];
	uint32_t i, np;
	int xfd;

	np = 0;
	if (n > PEAK_WAIT_MAX - 2)
		n = PEAK_WAIT_MAX - 2;
	if (win) {
		xfd = peak_window_fd(win);
		if (xfd >= 0) {
			pfd[np].fd = xfd;
			pfd[np].events = POLLIN;
			np++;
		}
		if (peak_window_pending(win) > 0)
			timeout_ms = 0;
	}
	for (i = 0; i < n; i++) {
		if (!fds || fds[i] < 0)
			continue;
		pfd[np].fd = fds[i];
		pfd[np].events = POLLIN | POLLHUP | POLLERR;
		np++;
	}
	if (!np)
		return 0;
	return poll(pfd, (nfds_t)np, timeout_ms) > 0;
}

int
peak_runtime_dir(char *buf, size_t cap, const char *app)
{
	const char *rt;
	int n;

	if (!buf || cap < 2 || !app || !app[0])
		return 0;
	rt = getenv("XDG_RUNTIME_DIR");
	if (rt && rt[0])
		n = snprintf(buf, cap, "%s/%s", rt, app);
	else
		n = snprintf(buf, cap, "/tmp/%s-%d", app, (int)getuid());
	if (n < 0 || (size_t)n >= cap)
		return 0;
	if (mkdir(buf, 0700) < 0 && errno != EEXIST)
		return 0;
	return 1;
}

PEAK_HANDLE
peak_sock_listen(const char *path)
{
	struct sockaddr_un addr;
	int fd;
	size_t n;

	if (!path || !path[0])
		return PEAK_HANDLE_INVALID;
	n = strlen(path);
	if (n >= sizeof addr.sun_path)
		return PEAK_HANDLE_INVALID;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return PEAK_HANDLE_INVALID;
	peak_internal_nb(fd);
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, path, n + 1);
	unlink(path);
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(fd);
		return PEAK_HANDLE_INVALID;
	}
	if (chmod(path, 0600) < 0 || listen(fd, 8) < 0) {
		unlink(path);
		close(fd);
		return PEAK_HANDLE_INVALID;
	}
	return fd;
}

PEAK_HANDLE
peak_sock_accept(PEAK_HANDLE listen_fd)
{
	int fd;

	if (listen_fd < 0)
		return PEAK_HANDLE_INVALID;
	for (;;) {
		fd = accept(listen_fd, NULL, NULL);
		if (fd >= 0)
			return peak_internal_nb(fd);
		if (errno == EINTR)
			continue;
		return PEAK_HANDLE_INVALID;
	}
}

PEAK_HANDLE
peak_sock_connect(const char *path)
{
	struct sockaddr_un addr;
	int fd;
	size_t n;

	if (!path || !path[0])
		return PEAK_HANDLE_INVALID;
	n = strlen(path);
	if (n >= sizeof addr.sun_path)
		return PEAK_HANDLE_INVALID;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return PEAK_HANDLE_INVALID;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, path, n + 1);
	if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(fd);
		return PEAK_HANDLE_INVALID;
	}
	return peak_internal_nb(fd);
}

int
peak_filesystem_mkdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return mkdir(path, 0777) == 0;
}

int
peak_filesystem_rm(const char *path)
{
	if (!path || !path[0])
		return 0;
	if (unlink(path) == 0)
		return 1;
	if (errno == EISDIR || errno == EPERM)
		return rmdir(path) == 0;
	return 0;
}

int
peak_filesystem_cwd(char *buf, size_t cap)
{
	if (!buf || cap < 2)
		return 0;
	return getcwd(buf, cap) != NULL;
}

int
peak_filesystem_chdir(const char *path)
{
	if (!path || !path[0])
		return 0;
	return chdir(path) == 0;
}

int
peak_filesystem_rename(const char *from, const char *to)
{
	if (!from || !from[0] || !to || !to[0])
		return 0;
	return rename(from, to) == 0;
}

int
peak_fd_read(PEAK_HANDLE fd, void *buf, size_t n)
{
	ssize_t r;

	if (fd < 0 || !buf)
		return 0;
	n = peak_internal_io_n(n);
	for (;;) {
		r = read(fd, buf, n);
		if (r > 0)
			return (int)r;
		if (r == 0)
			return 0;
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;
		return 0;
	}
}

int
peak_fd_write(PEAK_HANDLE fd, const void *buf, size_t n)
{
	ssize_t r;

	if (fd < 0 || !buf)
		return 0;
	n = peak_internal_io_n(n);
	for (;;) {
		r = write(fd, buf, n);
		if (r > 0)
			return (int)r;
		if (r == 0)
			return 0;
		if (errno == EINTR)
			continue;
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;
		return 0;
	}
}

void
peak_fd_close(PEAK_HANDLE fd)
{
	if (fd >= 0)
		close(fd);
}

PeakProc
peak_job_run(const char *cmd, const char *cwd)
{
	PeakProc p;
	int pipefd[2], pid;

	if (!cmd || !cmd[0])
		return peak_internal_proc_fail();
	if (pipe(pipefd) < 0)
		return peak_internal_proc_fail();
	pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return peak_internal_proc_fail();
	}
	if (pid == 0) {
		int nullfd;

		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		if (pipefd[1] > STDERR_FILENO)
			close(pipefd[1]);
		nullfd = open("/dev/null", O_RDONLY);
		if (nullfd >= 0) {
			dup2(nullfd, STDIN_FILENO);
			if (nullfd > STDERR_FILENO)
				close(nullfd);
		}
		if (cwd && cwd[0] && chdir(cwd) < 0) {
			/* inherit parent cwd */
		}
		execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
		_Exit(127);
	}
	close(pipefd[1]);
	p.fd = peak_internal_nb(pipefd[0]);
	p.pid = pid;
	return p;
}

int
peak_job_reap(PeakProc *job, int *code)
{
	int r, status;

	if (!job || job->pid <= 0)
		return 0;
	r = waitpid(job->pid, &status, WNOHANG);
	if (r <= 0)
		return 0;
	job->pid = 0;
	if (code) {
		if (WIFEXITED(status))
			*code = WEXITSTATUS(status);
		else if (WIFSIGNALED(status))
			*code = 128 + WTERMSIG(status);
		else
			*code = 1;
	}
	return 1;
}

void
peak_job_kill(PeakProc *job)
{
	if (!job)
		return;
	if (job->fd >= 0) {
		close(job->fd);
		job->fd = PEAK_HANDLE_INVALID;
	}
	if (job->pid > 0) {
		kill(job->pid, SIGKILL);
		waitpid(job->pid, NULL, 0);
		job->pid = 0;
	}
}

int
peak_pid_cwd(int pid, char *buf, size_t cap)
{
	char path[64];
	ssize_t n;

	if (pid <= 0 || !buf || cap < 2)
		return 0;
#ifdef __linux__
	snprintf(path, sizeof path, "/proc/%d/cwd", pid);
	n = readlink(path, buf, cap - 1);
	if (n < 0)
		return 0;
	buf[n] = 0;
	return 1;
#else
	(void)path;
	(void)n;
	if (pid != (int)getpid())
		return 0;
	return getcwd(buf, cap) != NULL;
#endif
}

size_t
peak_page_size(void)
{
	long n;

	n = sysconf(_SC_PAGESIZE);
	if (n <= 0)
		return 4096;
	return (size_t)n;
}

void *
peak_mirror_map(size_t size)
{
	int fd;
	char *base;
	void *a, *b;

	if (!size || size % peak_page_size())
		return NULL;
	fd = peak_internal_memfd();
	if (fd < 0 || ftruncate(fd, (off_t)size) < 0) {
		if (fd >= 0)
			close(fd);
		return NULL;
	}
	base = mmap(NULL, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) {
		close(fd);
		return NULL;
	}
	a = mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	b = mmap(base + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	close(fd);
	if (a == MAP_FAILED || b == MAP_FAILED) {
		munmap(base, size * 2);
		return NULL;
	}
	return base;
}

void
peak_mirror_unmap(void *p, size_t size)
{
	if (!p || !size)
		return;
	munmap(p, size * 2);
}
