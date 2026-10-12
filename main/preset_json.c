#include "preset_json.h"
#include "cJSON.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define FORMAT "inkboy-preset-v1"
/* Real presets nest four deep; cJSON would otherwise recurse up to its 1000 limit on a small stack. */
#define MAX_NESTING 8

static const char *const MODES[DISPLAY_MODE_COUNT] = {"dithered", "grayscale"};
static const char *const SHADES[4] = {"K", "D", "L", "W"};
static const char *const SCAN_POLICIES[] = {"auto", "one"};
static const char *const STRENGTHS[] = {"stock", "-13V", "-11V", "-9V"};
static const char *const HISTORY_MODES[] = {"off", "returns", "reversals", "both"};
static const char *const WEAK_WHITE_TO_LIGHT[] = {"normal", "11V", "8.8V"};
static const char *const ROUTES[GRAY_ROUTES] = {"L>K>L", "D>K>D", "D>W>D", "L>W>L", "D>L>D", "L>D>L"};
#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

typedef struct { char *buffer; size_t size, used; bool overflow; } writer_t;

static void put(writer_t *w, const char *format, ...) __attribute__((format(printf, 2, 3)));
static void put(writer_t *w, const char *format, ...)
{
    if (w->overflow) return;
    va_list arguments;
    va_start(arguments, format);
    int n = vsnprintf(w->buffer + w->used, w->size - w->used, format, arguments);
    va_end(arguments);
    if (n < 0 || (size_t)n >= w->size - w->used) w->overflow = true;
    else w->used += (size_t)n;
}

static void put_gray(writer_t *w, const gray_profile_t *g)
{
    put(w, "{\n    \"doses\": {\n");
    for (unsigned from = 0; from < 4; ++from)
        put(w, "      \"%s\": [%u, %u, %u, %u]%s\n", SHADES[from], g->tuning.dose[from][0], g->tuning.dose[from][1],
            g->tuning.dose[from][2], g->tuning.dose[from][3], from < 3 ? "," : "");
    put(w, "    },\n    \"retarget_percent\": %u,\n", gray_retarget_percent(&g->tuning));
    put(w, "    \"scan_policy\": \"%s\",\n    \"strength\": \"%s\",\n    \"pair_hold\": %s,\n",
        SCAN_POLICIES[g->tuning.scan_policy], STRENGTHS[g->strength], g->immediate ? "true" : "false");
    put(w, "    \"holds\": {\n");
    for (unsigned from = 0; from < 4; ++from) {
        const unsigned holds = gray_profile_holds(g);
        put(w, "      \"%s\": [", SHADES[from]);
        for (unsigned to = 0; to < 4; ++to) put(w, "%s%s", holds & (1u << (from * 4 + to)) ? "true" : "false", to < 3 ? ", " : "");
        put(w, "]%s\n", from < 3 ? "," : "");
    }
    put(w, "    },\n    \"history\": \"%s\",\n    \"weak_white_to_light\": \"%s\",\n    \"routes\": {\n",
        HISTORY_MODES[g->adaptive.history_mode], WEAK_WHITE_TO_LIGHT[g->adaptive.weak_wl]);
    for (unsigned route = 0; route < GRAY_ROUTES; ++route) {
        put(w, "      \"%s\": { \"return\": %d, \"reverse_percent\": ", ROUTES[route], g->adaptive.return_delta[route]);
        if (g->adaptive.reverse_percent[route]) put(w, "%u", g->adaptive.reverse_percent[route]);
        else put(w, "null");
        put(w, " }%s\n", route + 1 < GRAY_ROUTES ? "," : "");
    }
    put(w, "    }\n  }");
}

bool preset_json_encode(const display_preset_t *preset, char *json, size_t size, size_t *length)
{
    if (!display_preset_valid(preset) || !json || !size) return false;
    writer_t w = {json, size, 0, false};
    put(&w, "{\n  \"format\": \"" FORMAT "\",\n  \"mode\": \"%s\",\n  \"name\": \"%s\",\n  \"settings\": ",
        MODES[preset->mode], preset->name);
    if (preset->mode == DISPLAY_DITHERED)
        put(&w, "{ \"black_pulses\": %u, \"white_pulses\": %u }", preset->black_pulses, preset->white_pulses);
    else put_gray(&w, &preset->gray);
    put(&w, "\n}\n");
    if (w.overflow) return false;
    if (length) *length = w.used;
    return true;
}

