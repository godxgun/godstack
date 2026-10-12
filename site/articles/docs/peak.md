+++
title = 'Peak'
description = 'Set up Peak runtime contexts and a platform event loop with target-specific dependencies.'
weight = 60
[params]
  id = 'peak'
  product = 'Peak'
  category = 'Core Libraries'
  tags = ['C', 'platform', 'windowing', 'audio']
  access = 'free'
  audience = 'Core'
+++

# Peak

Peak is a platform layer that abstracts operating-system windowing and related services behind
a C API. Platform-specific implementations are selected at compile time; some system libraries
are loaded at runtime.

## Opening a Window

The first thing to try is opening a window and setting up an event loop. You can poll events with
`peak_window_epoll`, or let `peak_window_run` call a tick callback.

### A Basic Event Loop
```c
#define PEAK_IMPLEMENTATION
#include "Peak.h"
int main(void) {
    PeakCtx *ctx = peak_init();
    if (!ctx) return 1;

    PeakWindow window = peak_window_open(ctx, "My Window", 888, 333, 0);
    if (!window.ctx) {
        peak_quit(ctx);
        return 1;
    }

    PeakEvent event;
    bool running = true;
    while (running) {
        while (peak_window_epoll(&window, &event)) {
            if (event.type == PEAK_EVENT_WINDOW_CLOSE) running = false;
        }
    }

    peak_window_close(&window);
    peak_quit(ctx);
    return 0;
}
```

The context owns Peak's runtime state and is required when opening a window. Call `peak_init()`
to create it and `peak_quit(ctx)` when done. Peak manages its runtime storage internally.

### With The Callback API

The callback API helps portability on platforms such as the web, where the browser owns the
main loop. `peak_window_run()` repeatedly calls your callback on native platforms and registers
it with the browser main loop on Web.


```c
static int tick(PeakWindow *window, void *userdata) {
    PeakEvent event;
    (void)userdata;
    while (peak_window_epoll(window, &event)) {
        if (event.type == PEAK_EVENT_WINDOW_CLOSE) return 0;
    }
    return 1;
}

int main(void) {
    PeakCtx *ctx = peak_init();
    if (!ctx) return 1;
    PeakWindow window = peak_window_open(ctx, "My Window", 888, 333, 0);
    if (window.ctx) {
        peak_window_run(&window, tick, NULL);
        peak_window_close(&window);
    }
    peak_quit(ctx);
    return 0;
}
```

## Using the Filesystem

The basic file helpers use paths from the process working directory. `peak_file_alloc()` returns
a heap buffer; release it with `free()`. The filesystem helpers return nonzero on success.

```c
const char message[] = "hello from Peak\n";
const char *path = "peak-note.txt";

if (!peak_file_write(path, message, sizeof(message) - 1)) return 1;

unsigned long size = 0;
char *contents = peak_file_alloc(path, &size);
if (!contents) return 1;
fwrite(contents, 1, size, stdout);
free(contents);

if (!peak_filesystem_rename(path, "peak-note-copy.txt")) return 1;
if (!peak_filesystem_rm("peak-note-copy.txt")) return 1;
```

Use `peak_filesystem_cwd()` and `peak_filesystem_chdir()` to inspect or change the working
directory. `peak_filesystem_mkdir()` creates one directory level, and `peak_filesystem_list()`
calls your callback for each entry. For bounded, directory-handle-based I/O on supported Linux
systems, see the `peak_io_*` API in the [symbol reference](/symbols/peak/).

## Spawning a TTY

`peak_pty_spawn()` starts a child attached to a pseudoterminal and returns its nonblocking file
handle and process ID. Pass the executable and a NULL-terminated argument vector. This example
uses the POSIX shell; choose a host-appropriate executable and arguments for other systems.

```c
const char *argv[] = {"/bin/sh", NULL};
PeakProc tty = peak_pty_spawn("/bin/sh", argv, 80, 24, 0, 0);
if (tty.fd == PEAK_HANDLE_INVALID) return 1;

const char command[] = "printf 'hello from the child\\n'; exit\n";
if (peak_fd_write(tty.fd, command, sizeof(command) - 1) < 0) {
    peak_pty_close(&tty);
    return 1;
}

char output[256];
for (;;) {
    int n = peak_fd_read(tty.fd, output, sizeof(output));
    if (n > 0) fwrite(output, 1, (size_t)n, stdout);
    else if (n == 0) break; /* child closed the terminal */
    else if (peak_pty_reap(&tty)) break; /* no data and child has exited */
    else peak_sleep_ns(1000000); /* -1: no data available yet */
}
while (!peak_pty_reap(&tty)) peak_sleep_ns(1000000);
peak_pty_close(&tty);
```

Use `peak_pty_resize()` when the terminal dimensions change. `peak_pty_reap()` reports when the
child exits; `peak_pty_close()` releases the PTY and any remaining process resources.

## Memory Mapped Circular Ring Buffer

`peak_mirror_map()` maps the same storage twice in adjacent address ranges. The requested size
must be a multiple of `peak_page_size()`. The returned pointer can be indexed across the wrap
boundary without splitting a copy; the valid mapped range is `size * 2` bytes.

```c
size_t size = peak_page_size();
unsigned char *ring = peak_mirror_map(size);
if (!ring) return 1;

/* This contiguous write crosses the ring boundary at `size`. */
memcpy(ring + size - 3, "abcdef", 6);
/* The bytes are also visible at ring[0..2] through the second mapping. */

peak_mirror_unmap(ring, size);
```

Keep the original `size` for `peak_mirror_unmap()` and do not access the mapping after unmapping.

## Streaming Audio

`peak_audio_start()` asks the audio device to pull interleaved signed 16-bit samples. The callback
receives a frame count, so write `frames * channels` samples. Keep it real-time safe: avoid
blocking, file I/O, and allocation in the callback. It returns nonzero when the stream starts;
call `peak_audio_stop()` to stop it. With `PEAK_NO_AUDIO`, starting audio returns zero.

```c
static void fill_audio(int16_t *out, size_t frames, void *userdata) {
    uint32_t *phase = userdata;
    for (size_t i = 0; i < frames; ++i) {
        *phase = (*phase + 440) % 48000;
        out[i] = (*phase % 48000) < 24000 ? 8000 : -8000;
    }
}

uint32_t phase = 0;
if (!peak_audio_start(1, 48000, fill_audio, &phase)) return 1;
/* ... play ... */
peak_audio_stop();
```

