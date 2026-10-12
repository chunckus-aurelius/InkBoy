# BLE gamepad host

Uses the ESP-IDF native esp_hidh API. The vendor directory contains
esp_hid_gap.c/.h from ESP-IDF v6.1, commit
fff9895c82d744c7237be8847347bdd1b07c6643,
examples/bluetooth/esp_hid_host/main. Their Unlicense/CC0 notices are retained.
esp_hid_gap.h is unchanged. esp_hid_gap.c is modified in one place: GAP_DBG_PRINTF
no longer prints every scan result to stdout, because nothing may write there
while the USB controller owns the USB PHY.

The adapter accepts the ESP32 Gamepad name and its existing HID report 3. Buttons
1–8 occupy the first report byte; ID is supplied separately by esp_hidh. Physical
mapping is calibrated and stored by the player. It is not a general gamepad HID
report parser. Report loss releases inputs after two seconds; disconnection
restarts discovery. `ble_gamepad_stop` ends scanning and releases the controller
so that the USB controller can take over; start and stop are idempotent and the
Bluetooth stack stays initialised in between. See the top-level README for
pairing and wiring.
