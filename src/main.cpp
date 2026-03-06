#include <simpleble/SimpleBLE.h>
#include <gpiod.hpp>
#include <iostream>
#include <vector>
#include <unistd.h>

#define ADAPTER_ID "hci0"
#define REMOTE_UUID "45:3C:C1:BF:57:5A"
#define SENSOR_UUID "33:53:F9:85:68:94"
#define CHAR_ID_STATUS_UPDATE "19B10002-E8F2-537E-4F6C-D104768A1214"
#define CHAR_ID_REMOTE_PRESS_BUTTON "19B10001-E8F2-537E-4F6C-D104768A1214"
#define CHAR_ID_SENSOR_TRIGGER "abcdef01-1234-5678-1234-56789abcdef0"
#define SERVICE_ID_REMOTE "19B10000-E8F2-537E-4F6C-D104768A1214"
#define SERVICE_ID_SENSOR "12345678-1234-5678-1234-56789abcdef0"

/*
#define CHIP_NAME "gpiochip0"
#define CHIP_PIN_NUM 23
*/

typedef enum { REMOTE, SENSOR } device_type;
typedef struct {
    SimpleBLE::BluetoothUUID device_id;
    device_type type;
} device_model;

device_model expected_devices[] = {
    {REMOTE_UUID, REMOTE},
    {SENSOR_UUID, SENSOR}
};

void connect_device(SimpleBLE::Peripheral pref);
void request_handler(SimpleBLE::ByteArray req, device_type type);
void disconnect_handler(device_type type);
void update_actuator();
void broadcast_state();

SimpleBLE::Adapter adapter;
SimpleBLE::Peripheral remote;
SimpleBLE::Peripheral sensor;
bool remote_initialized = false;
bool sensor_initialized = false;
bool is_active = false;
bool is_armed = false;
bool alarm_on = false;

void connect_device(SimpleBLE::Peripheral pref) {
    device_type pref_type;

    if (pref.address() == REMOTE_UUID) {
        pref_type = REMOTE;
    } else if (pref.address() == SENSOR_UUID) {
        pref_type = SENSOR;
    } else {
        return;
    }

    // FIX 1: Guard against re-entry — scan callbacks fire for every
    //         advertisement packet, so connect_device() is called repeatedly
    //         for the same device. Without this, connect() gets spammed on
    //         an already-connected peripheral.
    if (pref_type == REMOTE && remote_initialized) return;
    if (pref_type == SENSOR && sensor_initialized) return;

    std::cout << "Device found: " << pref.address()
              << "    " << pref.identifier() << std::endl;

    try {
        pref.connect();
    } catch (const std::exception& e) {
        std::cout << "UUID matched but connecting failed:\n" << e.what() << std::endl;
        return;
    }

    try {
        if (pref_type == REMOTE) {
            remote = pref;
            remote_initialized = true;
        } else {
            sensor = pref;
            is_active = true;
            sensor_initialized = true;
        }

        pref.set_callback_on_disconnected([pref_type]() {
            disconnect_handler(pref_type);
        });

        if (pref_type == REMOTE) {
            pref.notify(SERVICE_ID_REMOTE, CHAR_ID_REMOTE_PRESS_BUTTON,
                [pref_type](SimpleBLE::ByteArray bytes) {
                    request_handler(bytes, pref_type);
                });
        } else {
            pref.notify(SERVICE_ID_SENSOR, CHAR_ID_SENSOR_TRIGGER,
                [pref_type](SimpleBLE::ByteArray bytes) {
                    request_handler(bytes, pref_type);
                });
        }
    } catch (const std::exception& e) {
        std::cout << "UUID matched but initialization failed:\n" << e.what() << std::endl;

        if (pref_type == REMOTE) {
            remote_initialized = false;
        } else {
            sensor_initialized = false;
            is_active = false;
        }

        try { pref.disconnect(); } catch (...) {}
        return;
    }

    std::cout << "Device connected\n";
}

void broadcast_state() {
    std::cout << "Broadcasting state:\n";
    // FIX 2: kvn::bytearray does not exist in SimpleBLE. Use
    //         SimpleBLE::ByteArray (std::vector<uint8_t>) directly.
    SimpleBLE::ByteArray data_out = {
        static_cast<uint8_t>(is_active),
        static_cast<uint8_t>(is_armed),
        static_cast<uint8_t>(alarm_on)
    };
    remote.write_command(SERVICE_ID_REMOTE, CHAR_ID_STATUS_UPDATE, data_out);
}

