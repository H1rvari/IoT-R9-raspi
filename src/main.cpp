#include <simpleble/SimpleBLE.h>
#include <iostream>
#include <vector>
#include <unistd.h>

#define ADAPTER_ID "hci0"

#define REMOTE_UUID "placeholder"
#define SENSOR_UUID "placeholder"

#define CHAR_ID "placeholder"

#define EXPECTED_DEVICES 2

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


SimpleBLE::Adapter adapter;
SimpleBLE::Peripheral remote;
SimpleBLE::Peripheral sensor;

bool is_active = false;
bool is_armed = false;
bool alarm_on = false;


void connect_device(SimpleBLE::Peripheral pref){

   std::cout << "Device found\n";

   device_type pref_type;
   switch(pref.address()){
      case REMOTE_UUID:
         pref_type = REMOTE;
         break;
      case SENSOR_UUID:
         pref_type = SENSOR;
         is_active = true;
         break;
      default:
         std::cout << "invalid UUID: " << pref.address(); << "\n";
         return;
   }
   
   std::cout << "Device conneccted\n";

   pref_ptr.connect();
   pref_ptr.set_callback_on_disconnected([pref_type](){disconnect_handler(pref_type);});
   pref_ptr.indicate(pref_ptr->address(), CHAR_ID, [pref_type] (SimpleBLE::ByteArray bytes){request_handler(bytes, pref_type);});

}

void update_acturator(){
   std::cout << "Actuator controller reached\n";
}

void disconnect_handler(device_type type){
   std::cout << "Sensor disconnect handler reached!\n";
}

void request_handler(SimpleBLE::ByteArray req, device_type type){
   
   uint8_t data_in = req[0];
   std::vector<uint8_t> data_out;

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
      data_out.append(is_active);
      data_out.append(is_armed);
      data_out.append(alarm_on);
      // TODO broadcat state to renote
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

   while (!adapter.initialized()) {
      sleep(1);
      std::cout << "not initialized" << std::endl;
   };
   
   while(true){
      sleep(1);
      std::cout << "Here some status update in the future\n";
   }
   return EXIT_SUCCESS;

}