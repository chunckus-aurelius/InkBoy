idf_component_register(
    SRCS "game_audio.c" "CodecDriver.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
        "vendor/esp_codec_dev/device/es8311"
        "vendor/esp_codec_dev/device/priv_include"
        "vendor/esp_codec_dev/device/include"
        "vendor/esp_codec_dev/interface"
        "vendor/esp_codec_dev/include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_i2s
)
