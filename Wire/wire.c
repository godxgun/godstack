#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "wire.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static int wire_http_path_ok(const char *rel);
static const char *wire_http_mime(const char *path);
static int wire_http_header_complete(const char *buf, size_t n);
static int wire_http_parse_req(char *buf, WireHttpRequest *req);
static int wire_http_write_all(int fd, const void *buf, size_t len);
static const char *wire_http_status_text(int status);
static int wire_http_fail(int fd, int file, int status);

const char *
wire_http_status_text(int status)
{
	static const struct {
		int code;
		const char *text;
	} tab[] = {
		{ 200, "OK" },
		{ 400, "Bad Request" },
		{ 404, "Not Found" },
		{ 405, "Method Not Allowed" },
		{ 500, "Internal Server Error" },
	};
	size_t i;

	for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
		if (tab[i].code == status)
			return tab[i].text;
	}
	return "Error";
}

int
wire_http_write_all(int fd, const void *buf, size_t len)
{
	const char *p;
	size_t left;

	p = buf;
	left = len;
	while (left > 0) {
		ssize_t n;

		n = write(fd, p, left);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		p += (size_t)n;
		left -= (size_t)n;
	}
	return 0;
}

int
wire_http_write_status(int fd, int status, const char *ctype, const void *body, size_t len)
{
	char hdr[512];
	int n;

	n = snprintf(hdr, sizeof hdr,
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"\r\n",
		status, wire_http_status_text(status), ctype, len);
	if (n < 0 || (size_t)n >= sizeof hdr)
		return -1;
	if (wire_http_write_all(fd, hdr, (size_t)n) < 0)
		return -1;
	if (len > 0 && body)
		return wire_http_write_all(fd, body, len);
	return 0;
}

int
wire_http_error(int fd, int status)
{
	char body[256];
	int n;

	n = snprintf(body, sizeof body, "<h1>%d %s</h1>\n",
		status, wire_http_status_text(status));
	if (n < 0)
		return -1;
	return wire_http_write_status(fd, status, "text/html; charset=utf-8",
		body, (size_t)n);
}

int
wire_http_header_complete(const char *buf, size_t n)
{
	size_t i;

	if (n < 4)
		return 0;
	for (i = 0; i + 3 < n; i++) {
		if (buf[i] == '\r' && buf[i + 1] == '\n' &&
		    buf[i + 2] == '\r' && buf[i + 3] == '\n')
			return 1;
	}
	return 0;
}

int
wire_http_parse_req(char *buf, WireHttpRequest *req)
{
	char *line;
	char *save;
	char *method;
	char *path;
	char *q;
	char *hdr;
	size_t mlen;
	size_t plen;

	req->hx = 0;
	req->method[0] = 0;
	req->path[0] = 0;

	line = strtok_r(buf, "\r\n", &save);
	if (!line)
		return 400;

	method = line;
	path = strchr(line, ' ');
	if (!path)
		return 400;
	*path++ = 0;
	while (*path == ' ')
		path++;
	{
		char *ver;

		ver = strchr(path, ' ');
		if (ver)
			*ver = 0;
	}

	q = strchr(path, '?');
	if (q)
		*q = 0;

	mlen = strlen(method);
	plen = strlen(path);
	if (mlen == 0 || mlen >= sizeof req->method)
		return 400;
	if (plen == 0 || plen >= sizeof req->path)
		return 400;
	if (path[0] != '/')
		return 400;

	memcpy(req->method, method, mlen + 1);
	memcpy(req->path, path, plen + 1);

	while ((hdr = strtok_r(NULL, "\r\n", &save)) != NULL) {
		char *colon;
		char *val;

		colon = strchr(hdr, ':');
		if (!colon)
			continue;
		*colon = 0;
		val = colon + 1;
		while (*val == ' ' || *val == '\t')
			val++;
		if (strcasecmp(hdr, "HX-Request") == 0) {
			if (strcasecmp(val, "true") == 0)
				req->hx = 1;
		}
	}

	if (strcmp(req->method, "GET") != 0)
		return 405;
	return 0;
}

