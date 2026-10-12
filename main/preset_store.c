#include "preset_store.h"
#include "game_storage.h"
#include "preset_json.h"
#include "save_file.h"
#include "esp_log.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define PATH_BYTES 192
#define MAX_USER_NUMBER 999u

static const char *TAG = "presets";
static const char *const DIRECTORIES[DISPLAY_MODE_COUNT] = {"dithered", "grayscale"};

static bool mode_directory(display_mode_t mode, char path[PATH_BYTES])
{
    if (mode >= DISPLAY_MODE_COUNT) return false;
    int n = snprintf(path, PATH_BYTES, "%s/inkboy/presets/%s", game_storage_root(), DIRECTORIES[mode]);
    return n > 0 && n < PATH_BYTES;
}

static bool file_path(display_mode_t mode, const char *file, char path[PATH_BYTES])
{
    char directory[PATH_BYTES];
    if (!mode_directory(mode, directory)) return false;
    int n = snprintf(path, PATH_BYTES, "%s/%s", directory, file);
    return n > 0 && n < PATH_BYTES;
}

static bool preset_file_name(const char *file)
{
    size_t length = strlen(file);
    return file[0] != '.' && length > 5 && length <= PRESET_FILE_NAME_MAX && !strcasecmp(file + length - 5, ".json");
}

static preset_store_status_t read_preset(const char *path, display_mode_t mode, char *scratch, display_preset_t *preset)
{
    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? PRESET_STORE_NOT_FOUND : PRESET_STORE_IO;
    size_t length = fread(scratch, 1, PRESET_FILE_MAX + 1, file);
    bool failed = ferror(file);
    fclose(file);
    if (failed) return PRESET_STORE_IO;
    scratch[length > PRESET_FILE_MAX ? PRESET_FILE_MAX : length] = 0;
    preset_json_status_t status = preset_json_decode(scratch, length, mode, preset);
    if (status == PRESET_JSON_OK) return PRESET_STORE_OK;
    ESP_LOGW(TAG, "ignoring %s: %s", path, preset_json_status_name(status));
    return PRESET_STORE_INVALID;
}

typedef bool (*visit_t)(void *context, const char *file, const display_preset_t *preset);

static preset_store_status_t scan(display_mode_t mode, visit_t visit, void *context)
{
    char directory[PATH_BYTES];
    if (!mode_directory(mode, directory)) return PRESET_STORE_IO;
    if (game_storage_mount() != ESP_OK) return PRESET_STORE_NO_CARD;
    DIR *dir = opendir(directory);
    if (!dir) return errno == ENOENT ? PRESET_STORE_OK : PRESET_STORE_IO;
    char *scratch = malloc(PRESET_FILE_MAX + 1);
    if (!scratch) {
        closedir(dir);
        return PRESET_STORE_IO;
    }
    const struct dirent *entry;
    while ((entry = readdir(dir))) {
        char path[PATH_BYTES];
        display_preset_t preset;
        if (!preset_file_name(entry->d_name) || !file_path(mode, entry->d_name, path)) continue;
        if (read_preset(path, mode, scratch, &preset) != PRESET_STORE_OK) continue;
        if (display_preset_is_default_name(preset.name)) {
            ESP_LOGI(TAG, "skipping %s: the name Default is reserved", path);
            continue;
        }
        if (!visit(context, entry->d_name, &preset)) break;
    }
    free(scratch);
    closedir(dir);
    return PRESET_STORE_OK;
}

static bool entry_before(const preset_entry_t *a, const preset_entry_t *b)
{
    int by_name = strcasecmp(a->name, b->name);
    return by_name ? by_name < 0 : strcmp(a->file, b->file) < 0;
}

typedef struct { preset_entry_t *entries; unsigned count; } list_t;

static bool collect(void *context, const char *file, const display_preset_t *preset)
{
    list_t *list = context;
    preset_entry_t candidate;
    snprintf(candidate.file, sizeof(candidate.file), "%s", file);
    snprintf(candidate.name, sizeof(candidate.name), "%s", preset->name);
    unsigned at = list->count;
    while (at > 0 && entry_before(&candidate, &list->entries[at - 1])) --at;
    if (at >= PRESET_LIST_MAX) return true;
    unsigned last = list->count < PRESET_LIST_MAX ? list->count : PRESET_LIST_MAX - 1;
    memmove(&list->entries[at + 1], &list->entries[at], (last - at) * sizeof(*list->entries));
    list->entries[at] = candidate;
    if (list->count < PRESET_LIST_MAX) ++list->count;
    return true;
}

