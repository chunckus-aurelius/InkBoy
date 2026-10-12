#include "nvs.h"
#include "nvs_flash.h"
#include "host_fakes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { ENTRIES = 48, HANDLES = 8, STAGED = 8 };

typedef struct { char space[16], key[16]; host_nvs_entry_t value; bool used; } stored_t;
typedef struct { bool erase; stored_t item; } staged_t;
typedef struct { bool open; bool writable; char space[16]; staged_t staged[STAGED]; unsigned staged_count; } handle_t;

static stored_t store[ENTRIES];
static handle_t handles[HANDLES];
esp_err_t host_nvs_open_error, host_nvs_get_error, host_nvs_set_error, host_nvs_commit_error;
unsigned host_nvs_commits;

void host_nvs_reset(void)
{
    memset(store, 0, sizeof(store));
    memset(handles, 0, sizeof(handles));
    host_nvs_open_error = host_nvs_get_error = host_nvs_set_error = host_nvs_commit_error = ESP_OK;
    host_nvs_commits = 0;
}

static stored_t *lookup(const char *space, const char *key)
{
    for (unsigned i = 0; i < ENTRIES; ++i)
        if (store[i].used && !strcmp(store[i].space, space) && !strcmp(store[i].key, key)) return &store[i];
    return NULL;
}

static void apply(const staged_t *op)
{
    stored_t *slot = lookup(op->item.space, op->item.key);
    if (op->erase) {
        if (slot) memset(slot, 0, sizeof(*slot));
        return;
    }
    for (unsigned i = 0; !slot && i < ENTRIES; ++i) if (!store[i].used) slot = &store[i];
    assert(slot);
    *slot = op->item;
    slot->used = true;
}

void host_nvs_seed(const char *space, const char *key, host_nvs_type_t type, const void *data, size_t size)
{
    staged_t op = {0};
    assert(size <= sizeof(op.item.value.data));
    snprintf(op.item.space, sizeof(op.item.space), "%s", space);
    snprintf(op.item.key, sizeof(op.item.key), "%s", key);
    op.item.value.type = type;
    op.item.value.size = size;
    memcpy(op.item.value.data, data, size);
    apply(&op);
}

const host_nvs_entry_t *host_nvs_find(const char *space, const char *key)
{
    stored_t *slot = lookup(space, key);
    return slot ? &slot->value : NULL;
}

uint8_t host_nvs_u8(const char *space, const char *key)
{
    const host_nvs_entry_t *entry = host_nvs_find(space, key);
    assert(entry && entry->type == HOST_NVS_U8);
    return entry->data[0];
}

void host_nvs_restore(const char *space, const char *key, const host_nvs_entry_t *entry)
{
    host_nvs_seed(space, key, entry->type, entry->data, entry->size);
}

void host_nvs_erase(const char *space, const char *key)
{
    stored_t *slot = lookup(space, key);
    if (slot) memset(slot, 0, sizeof(*slot));
}

esp_err_t nvs_flash_init(void) { return ESP_OK; }

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    if (host_nvs_open_error) return host_nvs_open_error;
    for (unsigned i = 1; i < HANDLES; ++i) {
        if (handles[i].open) continue;
        handles[i] = (handle_t){.open = true, .writable = mode == NVS_READWRITE};
        snprintf(handles[i].space, sizeof(handles[i].space), "%s", name);
        *handle = i;
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

void nvs_close(nvs_handle_t handle)
{
    assert(handle && handle < HANDLES && handles[handle].open);
    handles[handle].open = false;
}

static handle_t *use(nvs_handle_t handle)
{
    assert(handle && handle < HANDLES && handles[handle].open);
    return &handles[handle];
}

static esp_err_t get(nvs_handle_t handle, const char *key, host_nvs_type_t type, void *out, size_t *length)
{
    handle_t *h = use(handle);
    if (host_nvs_get_error) return host_nvs_get_error;
    const host_nvs_entry_t *entry = NULL;
    stored_t *slot = lookup(h->space, key);
    if (slot) entry = &slot->value;
    for (unsigned i = 0; i < h->staged_count; ++i)
        if (!strcmp(h->staged[i].item.key, key)) entry = h->staged[i].erase ? NULL : &h->staged[i].item.value;
    if (!entry) return ESP_ERR_NVS_NOT_FOUND;
    if (entry->type != type) return ESP_ERR_NVS_TYPE_MISMATCH;
    if (type == HOST_NVS_BLOB || type == HOST_NVS_STR) {
        if (!out) { *length = entry->size; return ESP_OK; }
        if (*length < entry->size) return ESP_ERR_NVS_INVALID_LENGTH;
        *length = entry->size;
    }
    memcpy(out, entry->data, entry->size);
    return ESP_OK;
}

static esp_err_t stage(nvs_handle_t handle, const char *key, host_nvs_type_t type, const void *data, size_t size, bool erase)
{
    handle_t *h = use(handle);
    if (!h->writable) return ESP_ERR_NVS_READ_ONLY;
    if (host_nvs_set_error) return host_nvs_set_error;
    assert(h->staged_count < STAGED && strlen(key) < 16 && size <= sizeof(h->staged[0].item.value.data));
    staged_t *op = &h->staged[h->staged_count++];
    memset(op, 0, sizeof(*op));
    op->erase = erase;
    memcpy(op->item.space, h->space, sizeof(op->item.space));
    snprintf(op->item.key, sizeof(op->item.key), "%s", key);
    op->item.value.type = type;
    op->item.value.size = size;
    if (data) memcpy(op->item.value.data, data, size);
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *value) { return get(handle, key, HOST_NVS_U8, value, NULL); }
esp_err_t nvs_get_u16(nvs_handle_t handle, const char *key, uint16_t *value) { return get(handle, key, HOST_NVS_U16, value, NULL); }
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *value, size_t *length) { return get(handle, key, HOST_NVS_BLOB, value, length); }
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *value, size_t *length) { return get(handle, key, HOST_NVS_STR, value, length); }
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value) { return stage(handle, key, HOST_NVS_STR, value, strlen(value) + 1, false); }
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) { return stage(handle, key, HOST_NVS_U8, &value, 1, false); }
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length) { return stage(handle, key, HOST_NVS_BLOB, value, length, false); }

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    handle_t *h = use(handle);
    if (!lookup(h->space, key)) return ESP_ERR_NVS_NOT_FOUND;
    return stage(handle, key, HOST_NVS_U8, NULL, 0, true);
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    handle_t *h = use(handle);
    if (host_nvs_commit_error) { h->staged_count = 0; return host_nvs_commit_error; }
    for (unsigned i = 0; i < h->staged_count; ++i) apply(&h->staged[i]);
    h->staged_count = 0;
    ++host_nvs_commits;
    return ESP_OK;
}
