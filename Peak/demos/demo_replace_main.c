#include <stdio.h>
#include <math.h>
#include <unistd.h>

#define PEAK_REPLACE_MAIN
#define PEAK_IMPLEMENTATION
#include "../peak.h"


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

int main(int argc, char**argv) {
    data.msg = "Hello World";
    return PEAK_CONTINUE;
}

void
peak_events(PeakEvent ev) 
{
    print_event[ev.type](ev);
    switch (ev.type) {
        case PEAK_EVENT_WINDOW_CLOSE:
            printf("stoppppeeed\n");
            peak_stop();
            break;
    }
}

void
peak_tick() 
{
    printf("%s\n",data.msg);
    sleep(1);
}
