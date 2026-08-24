#include <stdio.h>

#define PEAK_IMPLEMENTATION
#include "../../Peak/peak.h"

void print_window_resize(PeakEvent ev) {
    printf("window: %u %u\n", ev.resize.width, ev.resize.height);
}

void print_window_close(PeakEvent ev) {
    (void)ev;
    printf("WINDOW CLOSE!!!!!!\nWOWWWOWAAAAAAAAAAAAAAAAAAAAAAAAh\n");
}

void print_mouse(PeakEvent ev) {
    printf("mouse: state=%d type=%d pos=(%f, %f)\n", ev.pointer.state, ev.pointer.type, ev.pointer.x, ev.pointer.y);
}

void print_key(PeakEvent ev) {
    printf("key: key=%d mod=%d\n", ev.key.key, ev.key.mod);
}

typedef void (*print_event_func)(PeakEvent ev);
print_event_func print_event[PEAK_EVENT_LAST] = {
    [PEAK_EVENT_NONE] = print_mouse,
    [PEAK_EVENT_WINDOW_RESIZE] = print_window_resize,
    [PEAK_EVENT_WINDOW_CLOSE] = print_window_close,
    [PEAK_EVENT_POINTER] = print_mouse,
    [PEAK_EVENT_POINTER_CONNECTED] = print_mouse,
    [PEAK_EVENT_POINTER_DISCONNECTED] = print_mouse,
    [PEAK_EVENT_KEY_DOWN] = print_key,
    [PEAK_EVENT_KEY_UP] = print_key,
};

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (!peak_init()) return 1;

    PeakWindow win = peak_window_open("Demo", 800, 600, 0);
    peak_window_clear(&win, 0.5, 0.5, 0, 1);

    PeakEvent ev;
    while (win.running) {
        while (peak_window_epoll(&win, &ev)) {
            if (ev.type == PEAK_EVENT_NONE) continue;
            print_event[ev.type](ev);
            if (ev.type == PEAK_EVENT_WINDOW_CLOSE) {
                win.running = 0;
                break;
            }
        }
        if (!win.running) break;

    }

    peak_window_close(&win);
    peak_quit();
    return 0;
}
