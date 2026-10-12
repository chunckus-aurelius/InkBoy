idf_component_register(SRCS "ble_gamepad.c" "vendor/esp_hid_gap.c" INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "vendor"
    REQUIRES esp_hid esp_timer nvs_flash)
