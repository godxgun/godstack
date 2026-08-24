#define PEAK_IMPLEMENTATION
#include "../../Peak/peak.h"

#include <stdio.h>


void print_window_resize(PeakEvent ev) {
    printf("window: %u %u\n", ev.resize.width, ev.resize.height);
}

void print_window_close(PeakEvent ev) {
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


typedef struct {
    char *msg;
} AppData;

static AppData data;

static int
peak_run_func(PeakWindow *win, void *userdata) 
{
    AppData d = *(AppData*) userdata;
    static PeakEvent ev; 
    while (peak_window_epoll(win, &ev)) {
        switch (ev.type) {
            case PEAK_EVENT_NONE:
                continue;
            case PEAK_EVENT_WINDOW_CLOSE:
                return 0;
            default:
                print_event[ev.type](ev);
                continue;
        }
    }
    printf("%s\n", d.msg);
    return 1;
}

int
main(int argc, char**argv) 
{
    data.msg = "Hello World";

    if (!peak_init()) {
        return 1;
    }

    PeakWindow win = peak_window_open("demo", 400, 400, 0);
    peak_window_run(&win, peak_run_func, &data);
    peak_window_close(&win);
    peak_quit();
    return 0;
}
