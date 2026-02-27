#include <simpleble/SimpleBLE.h>
#include <iostream>
#include <vector>

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


SimpleBLE::Adapter adapter;
SimpleBLE::Peripheral* remote;
std::vector<SimpleBLE::Peripheral*> sensors = {};

void connect_device(SimpleBLE::Peripheral pref){

   SimpleBLE::Peripheral* pref_ptr;
   for (int i = 0 ; i < EXPECTED_DEVICES ; i++){
      if (expected_devices[i].device_id  == pref.address()){

         pref_ptr = new SimpleBLE::Peripheral;
         pref_ptr = &pref;
         device_type pref_type = expected_devices[i].type;

         if (pref_type == REMOTE){
            remote = pref_ptr;
         }
         else if (pref_type == SENSOR){
            sensors.push_back(pref_ptr);
         }
         else {
            std::cout << "This should never happen\n";
         }
         pref_ptr->indicate(pref_ptr->address(), CHAR_ID, [pref_type] (SimpleBLE::ByteArray bytes){request_handler(bytes, pref_type);});
      }
   }

}

void request_handler(SimpleBLE::ByteArray req, device_type type){
   std::cout << "Request handler reached!\n";
}

bool ble_init(){

   if (!SimpleBLE::Adapter::bluetooth_enabled()) {
      std::cout << "Bluetooth is not enabled" << std::endl;
      return false;
   }

   auto adapters = SimpleBLE::Adapter::get_adapters();
   if (adapters.empty()) {
      std::cout << "No Bluetooth adapters found" << std::endl;
      return false;
   }

   adapter = adapters[0];

   adapter.set_callback_on_scan_found([](SimpleBLE::Peripheral pref){connect_device(pref);});
   
   return true;
}

int main() {
    
   if (!ble_init()) return EXIT_FAILURE;


   
   return EXIT_SUCCESS;

}