int
wire_http_read_req(int fd, WireHttpRequest *req)
{
	char buf[WIRE_HTTP_MAX_REQ];
	size_t nread;

	nread = 0;
	while (nread < sizeof buf - 1) {
		ssize_t n;

		n = read(fd, buf + nread, sizeof buf - 1 - nread);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return 400;
		}
		if (n == 0)
			break;
		nread += (size_t)n;
		buf[nread] = 0;
		if (wire_http_header_complete(buf, nread))
			break;
	}
	if (!wire_http_header_complete(buf, nread))
		return 400;
	buf[nread] = 0;
	return wire_http_parse_req(buf, req);
}

int
wire_http_path_ok(const char *rel)
{
	size_t i;

	if (!rel || !rel[0])
		return 0;
	for (i = 0; rel[i]; i++) {
		char c;

		c = rel[i];
		if (c == '.' && rel[i + 1] == '.')
			return 0;
		if (!(isalnum((unsigned char)c) || c == '.' || c == '_' ||
		    c == '-' || c == '/'))
			return 0;
	}
	return 1;
}

const char *
wire_http_mime(const char *path)
{
	static const struct {
		const char *ext;
		const char *mime;
	} tab[] = {
		{ ".css", "text/css" },
		{ ".js", "application/javascript" },
		{ ".svg", "image/svg+xml" },
		{ ".ttf", "font/ttf" },
		{ ".gif", "image/gif" },
		{ ".html", "text/html; charset=utf-8" },
	};
	const char *dot;
	size_t i;

	dot = strrchr(path, '.');
	if (!dot)
		return "application/octet-stream";
	for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
		if (strcmp(dot, tab[i].ext) == 0)
			return tab[i].mime;
	}
	return "application/octet-stream";
}

int
wire_http_fail(int fd, int file, int status)
{
	if (file >= 0)
		close(file);
	wire_http_error(fd, status);
	return status;
}

int
wire_http_listen(int port)
{
	int fd;
	int one;
	struct sockaddr_in addr;

	if (port <= 0 || port > 65535)
		return -1;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
		close(fd);
		return -1;
	}
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((unsigned short)port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(fd);
		return -1;
	}
	if (listen(fd, 64) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

int
wire_http_accept(int listen_fd)
{
	for (;;) {
		int fd;

		fd = accept(listen_fd, NULL, NULL);
		if (fd < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		return fd;
	}
}

int
wire_http_serve(int port, WireHttpHandler handler, void *user)
{
	int listen_fd;
	struct sigaction sa;

	if (!handler)
		return -1;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
	listen_fd = wire_http_listen(port);
	if (listen_fd < 0)
		return -1;
	for (;;) {
		WireHttpRequest req;
		int err;
		int fd;

		fd = wire_http_accept(listen_fd);
		if (fd < 0)
			continue;
		err = wire_http_read_req(fd, &req);
		if (err) {
			wire_http_error(fd, err);
			close(fd);
			continue;
		}
		handler(fd, &req, user);
		close(fd);
	}
}

int
wire_http_serve_static(int fd, const char *url_path)
{
	const char *rel;
	char full[PATH_MAX];
	char root_real[PATH_MAX];
	char file_real[PATH_MAX];
	int file;
	struct stat st;
	size_t want;
	size_t rlen;
	off_t off;

	if (strncmp(url_path, "/static/", 8) != 0)
		return wire_http_fail(fd, -1, 404);
	rel = url_path + 8;
	if (!wire_http_path_ok(rel))
		return wire_http_fail(fd, -1, 404);
	if (snprintf(full, sizeof full, "static/%s", rel) >= (int)sizeof full)
		return wire_http_fail(fd, -1, 404);
	if (!realpath("static", root_real) || !realpath(full, file_real))
		return wire_http_fail(fd, -1, 404);
	rlen = strlen(root_real);
	if (strncmp(file_real, root_real, rlen) != 0 ||
	    (file_real[rlen] != '/' && file_real[rlen] != 0))
		return wire_http_fail(fd, -1, 404);

	file = open(file_real, O_RDONLY);
	if (file < 0)
		return wire_http_fail(fd, -1, 404);
	if (fstat(file, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size < 0)
		return wire_http_fail(fd, file, 404);
	want = (size_t)st.st_size;
	if (wire_http_write_status(fd, 200, wire_http_mime(file_real), NULL, want) < 0)
		return wire_http_fail(fd, file, 500);
	off = 0;
	while ((size_t)off < want) {
		ssize_t n;

		n = sendfile(fd, file, &off, want - (size_t)off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			close(file);
			return 500;
		}
		if (n == 0)
			break;
	}
	close(file);
	return 200;
}
