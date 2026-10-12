#include "esp_log.h"
#include "host_fakes.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LINES = 4096, WIDTH = 384 };
static char lines[LINES][WIDTH];
static unsigned count;
static vprintf_like_t log_output = vprintf;

vprintf_like_t esp_log_set_vprintf(vprintf_like_t func)
{
    vprintf_like_t previous = log_output;
    log_output = func;
    return previous;
}

void host_log(char level, const char *tag, const char *format, ...)
{
    char *line = lines[count++ % LINES];
    int used = snprintf(line, WIDTH, "%c (%s) ", level, tag);
    va_list args;
    va_start(args, format);
    vsnprintf(line + used, WIDTH - (size_t)used, format, args);
    va_end(args);
    if (getenv("HOST_LOG")) fprintf(stderr, "%s\n", line);
}

void host_log_clear(void) { count = 0; }

unsigned host_log_count(const char *needle)
{
    unsigned found = 0;
    for (unsigned i = 0; i < (count < LINES ? count : LINES); ++i) found += strstr(lines[i], needle) != NULL;
    return found;
}

const char *host_log_last(const char *needle)
{
    for (unsigned i = count < LINES ? count : LINES; i-- > 0;) {
        const char *line = lines[(count - (count < LINES ? count : LINES) + i) % LINES];
        if (strstr(line, needle)) return line;
    }
    return NULL;
}

const char *esp_err_to_name(esp_err_t error) { return error == ESP_OK ? "ESP_OK" : "ESP_ERR"; }
