+++
title = 'Wire symbols'
description = 'Types, macros, and functions declared by Wire.h.'
weight = 70
[params]
  id = 'symbols/wire'
  product = 'Wire'
  category = 'Symbols'
  tags = ['API', 'symbols', 'C']
  access = 'free'
  symbols = true
  audience = 'Core'
+++

# Wire symbols

For setup and API workflows, read the [Wire guide](/docs/wire/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Wire.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### WireHttpRequest {#wirehttprequest}

Source: `Wire.h:44`

```c
typedef struct WireHttpRequest {
	char method[16];
	char path[WIRE_HTTP_MAX_PATH];
	int hx;
} WireHttpRequest;
```

#### WireHttpHandler {#wirehttphandler}

Source: `Wire.h:50`

```c
typedef int (*WireHttpHandler)(int fd, const WireHttpRequest *req, void *user);
```

### Functions

#### wire_http_listen {#wire_http_listen}

Source: `Wire.h:52`

```c
int wire_http_listen(int port);
```

Bind 0.0.0.0:port. Returns fd, or -1.

#### wire_http_accept {#wire_http_accept}

Source: `Wire.h:53`

```c
int wire_http_accept(int listen_fd);
```

Accept a client. Returns fd, or -1.

#### wire_http_serve {#wire_http_serve}

Source: `Wire.h:54`

```c
int wire_http_serve(int port, WireHttpHandler handler, void *user);
```

Listen, read GET, call handler, close. Does not return on success.

#### wire_http_read_req {#wire_http_read_req}

Source: `Wire.h:55`

```c
int wire_http_read_req(int fd, WireHttpRequest *req);
```

Parse a GET request. 0 ok, else status.

#### wire_http_write_status {#wire_http_write_status}

Source: `Wire.h:56`

```c
int wire_http_write_status(int fd, int status, const char *ctype, const void *body, size_t len);
```

Write a complete response. body may be NULL if len is 0.

#### wire_http_error {#wire_http_error}

Source: `Wire.h:57`

```c
int wire_http_error(int fd, int status);
```

Small HTML error page.

#### wire_http_serve_static {#wire_http_serve_static}

Source: `Wire.h:58`

```c
int wire_http_serve_static(int fd, const char *url_path);
```

GET /static/* from ./static. Returns status.

<!-- END GENERATED API -->