static bool nesting_within_limit(const char *json, size_t length)
{
    unsigned depth = 0;
    bool in_string = false;
    for (size_t i = 0; i < length; ++i) {
        const char c = json[i];
        if (in_string) {
            if (c == '\\') ++i;
            else if (c == '"') in_string = false;
        } else if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            if (++depth > MAX_NESTING) return false;
        } else if ((c == '}' || c == ']') && depth) {
            --depth;
        }
    }
    return true;
}

static preset_json_status_t member(const cJSON *object, const char *key, const cJSON **value)
{
    *value = cJSON_GetObjectItemCaseSensitive(object, key);
    return *value ? PRESET_JSON_OK : PRESET_JSON_MISSING_KEY;
}

static preset_json_status_t as_int(const cJSON *node, int low, int high, int *value)
{
    if (!cJSON_IsNumber(node) || node->valuedouble != floor(node->valuedouble) ||
        node->valuedouble < low || node->valuedouble > high) return PRESET_JSON_BAD_VALUE;
    *value = (int)node->valuedouble;
    return PRESET_JSON_OK;
}

static preset_json_status_t member_int(const cJSON *object, const char *key, int low, int high, int *value)
{
    const cJSON *node;
    preset_json_status_t status = member(object, key, &node);
    return status ? status : as_int(node, low, high, value);
}

static preset_json_status_t member_object(const cJSON *object, const char *key, const cJSON **value)
{
    preset_json_status_t status = member(object, key, value);
    return status ? status : cJSON_IsObject(*value) ? PRESET_JSON_OK : PRESET_JSON_BAD_VALUE;
}

static preset_json_status_t member_choice(const cJSON *object, const char *key, const char *const *names, size_t count, unsigned *index)
{
    const cJSON *node;
    preset_json_status_t status = member(object, key, &node);
    if (status) return status;
    if (!cJSON_IsString(node)) return PRESET_JSON_BAD_VALUE;
    for (size_t i = 0; i < count; ++i)
        if (!strcmp(node->valuestring, names[i])) { *index = (unsigned)i; return PRESET_JSON_OK; }
    return PRESET_JSON_BAD_VALUE;
}

static preset_json_status_t member_array4(const cJSON *object, const char *key, const cJSON *elements[4])
{
    const cJSON *node;
    preset_json_status_t status = member(object, key, &node);
    if (status) return status;
    if (!cJSON_IsArray(node) || cJSON_GetArraySize(node) != 4) return PRESET_JSON_BAD_VALUE;
    for (int i = 0; i < 4; ++i) elements[i] = cJSON_GetArrayItem(node, i);
    return PRESET_JSON_OK;
}

