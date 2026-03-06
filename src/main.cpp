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

#define SERVICE_ID_REMOTE "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SERVICE_ID_SENSOR "12345678-1234-5678-1234-56789abcdef0"

/*
#define CHIP_NAME "gpiochip0"
#define CHIP_PIN_NUM 23
*/

typedef enum {REMOTE, SENSOR} device_type;

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


void connect_device(SimpleBLE::Peripheral pref){

   device_type pref_type;


   if (pref.address() == REMOTE_UUID){
      remote = pref;
      pref_type = REMOTE;
      remote_initialized = true;
   }
   else if (pref.address() == SENSOR_UUID){
      sensor = pref;
      pref_type = SENSOR;
      is_active = true;
      sensor_initialized = true;
   }
   else{
      //std::cout << "invalid identifier: " << pref.identifier() << " address: " << pref.address() << "\n";
      return;
   }
   std::cout << "Device found: " << pref.address() << "    " << pref.identifier() << std::endl;
   adapter.scan_stop();
   sleep(2);
   if (!pref.is_connectable()){
      std::cout << pref.identifier() << " is not connectable\n";
      adapter.scan_start();
      return;
   }
   try {
      pref.connect();
   } catch (const std::exception& e){
      std::cout << "UUID matched but connecting failed:\n" << e.what() << std::endl;
      adapter.scan_start();
      return;
   }
   std::cout << "Connecting successfull\n";

   try {
      pref.set_callback_on_disconnected([pref_type](){disconnect_handler(pref_type);});
      if (pref_type == REMOTE){
         pref.indicate(SERVICE_ID_REMOTE, CHAR_ID_REMOTE_PRESS_BUTTON, [pref_type] (SimpleBLE::ByteArray bytes){request_handler(bytes, pref_type);});
      }
      else {
         pref.indicate(SERVICE_ID_SENSOR, CHAR_ID_SENSOR_TRIGGER, [pref_type] (SimpleBLE::ByteArray bytes){request_handler(bytes, pref_type);});
      }
   } catch (const std::exception& e){
      std::cout << "UUID matched but initialization failed:\n" << e.what() << std::endl;
      return;
   }
   std::cout << "Device conneccted\n";
   adapter.scan_start();

}

void broadcast_state(){
   std::cout << "Broadcasting state:\n";
   std::vector<uint8_t> data_out = {is_active, is_armed, alarm_on};
   remote.write_command(SERVICE_ID_REMOTE, CHAR_ID_STATUS_UPDATE, kvn::bytearray(data_out));
}

void update_actuator(){
   std::cout << "Alarm is set to ";
   if (alarm_on){
      std::cout << "on\n";
   }
   else {
      std::cout << "off\n";
   }
}

void disconnect_handler(device_type type){
   
   if (type == SENSOR){
      is_active = false;
      is_armed = false;
      alarm_on = false;
      if  (remote.is_connected()){
         broadcast_state();
      }
      std::cout << "Sensor disconnected\n";
   }
   else if (type == REMOTE) {
      std::cout << "Remote disconnected\n";
   }
   else {
      std::cout << "Error: invalid device type in disconnect handler\n";
   }
}

void request_handler(SimpleBLE::ByteArray req, device_type type){
   
   uint8_t data_in = req[0];

   if (data_in != ~0x00){
      std::cout << "Invalid input data: " << data_in << "\n";
      return;
   }
   if (type == SENSOR){
      if (is_active && is_armed){
         alarm_on = true;
      }
   }
   else if (type == REMOTE){
      
      if (!is_active){
         is_armed = false;
         alarm_on = false;
      }
      else {
         if (is_armed){
            is_armed = false;
            alarm_on = false;
         }
         else {
            is_armed = true;
         }

      }
      if (remote.is_connected()){
         try {
            broadcast_state();
         } catch (const std::exception& e){
            std::cout << "failed to broadcast state to remote despite being connected:\n" << e.what() << std::endl;
         }

      }
      update_actuator();

   }
}

bool ble_init(){
   
   
   if (!SimpleBLE::Adapter::bluetooth_enabled()) {
      std::cout << "Bluetooth is not enabled" << std::endl;
      return false;
   }
   
  
   auto adapters = SimpleBLE::Adapter::get_adapters();
   
   bool adapter_found = false;
   for (auto ad : adapters){
      std::cout << "Adapter found: " << ad.identifier() << std::endl;
      if (ad.identifier() == ADAPTER_ID){
         adapter = ad;
         adapter_found = true;
         break;
      }
   }
   if (!adapter_found){
      std::cout << "The correct adapter not found" << std::endl;
      return false;
   }
   
   adapter.set_callback_on_scan_found([](SimpleBLE::Peripheral pref){connect_device(pref);});
   adapter.set_callback_on_scan_start([]() { std::cout << "Scan started." << std::endl; });
   adapter.set_callback_on_scan_stop([]() { std::cout << "Scan stopped." << std::endl; });
   adapter.power_on();
   adapter.scan_start();
   
   return true;
   
}

int main() {

   if (!ble_init()) return EXIT_FAILURE;

   std::cout << "initialization successful" << std::endl;

   std::string state_active;
   std::string state_armed;
   std::string state_alarm;

   update_actuator();
   
   while(true){
      sleep(1);
      state_active = is_active ? "device active" : "device inactive";
      state_armed = is_armed ? "device armed" : "device not armed";
      state_alarm = alarm_on ? "alarm active" : "alarm not active";
      std::cout << "Current state:    " << state_active << "    " << state_armed << "    " << state_alarm << "\n";

      if (sensor_initialized && remote_initialized){
         if (adapter.scan_is_active() && (remote.is_connected() && sensor.is_connected())){
            adapter.scan_stop();
         }
         else if (!adapter.scan_is_active() && !(remote.is_connected() && sensor.is_connected())){
            adapter.scan_start();
         }
      }
   }
   return EXIT_SUCCESS;
}