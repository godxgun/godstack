# Wire

Documentation: https://godxgun.github.io/godstack/docs/wire/
Description: Handle GET requests with Wire on Unix and understand its deliberately narrow HTTP scope.


# Wire

Wire is a small blocking HTTP/1.1 server for GET requests. You supply one handler function; Wire
accepts connections, parses the request line, calls your handler, and closes the connection. It
also serves files from a `static` directory. It currently builds on Unix systems with
`sendfile()` in `<sys/sendfile.h>`, such as Linux.

Define `WIRE_IMPLEMENTATION` in exactly one translation unit before including `Wire.h`. Other
translation units include the header without it. In the implementation file, include `Wire.h`
before any system header: the implementation defines `_DEFAULT_SOURCE` to get POSIX declarations
such as `realpath()` and `sigaction()`, which only works if no system header was read first.
Alternatively, define `_DEFAULT_SOURCE` yourself at the top of that file.

## Serving a Response

The first thing to try is a server that answers every request with the same body:

```c
#define WIRE_IMPLEMENTATION
#include "Wire.h"

static int on_request(int fd, const WireHttpRequest *req, void *user)
{
    (void)req;
    (void)user;
    return wire_http_write_status(fd, 200, "text/plain", "ok\n", 3);
}

int main(void)
{
    return wire_http_serve(8080, on_request, NULL);
}
```

Run it and open `http://127.0.0.1:8080/`. `wire_http_serve()` binds `0.0.0.0:8080`, so the server
is reachable from other machines unless a firewall prevents it. On success it never returns; it
returns `-1` if the handler is NULL or the port cannot be bound. It also ignores `SIGPIPE`, so a
client that disconnects early does not kill the process.

The handler receives the connected socket, the parsed request and the `user` pointer passed to
`wire_http_serve()`. `wire_http_write_status()` writes a complete response: status line,
`Content-Type`, `Content-Length`, `Connection: close` and the body. The body may be NULL when the
length is zero. Wire closes the socket after the handler returns and ignores the return value.

## Routing Requests

`req->path` holds the request path with any query string removed (at most
`WIRE_HTTP_MAX_PATH - 1` bytes). Requests are handled one at a time, and only GET reaches the
handler: malformed requests get a `400` page and other methods a `405` page before your code
runs. Branch on the path and write one response per request:

```c
static const char home[] = "<!doctype html><title>Wire</title><h1>Hello</h1>\n";

static int route(int fd, const WireHttpRequest *req, void *user)
{
    (void)user;
    if (strcmp(req->path, "/") == 0)
        return wire_http_write_status(fd, 200, "text/html; charset=utf-8",
                                      home, sizeof home - 1);
    if (strcmp(req->path, "/health") == 0)
        return wire_http_write_status(fd, 200, "text/plain", "ok\n", 3);
    return wire_http_error(fd, 404);
}
```

`wire_http_error()` writes a small HTML page such as `<h1>404 Not Found</h1>`. The path comes from
the client: escape it before echoing it into HTML, for example with
[Cool](/godstack/docs/cool/)'s `cool_html_txt()`.

## Serving Static Files

`wire_http_serve_static()` answers `/static/...` URLs from the `static` directory under the
current working directory:

```c
if (strncmp(req->path, "/static/", 8) == 0)
    return wire_http_serve_static(fd, req->path);
```

It writes the whole response, including error pages, and returns the HTTP status it sent. The
part after `/static/` may contain only letters, digits, `.`, `_`, `-` and `/`, must not contain
`..`, and must resolve to a regular file inside `static` after symbolic links are followed.
Anything else is a `404`. The content type comes from the extension: `.html`, `.css`, `.js`,
`.svg`, `.ttf` and `.gif` are recognized, and every other file is sent as
`application/octet-stream`.

## Partial Responses for htmx

`req->hx` is nonzero when the request carries the `HX-Request: true` header that
[htmx](https://htmx.org/) sends. A handler can return just the changed fragment to htmx and a full
page to everyone else:

```c
static int counter(int fd, const WireHttpRequest *req, void *user)
{
    int *count = user;
    char body[128];
    int n;

    ++*count;
    if (req->hx)
        n = snprintf(body, sizeof body, "<span id=\"count\">%d</span>", *count);
    else
        n = snprintf(body, sizeof body,
                     "<!doctype html><button hx-get=\"/\" hx-target=\"#count\">"
                     "<span id=\"count\">%d</span></button>", *count);
    if (n < 0 || (size_t)n >= sizeof body) return wire_http_error(fd, 500);
    return wire_http_write_status(fd, 200, "text/html; charset=utf-8", body, (size_t)n);
}
```

Pass `&count` as the `user` argument of `wire_http_serve()`. Wire calls the handler from the
thread that called `wire_http_serve()`, one request at a time, so `user` state needs no locking.

## Running Your Own Loop

`wire_http_serve()` is built from four public calls. Use them directly when you need to log,
count or stop serving:

```c
int listener = wire_http_listen(8080);
if (listener < 0) return 1;

for (int served = 0; served < 100; ++served) {
    int fd = wire_http_accept(listener);
    if (fd < 0) continue;

    WireHttpRequest req;
    int status = wire_http_read_req(fd, &req);
    if (status != 0)
        wire_http_error(fd, status);
    else
        route(fd, &req, NULL);
    close(fd);
}
close(listener);
```

`wire_http_read_req()` returns `0` for a GET request, or the status to send (`400` or `405`). It
reads at most `WIRE_HTTP_MAX_REQ` bytes of request header and ignores any body. A custom loop
must ignore `SIGPIPE` itself if clients may disconnect early.

## Limits

Wire handles one connection at a time and has no timeouts, so a slow client delays everyone else.
There is no keep-alive, TLS, request body, chunked encoding or header access beyond `HX-Request`.
Put it behind a reverse proxy, or use a larger server, when you need those.

See [Wire symbols](/godstack/symbols/wire/) for the generated declarations.