static preset_json_status_t decode_gray(const cJSON *settings, gray_profile_t *g)
{
    preset_json_status_t status;
    const cJSON *section, *elements[4];
    unsigned choice;
    int number;
    *g = (gray_profile_t){.version = GRAY_PROFILE_VERSION, .tuning.version = 1};
    if ((status = member_object(settings, "doses", &section))) return status;
    for (unsigned from = 0; from < 4; ++from) {
        if ((status = member_array4(section, SHADES[from], elements))) return status;
        for (unsigned to = 0; to < 4; ++to) {
            if ((status = as_int(elements[to], 0, 255, &number))) return status;
            g->tuning.dose[from][to] = (uint8_t)number;
        }
    }
    if ((status = member_int(settings, "retarget_percent", 0, 255, &number))) return status;
    if (number < 50 || (number - 50) % 25 || number > 150) return PRESET_JSON_BAD_VALUE;
    g->tuning.retarget_gain = (uint8_t)((number - 50) / 25);
    if ((status = member_choice(settings, "scan_policy", SCAN_POLICIES, COUNT(SCAN_POLICIES), &choice))) return status;
    g->tuning.scan_policy = (uint8_t)choice;
    if ((status = member_choice(settings, "strength", STRENGTHS, COUNT(STRENGTHS), &choice))) return status;
    g->strength = (uint8_t)choice;
    const cJSON *flag;
    if ((status = member(settings, "pair_hold", &flag))) return status;
    if (!cJSON_IsBool(flag)) return PRESET_JSON_BAD_VALUE;
    g->immediate = cJSON_IsTrue(flag);
    unsigned holds = 0;
    if ((status = member_object(settings, "holds", &section))) return status;
    for (unsigned from = 0; from < 4; ++from) {
        if ((status = member_array4(section, SHADES[from], elements))) return status;
        for (unsigned to = 0; to < 4; ++to) {
            if (!cJSON_IsBool(elements[to])) return PRESET_JSON_BAD_VALUE;
            if (cJSON_IsTrue(elements[to])) holds |= 1u << (from * 4 + to);
        }
    }
    g->holds_lo = (uint8_t)(holds & 255);
    g->holds_hi = (uint8_t)(holds >> 8);
    if ((status = member_choice(settings, "history", HISTORY_MODES, COUNT(HISTORY_MODES), &choice))) return status;
    g->adaptive.history_mode = (uint8_t)choice;
    if ((status = member_choice(settings, "weak_white_to_light", WEAK_WHITE_TO_LIGHT, COUNT(WEAK_WHITE_TO_LIGHT), &choice))) return status;
    g->adaptive.weak_wl = (uint8_t)choice;
    if ((status = member_object(settings, "routes", &section))) return status;
    for (unsigned route = 0; route < GRAY_ROUTES; ++route) {
        const cJSON *entry, *percent;
        if ((status = member_object(section, ROUTES[route], &entry))) return status;
        if ((status = member_int(entry, "return", -128, 127, &number))) return status;
        g->adaptive.return_delta[route] = (int8_t)number;
        if ((status = member(entry, "reverse_percent", &percent))) return status;
        if (cJSON_IsNull(percent)) continue;
        if ((status = as_int(percent, 1, 255, &number))) return status;
        g->adaptive.reverse_percent[route] = (uint8_t)number;
    }
    return gray_profile_valid(g) ? PRESET_JSON_OK : PRESET_JSON_BAD_VALUE;
}

static preset_json_status_t decode_root(const cJSON *root, display_mode_t mode, display_preset_t *preset)
{
    preset_json_status_t status;
    const cJSON *node, *settings;
    unsigned declared_mode;
    if (!cJSON_IsObject(root)) return PRESET_JSON_MALFORMED;
    if ((status = member(root, "format", &node))) return status;
    if (!cJSON_IsString(node) || strcmp(node->valuestring, FORMAT)) return PRESET_JSON_WRONG_FORMAT;
    if ((status = member_choice(root, "mode", MODES, COUNT(MODES), &declared_mode))) return status;
    if (declared_mode != mode) return PRESET_JSON_WRONG_MODE;
    if ((status = member(root, "name", &node))) return status;
    if (!cJSON_IsString(node) || !display_preset_name_valid(node->valuestring)) return PRESET_JSON_BAD_VALUE;
    *preset = (display_preset_t){.mode = mode};
    snprintf(preset->name, sizeof(preset->name), "%s", node->valuestring);
    if ((status = member_object(root, "settings", &settings))) return status;
    if (mode == DISPLAY_GRAYSCALE) status = decode_gray(settings, &preset->gray);
    else {
        int black, white;
        if ((status = member_int(settings, "black_pulses", 0, 255, &black))) return status;
        if ((status = member_int(settings, "white_pulses", 0, 255, &white))) return status;
        preset->black_pulses = (uint8_t)black;
        preset->white_pulses = (uint8_t)white;
    }
    if (status) return status;
    return display_preset_valid(preset) ? PRESET_JSON_OK : PRESET_JSON_BAD_VALUE;
}

preset_json_status_t preset_json_decode(const char *json, size_t length, display_mode_t mode, display_preset_t *preset)
{
    if (length > PRESET_FILE_MAX) return PRESET_JSON_TOO_LARGE;
    if (mode >= DISPLAY_MODE_COUNT || memchr(json, 0, length) || !nesting_within_limit(json, length)) return PRESET_JSON_MALFORMED;
    cJSON *root = cJSON_ParseWithLengthOpts(json, length + 1, NULL, true);
    if (!root) return PRESET_JSON_MALFORMED;
    display_preset_t decoded;
    preset_json_status_t status = decode_root(root, mode, &decoded);
    cJSON_Delete(root);
    if (!status) *preset = decoded;
    return status;
}

const char *preset_json_status_name(preset_json_status_t status)
{
    static const char *const names[] = {"ok", "file too large", "malformed JSON", "wrong format", "wrong mode",
                                        "missing key", "bad value"};
    return (size_t)status < COUNT(names) ? names[status] : "unknown";
}
