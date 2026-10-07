#include "peak.h"
#include "peak.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t declaration_a_time(void);
int declaration_b_alloc(void);
int main(void);

int
main(void)
{
    uint64_t before;
    uint64_t after;
    unsigned long size;
    char contents[sizeof "peak-header"];
    void *buffer;

    before = peak_get_time();
    peak_sleep_ns(1000000);
    after = peak_get_time();
    if (after < before) return 1;
    if (declaration_a_time() == 0) return 2;
    if (!declaration_b_alloc()) return 3;

    if (!peak_file_write("peak-header.bin", "peak-header", sizeof "peak-header")) return 4;
    if (!peak_file_exists("peak-header.bin")) return 5;
    buffer = peak_file_alloc("peak-header.bin", &size);
    if (!buffer || size != sizeof "peak-header") return 6;
    memcpy(contents, buffer, sizeof contents);
    free(buffer);
    if (memcmp(contents, "peak-header", sizeof contents) != 0) return 7;
    if (!peak_filesystem_rm("peak-header.bin")) return 8;

    puts("Peak single-header multi-TU workload passed");
    return 0;
}