preset_store_status_t preset_store_list(display_mode_t mode, preset_entry_t entries[PRESET_LIST_MAX], unsigned *count)
{
    list_t list = {entries, 0};
    preset_store_status_t status = scan(mode, collect, &list);
    *count = status == PRESET_STORE_OK ? list.count : 0;
    return status;
}

preset_store_status_t preset_store_load(display_mode_t mode, const char *file, display_preset_t *preset)
{
    char path[PATH_BYTES];
    if (!file || strchr(file, '/') || !preset_file_name(file) || !file_path(mode, file, path)) return PRESET_STORE_INVALID;
    if (game_storage_mount() != ESP_OK) return PRESET_STORE_NO_CARD;
    char *scratch = malloc(PRESET_FILE_MAX + 1);
    if (!scratch) return PRESET_STORE_IO;
    preset_store_status_t status = read_preset(path, mode, scratch, preset);
    free(scratch);
    return status;
}

static bool count_valid(void *context, const char *file, const display_preset_t *preset)
{
    (void)file; (void)preset;
    ++*(unsigned *)context;
    return true;
}

typedef struct { const char *name; char file[PRESET_FILE_NAME_MAX + 1]; } finder_t;

static bool find_name(void *context, const char *file, const display_preset_t *preset)
{
    finder_t *finder = context;
    if (!strcmp(preset->name, finder->name) && (!finder->file[0] || strcmp(file, finder->file) < 0))
        snprintf(finder->file, sizeof(finder->file), "%s", file);
    return true;
}

static bool make_directory(const char *path)
{
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

static bool make_directories(display_mode_t mode)
{
    char path[PATH_BYTES];
    const char *const parents[] = {"inkboy", "inkboy/presets"};
    for (unsigned i = 0; i < sizeof(parents) / sizeof(parents[0]); ++i) {
        int n = snprintf(path, sizeof(path), "%s/%s", game_storage_root(), parents[i]);
        if (n <= 0 || n >= PATH_BYTES || !make_directory(path)) return false;
    }
    return mode_directory(mode, path) && make_directory(path);
}

static preset_store_status_t write_preset(const char *path, const display_preset_t *preset)
{
    char *json = malloc(PRESET_JSON_MAX);
    if (!json) return PRESET_STORE_IO;
    size_t length;
    bool ok = preset_json_encode(preset, json, PRESET_JSON_MAX, &length) && save_file_replace(path, (const uint8_t *)json, length);
    free(json);
    return ok ? PRESET_STORE_OK : PRESET_STORE_IO;
}

preset_store_status_t preset_store_save_new(const display_preset_t *source, display_preset_t *saved)
{
    if (!display_preset_valid(source)) return PRESET_STORE_INVALID;
    unsigned valid = 0;
    preset_store_status_t status = scan(source->mode, count_valid, &valid);
    if (status != PRESET_STORE_OK) return status;
    if (valid >= PRESET_LIST_MAX) return PRESET_STORE_FULL;
    if (!make_directories(source->mode)) return PRESET_STORE_IO;
    char file[PRESET_FILE_NAME_MAX + 1], path[PATH_BYTES];
    struct stat info;
    unsigned number = 1;
    for (; number <= MAX_USER_NUMBER; ++number) {
        snprintf(file, sizeof(file), "user-%u.json", number);
        if (!file_path(source->mode, file, path)) return PRESET_STORE_IO;
        if (stat(path, &info) != 0) break;
    }
    if (number > MAX_USER_NUMBER) return PRESET_STORE_FULL;
    display_preset_t named = *source;
    snprintf(named.name, sizeof(named.name), "User %u", number);
    status = write_preset(path, &named);
    if (status == PRESET_STORE_OK) *saved = named;
    return status;
}

preset_store_status_t preset_store_overwrite(const display_preset_t *preset)
{
    if (!display_preset_valid(preset) || display_preset_is_default_name(preset->name)) return PRESET_STORE_INVALID;
    finder_t finder = {.name = preset->name};
    preset_store_status_t status = scan(preset->mode, find_name, &finder);
    if (status != PRESET_STORE_OK) return status;
    char path[PATH_BYTES];
    if (!finder.file[0]) return PRESET_STORE_NOT_FOUND;
    return file_path(preset->mode, finder.file, path) ? write_preset(path, preset) : PRESET_STORE_IO;
}
