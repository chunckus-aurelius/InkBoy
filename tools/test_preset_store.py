#!/usr/bin/env python3
"""Scan, load, number and atomically write presets in a real temporary card directory."""
from host_build import CJSON, PRESET_STORE_MODULES, SAVE_FILE, run

CODE = r'''
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fake_game_storage.h"
#include "host_fakes.h"
#include "preset_json.h"
#include "preset_store.h"

static char root[256];

static void make_dir(const char *path) { assert(mkdir(path, 0777) == 0 || access(path, F_OK) == 0); }

static void begin(void)
{
    host_fakes_reset();
    fake_storage_reset();
    assert(chdir(getenv("HOST_TEST_DIR")) == 0);
    snprintf(root, sizeof(root), "."); /* Keeps paths short however deep the scratch directory is. */
    snprintf(fake_storage.root, sizeof(fake_storage.root), "%s", root);
}

static void mode_dir(display_mode_t mode, char *path, size_t size)
{
    snprintf(path, size, "%s/inkboy", root); make_dir(path);
    snprintf(path, size, "%s/inkboy/presets", root); make_dir(path);
    snprintf(path, size, "%s/inkboy/presets/%s", root, mode == DISPLAY_DITHERED ? "dithered" : "grayscale"); make_dir(path);
}

static void put_text(display_mode_t mode, const char *file, const void *text, size_t length)
{
    char directory[300], path[400];
    mode_dir(mode, directory, sizeof(directory));
    snprintf(path, sizeof(path), "%s/%s", directory, file);
    FILE *f = fopen(path, "wb");
    assert(f && fwrite(text, 1, length, f) == length);
    fclose(f);
}

static display_preset_t dithered(const char *name, unsigned black, unsigned white)
{
    display_preset_t p = {.mode = DISPLAY_DITHERED, .black_pulses = (uint8_t)black, .white_pulses = (uint8_t)white};
    snprintf(p.name, sizeof(p.name), "%s", name);
    assert(display_preset_valid(&p));
    return p;
}

static display_preset_t gray(const char *name, unsigned dose)
{
    display_preset_t p = *display_preset_default(DISPLAY_GRAYSCALE);
    snprintf(p.name, sizeof(p.name), "%s", name);
    p.gray.tuning.dose[0][1] = (uint8_t)dose;
    assert(display_preset_valid(&p));
    return p;
}

static void put_preset_in(display_mode_t directory, const char *file, const display_preset_t *p)
{
    char json[PRESET_JSON_MAX];
    size_t length;
    assert(preset_json_encode(p, json, sizeof(json), &length));
    put_text(directory, file, json, length);
}

static void put_preset(const char *file, const display_preset_t *p) { put_preset_in(p->mode, file, p); }

static char *read_all(display_mode_t mode, const char *file, size_t *length)
{
    char directory[300], path[400];
    mode_dir(mode, directory, sizeof(directory));
    snprintf(path, sizeof(path), "%s/%s", directory, file);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *text = calloc(1, 9000);
    *length = fread(text, 1, 8999, f);
    fclose(f);
    return text;
}

static void unlink_user(display_mode_t mode, const char *file)
{
    char directory[300], path[400];
    mode_dir(mode, directory, sizeof(directory));
    snprintf(path, sizeof(path), "%s/%s", directory, file);
    assert(unlink(path) == 0);
}

static bool exists(display_mode_t mode, const char *file)
{
    size_t length;
    char *text = read_all(mode, file, &length);
    free(text);
    return text != NULL;
}

static void names_are(const preset_entry_t *entries, unsigned count, const char *expected)
{
    char joined[512] = "";
    for (unsigned i = 0; i < count; ++i) { strcat(joined, i ? "," : ""); strcat(joined, entries[i].name); }
    if (strcmp(joined, expected)) { fprintf(stderr, "got %s, expected %s\n", joined, expected); assert(0); }
}

static void missing_directory_is_empty(void)
{
    begin();
    preset_entry_t entries[PRESET_LIST_MAX];
    unsigned count = 99;
    assert(preset_store_list(DISPLAY_DITHERED, entries, &count) == PRESET_STORE_OK && count == 0);
    char directory[300];
    mode_dir(DISPLAY_DITHERED, directory, sizeof(directory));
    assert(preset_store_list(DISPLAY_GRAYSCALE, entries, &count) == PRESET_STORE_OK && count == 0);
    assert(preset_store_list(DISPLAY_DITHERED, entries, &count) == PRESET_STORE_OK && count == 0);
    assert(preset_store_list(DISPLAY_MODE_COUNT, entries, &count) == PRESET_STORE_IO && count == 0);
}

static void lists_sorted_and_skips_invalid(void)
{
    begin();
    display_preset_t d = dithered("zeta", 1, 1);
    put_preset("z.json", &d);
    d = dithered("Alpha", 2, 2); put_preset("a.json", &d);
    d = dithered("beta", 3, 3); put_preset("Upper.JSON", &d);
    d = dithered("Alpha", 4, 4); put_preset("a-second.json", &d);
    d = dithered("Default", 5, 5); put_preset("default.json", &d);
    d = dithered("DEFAULT", 5, 5); put_preset("shout.json", &d);
    d = gray("Gray One", 3); put_preset("gray.json", &d); put_preset_in(DISPLAY_DITHERED, "misfiled.json", &d);
    put_text(DISPLAY_DITHERED, "broken.json", "{\"format\":", 10);
    put_text(DISPLAY_DITHERED, "empty.json", "", 0);
    const char *no_settings = "{\"format\": \"inkboy-preset-v1\", \"mode\": \"dithered\", \"name\": \"X\"}";
    put_text(DISPLAY_DITHERED, "missing.json", no_settings, strlen(no_settings));
    char big[9000];
    memset(big, ' ', sizeof(big));
    put_text(DISPLAY_DITHERED, "big.json", big, sizeof(big));
    put_text(DISPLAY_DITHERED, "notes.txt", "hello", 5);
    put_text(DISPLAY_DITHERED, "a.json.bak", "x", 1);
    put_text(DISPLAY_DITHERED, "a.json.tmp", "x", 1);
    put_text(DISPLAY_DITHERED, "._a.json", "x", 1);
    put_text(DISPLAY_DITHERED, ".hidden.json", "x", 1);
    put_text(DISPLAY_DITHERED, "x.json.json.bak", "x", 1);
    put_text(DISPLAY_DITHERED, "name-that-is-far-too-long-for-the-menu.json", "{}", 2);
    char directory[300], path[400];
    mode_dir(DISPLAY_DITHERED, directory, sizeof(directory));
    snprintf(path, sizeof(path), "%s/folder.json", directory);
    make_dir(path);

    preset_entry_t entries[PRESET_LIST_MAX];
    unsigned count = 0;
    host_log_clear();
    assert(preset_store_list(DISPLAY_DITHERED, entries, &count) == PRESET_STORE_OK);
    names_are(entries, count, "Alpha,Alpha,beta,zeta");
    assert(!strcmp(entries[0].file, "a-second.json") && !strcmp(entries[1].file, "a.json") && !strcmp(entries[2].file, "Upper.JSON"));
    assert(host_log_count("ignoring") == 5 && host_log_count("reserved") == 2);
    assert(host_log_count("broken.json: malformed JSON") == 1 && host_log_count("big.json: file too large") == 1);
    assert(host_log_count("misfiled.json: wrong mode") == 1 && host_log_count("missing.json: missing key") == 1 && host_log_count("empty.json") == 1);
    assert(host_log_count("name-that-is-far") == 0);
    count = 0;
    assert(preset_store_list(DISPLAY_GRAYSCALE, entries, &count) == PRESET_STORE_OK && count == 1);
    names_are(entries, count, "Gray One");
}

static void caps_the_list(unsigned reverse)
{
    begin();
    for (unsigned i = 0; i < 20; ++i) {
        unsigned n = reverse ? 20 - i : i + 1;
        char name[16], file[32];
        snprintf(name, sizeof(name), "P%02u", n);
        snprintf(file, sizeof(file), "f%02u.json", (n * 7) % 20);
        display_preset_t p = dithered(name, 1 + n % 8, 2);
        put_preset(file, &p);
    }
    preset_entry_t entries[PRESET_LIST_MAX];
    unsigned count = 0;
    assert(preset_store_list(DISPLAY_DITHERED, entries, &count) == PRESET_STORE_OK && count == PRESET_LIST_MAX);
    for (unsigned i = 0; i < count; ++i) {
        char name[16];
        snprintf(name, sizeof(name), "P%02u", i + 1);
        assert(!strcmp(entries[i].name, name));
    }
    display_preset_t saved, source = dithered("Default", 3, 3);
    assert(preset_store_save_new(&source, &saved) == PRESET_STORE_FULL && !exists(DISPLAY_DITHERED, "user-1.json"));
}

static void save_new_numbers_files(void)
{
    begin();
    display_preset_t source = dithered("Default", 6, 7), saved, loaded;
    assert(preset_store_save_new(&source, &saved) == PRESET_STORE_OK);
    assert(!strcmp(saved.name, "User 1") && saved.black_pulses == 6 && saved.white_pulses == 7 && saved.mode == DISPLAY_DITHERED);
    assert(!strcmp(source.name, "Default"));
    size_t length;
    char *text = read_all(DISPLAY_DITHERED, "user-1.json", &length);
    char expected[PRESET_JSON_MAX];
    size_t expected_length;
    assert(text && preset_json_encode(&saved, expected, sizeof(expected), &expected_length) && length == expected_length && !memcmp(text, expected, length));
    free(text);
    assert(preset_store_load(DISPLAY_DITHERED, "user-1.json", &loaded) == PRESET_STORE_OK && display_preset_same_settings(&loaded, &saved));
    /* Gaps are filled from the lowest number, and an unreadable file still occupies its number. */
    display_preset_t other = dithered("Other", 1, 1);
    put_preset("user-3.json", &other);
    put_text(DISPLAY_DITHERED, "user-2.json", "garbage", 7);
    assert(preset_store_save_new(&source, &saved) == PRESET_STORE_OK && !strcmp(saved.name, "User 4"));
    unlink_user(DISPLAY_DITHERED, "user-2.json");
    assert(preset_store_save_new(&source, &saved) == PRESET_STORE_OK && !strcmp(saved.name, "User 2"));
    unlink_user(DISPLAY_DITHERED, "user-1.json");
    assert(preset_store_save_new(&source, &saved) == PRESET_STORE_OK && !strcmp(saved.name, "User 1"));
    text = read_all(DISPLAY_DITHERED, "user-3.json", &length);
    assert(text && strstr(text, "\"Other\""));
    free(text);
    preset_entry_t entries[PRESET_LIST_MAX];
    unsigned count;
    assert(preset_store_list(DISPLAY_DITHERED, entries, &count) == PRESET_STORE_OK);
    names_are(entries, count, "Other,User 1,User 2,User 4");
    /* The other mode has its own directory and numbering. */
    display_preset_t g = gray("Default", 5);
    assert(preset_store_save_new(&g, &saved) == PRESET_STORE_OK && !strcmp(saved.name, "User 1") && saved.mode == DISPLAY_GRAYSCALE);
    assert(preset_store_load(DISPLAY_GRAYSCALE, "user-1.json", &loaded) == PRESET_STORE_OK && loaded.gray.tuning.dose[0][1] == 5);
    assert(preset_store_load(DISPLAY_DITHERED, "user-1.json", &loaded) == PRESET_STORE_OK && loaded.mode == DISPLAY_DITHERED);
    display_preset_t bad = source;
    bad.black_pulses = 0;
    assert(preset_store_save_new(&bad, &saved) == PRESET_STORE_INVALID);
}

static void overwrite_by_name(void)
{
    begin();
    display_preset_t mine = gray("Mine", 4), copy = mine, loaded;
    put_preset("renamed-file.json", &mine);
    copy.gray.tuning.dose[0][1] = 9;
    copy.gray.strength = 3;
    assert(preset_store_overwrite(&copy) == PRESET_STORE_OK);
    assert(preset_store_load(DISPLAY_GRAYSCALE, "renamed-file.json", &loaded) == PRESET_STORE_OK && display_preset_same_settings(&loaded, &copy));
    assert(preset_store_load(DISPLAY_GRAYSCALE, "renamed-file.json.bak", &loaded) == PRESET_STORE_INVALID);
    size_t length;
    char *backup = read_all(DISPLAY_GRAYSCALE, "renamed-file.json.bak", &length);
    display_preset_t previous;
    assert(backup && preset_json_decode(backup, length, DISPLAY_GRAYSCALE, &previous) == PRESET_JSON_OK && display_preset_same_settings(&previous, &mine));
    free(backup);
    assert(!exists(DISPLAY_GRAYSCALE, "renamed-file.json.tmp") && !exists(DISPLAY_GRAYSCALE, "user-1.json"));
    display_preset_t absent = gray("Nobody", 3), reserved = gray("default", 3);
    assert(preset_store_overwrite(&absent) == PRESET_STORE_NOT_FOUND && preset_store_overwrite(&reserved) == PRESET_STORE_INVALID);
    display_preset_t invalid = copy;
    invalid.gray.strength = 7;
    assert(preset_store_overwrite(&invalid) == PRESET_STORE_INVALID);
    /* Duplicated names resolve to the first file name so the result does not depend on directory order. */
    put_preset("b.json", &mine);
    put_preset("a.json", &mine);
    copy.gray.tuning.dose[0][1] = 11;
    assert(preset_store_overwrite(&copy) == PRESET_STORE_OK);
    assert(preset_store_load(DISPLAY_GRAYSCALE, "a.json", &loaded) == PRESET_STORE_OK && loaded.gray.tuning.dose[0][1] == 11);
    assert(preset_store_load(DISPLAY_GRAYSCALE, "b.json", &loaded) == PRESET_STORE_OK && loaded.gray.tuning.dose[0][1] == 4);
}

static void failed_write_keeps_file(void)
{
    begin();
    display_preset_t mine = dithered("Mine", 2, 2), saved, loaded;
    assert(preset_store_save_new(&mine, &saved) == PRESET_STORE_OK);
    mine = saved;
    display_preset_t update = saved;
    update.black_pulses = 7;
    char directory[300];
    mode_dir(DISPLAY_DITHERED, directory, sizeof(directory));
    if (geteuid() == 0) return;
    assert(chmod(directory, 0555) == 0);
    assert(preset_store_overwrite(&update) == PRESET_STORE_IO);
    assert(preset_store_save_new(&mine, &saved) == PRESET_STORE_IO);
    assert(chmod(directory, 0755) == 0);
    assert(preset_store_load(DISPLAY_DITHERED, "user-1.json", &loaded) == PRESET_STORE_OK && loaded.black_pulses == 2);
    assert(!exists(DISPLAY_DITHERED, "user-2.json") && !exists(DISPLAY_DITHERED, "user-1.json.tmp"));
    char blocked[400];
    snprintf(blocked, sizeof(blocked), "%s/inkboy", root);
    char moved[400];
    snprintf(moved, sizeof(moved), "%s/inkboy-away", root);
    assert(rename(blocked, moved) == 0);
    FILE *f = fopen(blocked, "wb");
    assert(f);
    fclose(f);
    assert(preset_store_save_new(&mine, &saved) == PRESET_STORE_IO);
}

static void loads_validate(void)
{
    begin();
    display_preset_t d = dithered("Mine", 2, 5), loaded, sentinel;
    put_preset("mine.json", &d);
    display_preset_t g = gray("G", 3);
    put_preset_in(DISPLAY_DITHERED, "wrong-mode.json", &g);
    put_text(DISPLAY_DITHERED, "junk.json", "junk", 4);
    memset(&sentinel, 0xa5, sizeof(sentinel));
    loaded = sentinel;
    assert(preset_store_load(DISPLAY_DITHERED, "mine.json", &loaded) == PRESET_STORE_OK && !strcmp(loaded.name, "Mine") && loaded.white_pulses == 5);
    loaded = sentinel;
    assert(preset_store_load(DISPLAY_DITHERED, "wrong-mode.json", &loaded) == PRESET_STORE_INVALID);
    assert(preset_store_load(DISPLAY_DITHERED, "junk.json", &loaded) == PRESET_STORE_INVALID);
    assert(preset_store_load(DISPLAY_DITHERED, "gone.json", &loaded) == PRESET_STORE_NOT_FOUND);
    assert(preset_store_load(DISPLAY_DITHERED, "../dithered/mine.json", &loaded) == PRESET_STORE_INVALID);
    assert(preset_store_load(DISPLAY_DITHERED, "/mine.json", &loaded) == PRESET_STORE_INVALID);
    assert(preset_store_load(DISPLAY_DITHERED, "mine.txt", &loaded) == PRESET_STORE_INVALID);
    assert(preset_store_load(DISPLAY_DITHERED, ".json", &loaded) == PRESET_STORE_INVALID);
    assert(preset_store_load(DISPLAY_DITHERED, NULL, &loaded) == PRESET_STORE_INVALID);
    assert(!memcmp(&loaded, &sentinel, sizeof(loaded))); /* A rejected load leaves the caller's value alone. */
}

static void no_card(void)
{
    begin();
    display_preset_t d = dithered("Mine", 2, 3), saved, loaded;
    put_preset("mine.json", &d);
    fake_storage.mount_error = ESP_FAIL;
    preset_entry_t entries[PRESET_LIST_MAX];
    unsigned count = 5;
    assert(preset_store_list(DISPLAY_DITHERED, entries, &count) == PRESET_STORE_NO_CARD && count == 0);
    assert(preset_store_load(DISPLAY_DITHERED, "mine.json", &loaded) == PRESET_STORE_NO_CARD);
    assert(preset_store_save_new(&d, &saved) == PRESET_STORE_NO_CARD);
    assert(preset_store_overwrite(&d) == PRESET_STORE_NO_CARD);
    assert(!exists(DISPLAY_DITHERED, "user-1.json"));
    /* The active settings come from NVS, so a missing card never changes them. */
    assert(display_preset_load_active() == ESP_OK);
    assert(display_preset_set_active(&d, true) == ESP_OK);
    assert(display_preset_load_active() == ESP_OK && display_preset_active(DISPLAY_DITHERED)->black_pulses == 2);
    assert(!strcmp(display_preset_active(DISPLAY_DITHERED)->name, "Mine") && display_preset_active_modified(DISPLAY_DITHERED));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "missing")) missing_directory_is_empty();
    else if (!strcmp(argv[1], "list")) lists_sorted_and_skips_invalid();
    else if (!strcmp(argv[1], "cap")) caps_the_list(0);
    else if (!strcmp(argv[1], "cap_reversed")) caps_the_list(1);
    else if (!strcmp(argv[1], "save_new")) save_new_numbers_files();
    else if (!strcmp(argv[1], "overwrite")) overwrite_by_name();
    else if (!strcmp(argv[1], "write_failure")) failed_write_keeps_file();
    else if (!strcmp(argv[1], "load")) loads_validate();
    else if (!strcmp(argv[1], "nocard")) no_card();
    else assert(0);
    return 0;
}
'''

run(CODE, modules=PRESET_STORE_MODULES, fakes=['fake_game_storage.c'], extra_sources=[SAVE_FILE], vendor_sources=[CJSON],
    scenarios=[[name] for name in ('missing', 'list', 'cap', 'cap_reversed', 'save_new', 'overwrite', 'write_failure', 'load', 'nocard')])
print('PASS: card listing order/cap/skips, load validation, user-N numbering, atomic overwrite with backup, write failures, missing card')
