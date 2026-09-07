#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int peak_fast_env_fd(const char *name);
static int peak_fast_is_pipe(int fd);
static void peak_fast_stdout_tty(void);
static void peak_fast_stdout_pipe(void);
static int peak_fast_want_tty(const char *path, char *const argv[]);
static void peak_fast_stdout_for(const char *path, char *const argv[]);

static const char *const peak_fast_tty_names[] = {
	"nvim", "vim", "vi", "view", "nano", "emacs",
	"less", "more", "most",
	"htop", "btop", "top", "atop",
	"lf", "ranger", "nnn", "mc",
	"ncmpcpp", "cmus", "alsamixer",
	"pi", "gdb", "lldb",
	"fzf", "lazygit", "gitui",
	"tig", "dialog", "whiptail",
	"weechat", "irssi", "mutt", "neomutt",
	"w3m", "lynx", "helix", "kak",
	NULL
};

static int
peak_fast_env_fd(const char *name)
{
	const char *s;
	int fd;

	s = getenv(name);
	if (!s || *s < '0' || *s > '9')
		return -1;
	fd = 0;
	while (*s >= '0' && *s <= '9') {
		fd = fd * 10 + (*s - '0');
		s++;
	}
	if (*s)
		return -1;
	return fd;
}

static int
peak_fast_is_pipe(int fd)
{
	struct stat st;

	if (fd < 0 || fstat(fd, &st) != 0)
		return 0;
	return S_ISFIFO(st.st_mode);
}

static void
peak_fast_stdout_tty(void)
{
	int fd;

	if (isatty(STDOUT_FILENO))
		return;
	fd = peak_fast_env_fd("PEAK_FAST_TTY");
	if (fd >= 0 && isatty(fd))
		dup2(fd, STDOUT_FILENO);
}

static void
peak_fast_stdout_pipe(void)
{
	int fd;

	if (peak_fast_is_pipe(STDOUT_FILENO))
		return;
	fd = peak_fast_env_fd("PEAK_FAST_PIPE");
	if (peak_fast_is_pipe(fd))
		dup2(fd, STDOUT_FILENO);
}

static int
peak_fast_want_tty(const char *path, char *const argv[])
{
	const char *base;
	int i;

	base = NULL;
	if (path && path[0]) {
		base = strrchr(path, '/');
		base = base ? base + 1 : path;
	}
	if ((!base || !base[0]) && argv && argv[0]) {
		base = strrchr(argv[0], '/');
		base = base ? base + 1 : argv[0];
	}
	if (!base || !base[0])
		return 0;
	for (i = 0; peak_fast_tty_names[i]; i++) {
		if (strcmp(base, peak_fast_tty_names[i]) == 0)
			return 1;
	}
	return 0;
}

static void
peak_fast_stdout_for(const char *path, char *const argv[])
{
	if (peak_fast_want_tty(path, argv))
		peak_fast_stdout_tty();
	else
		peak_fast_stdout_pipe();
}

int
execve(const char *path, char *const argv[], char *const envp[])
{
	static int (*real)(const char *, char *const *, char *const *);

	peak_fast_stdout_for(path, argv);
	if (!real)
		real = (int (*)(const char *, char *const *, char *const *))dlsym(RTLD_NEXT, "execve");
	if (!real) {
		errno = ENOSYS;
		return -1;
	}
	return real(path, argv, envp);
}

int
execv(const char *path, char *const argv[])
{
	static int (*real)(const char *, char *const *);

	peak_fast_stdout_for(path, argv);
	if (!real)
		real = (int (*)(const char *, char *const *))dlsym(RTLD_NEXT, "execv");
	if (!real) {
		errno = ENOSYS;
		return -1;
	}
	return real(path, argv);
}

int
execvp(const char *file, char *const argv[])
{
	static int (*real)(const char *, char *const *);

	peak_fast_stdout_for(file, argv);
	if (!real)
		real = (int (*)(const char *, char *const *))dlsym(RTLD_NEXT, "execvp");
	if (!real) {
		errno = ENOSYS;
		return -1;
	}
	return real(file, argv);
}

int
execl(const char *path, const char *arg, ...)
{
	va_list ap;
	char *argv[256];
	int i;

	argv[0] = (char *)arg;
	va_start(ap, arg);
	for (i = 1; i < 255; i++) {
		argv[i] = va_arg(ap, char *);
		if (!argv[i])
			break;
	}
	va_end(ap);
	argv[i] = NULL;
	return execv(path, argv);
}

int
execlp(const char *file, const char *arg, ...)
{
	va_list ap;
	char *argv[256];
	int i;

	argv[0] = (char *)arg;
	va_start(ap, arg);
	for (i = 1; i < 255; i++) {
		argv[i] = va_arg(ap, char *);
		if (!argv[i])
			break;
	}
	va_end(ap);
	argv[i] = NULL;
	return execvp(file, argv);
}