void update_actuator() {
    std::cout << "Alarm is set to ";
    if (alarm_on) {
        std::cout << "on\n";
    } else {
        std::cout << "off\n";
    }
}

void disconnect_handler(device_type type) {
    if (type == SENSOR) {
        is_active = false;
        is_armed = false;
        alarm_on = false;
        sensor_initialized = false;    // FIX 3: was missing — main loop scan
                                       // restart logic depends on this flag.
        update_actuator();             // FIX 4: alarm_on just became false;
                                       // actuator must reflect that immediately.
        if (remote.is_connected()) {
            broadcast_state();
        }
        std::cout << "Sensor disconnected\n";
    } else if (type == REMOTE) {
        remote_initialized = false;    // FIX 3 (same): needed for scan restart.
        std::cout << "Remote disconnected\n";
    } else {
        std::cout << "Error: invalid device type in disconnect handler\n";
    }
}

void request_handler(SimpleBLE::ByteArray req, device_type type) {
    // FIX 5: Guard against empty payload — req[0] on an empty vector is UB.
    if (req.empty()) {
        std::cout << "Empty request received\n";
        return;
    }

    uint8_t data_in = req[0];

    // FIX 6: ~0x00 is int-typed; comparing to uint8_t silently fails on
    //         platforms where int != uint8_t width. Use the literal 0xFF.
    if (data_in != 0xFF) {
        std::cout << "Invalid input data: " << static_cast<int>(data_in) << "\n";
        return;
    }

    if (type == SENSOR) {
        if (is_active && is_armed) {
            alarm_on = true;
            update_actuator();    // FIX 7: actuator was never called when the
                                  //         sensor triggered the alarm.
        }
    } else if (type == REMOTE) {
        if (!is_active) {
            is_armed = false;
            alarm_on = false;
        } else {
            if (is_armed) {
                is_armed = false;
                alarm_on = false;
            } else {
                is_armed = true;
            }
        }

        if (remote.is_connected()) {
            try {
                broadcast_state();
            } catch (const std::exception& e) {
                std::cout << "Failed to broadcast state to remote despite being connected:\n"
                          << e.what() << std::endl;
            }
        }
        update_actuator();
    }
}

bool ble_init() {
    if (!SimpleBLE::Adapter::bluetooth_enabled()) {
        std::cout << "Bluetooth is not enabled" << std::endl;
        return false;
    }

    auto adapters = SimpleBLE::Adapter::get_adapters();

    bool adapter_found = false;
    for (auto ad : adapters) {
        std::cout << "Adapter found: " << ad.identifier() << std::endl;
        if (ad.identifier() == ADAPTER_ID) {
            adapter = ad;
            adapter_found = true;
            break;
        }
    }
    if (!adapter_found) {
        std::cout << "The correct adapter not found" << std::endl;
        return false;
    }

    adapter.set_callback_on_scan_found([](SimpleBLE::Peripheral pref) { connect_device(pref); });
    adapter.set_callback_on_scan_start([]() { std::cout << "Scan started." << std::endl; });
    adapter.set_callback_on_scan_stop([]() { std::cout << "Scan stopped." << std::endl; });
    adapter.power_on();
    adapter.scan_start();

    return true;
}

int main() {
    if (!ble_init()) return EXIT_FAILURE;

    std::cout << "Initialization successful" << std::endl;

    update_actuator();

    while (true) {
        sleep(1);

        std::string state_active = is_active ? "device active" : "device inactive";
        std::string state_armed  = is_armed  ? "device armed"  : "device not armed";
        std::string state_alarm  = alarm_on  ? "alarm active"  : "alarm not active";
        std::cout << "Current state:    " << state_active
                  << "    " << state_armed
                  << "    " << state_alarm << "\n";

        // FIX 8: Original logic only entered scan management when BOTH
        //         *_initialized flags were true, meaning if one device never
        //         connected, the block was never reached and scan ran forever
        //         with no stop/restart control. Now driven by is_connected()
        //         directly, which is always accurate regardless of init state.
        bool both_connected = remote.is_connected() && sensor.is_connected();

        if (adapter.scan_is_active() && both_connected) {
            adapter.scan_stop();
        } else if (!adapter.scan_is_active() && !both_connected) {
            adapter.scan_start();
        }
    }

    return EXIT_SUCCESS;
}
