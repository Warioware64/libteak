// SPDX-License-Identifier: CC0-1.0
//
// Runs the app_* hooks of a DSP binary on the host, prints "job checksum"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
void app_init(void);
uint16_t app_job(uint16_t job);
uint32_t app_checksum(uint16_t job);
uint16_t *app_upload_buffer(uint16_t table, uint16_t words);
uint16_t app_param(uint16_t id, uint32_t value);
// Uploads: "U table word word ..." lines from HC_UPLOADS file; params "P id value"
int main(int argc, char **argv) {
    app_init();
    FILE *f = argc > 2 ? fopen(argv[2], "r") : NULL;
    char kind[4];
    while (f && fscanf(f, "%3s", kind) == 1) {
        if (kind[0] == 'U') { unsigned t, n; fscanf(f, "%x %x", &t, &n); uint16_t *p = app_upload_buffer(t, n);
            for (unsigned i = 0; i < n; i++) { unsigned w; fscanf(f, "%x", &w); if (p) p[i] = w; } }
        else if (kind[0] == 'P') { unsigned id, v; fscanf(f, "%x %x", &id, &v); app_param(id, v); }
    }
    int jobs = atoi(argv[1]);
    for (int j = 0; j < jobs; j++) { uint16_t st = app_job(j); printf("%d %04X %08X\n", j, st, app_checksum(j)); }
    return 0;
}
