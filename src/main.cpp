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
void sensor_disconnect_handler(SimpleBLE::Peripheral* pref);


SimpleBLE::Adapter adapter;
SimpleBLE::Peripheral* remote = nullptr;
std::vector<SimpleBLE::Peripheral*> sensors = {};

void connect_device(SimpleBLE::Peripheral pref){

   SimpleBLE::Peripheral* pref_ptr;
   for (int i = 0 ; i < EXPECTED_DEVICES ; i++){
      if (expected_devices[i].device_id  == pref.address()){

         pref_ptr = new SimpleBLE::Peripheral;
         *pref_ptr = pref;
         device_type pref_type = expected_devices[i].type;

         if (pref_type == REMOTE){
            remote = pref_ptr;
            pref_ptr->set_callback_on_disconnected([pref_ptr](){delete pref_ptr; remote = nullptr;});
         }
         else if (pref_type == SENSOR){
            sensors.push_back(pref_ptr);
            pref_ptr->set_callback_on_disconnected([pref_ptr](){delete pref_ptr; sensor_disconnect_handler(pref_ptr);});
         }
         else {
            delete pref_ptr;
            continue;
         }
         pref_ptr->connect();
         pref_ptr->set_callback_on_disconnected([pref_ptr](){delete pref_ptr;});
         pref_ptr->indicate(pref_ptr->address(), CHAR_ID, [pref_type] (SimpleBLE::ByteArray bytes){request_handler(bytes, pref_type);});

      }
   }

}

void sensor_disconnect_handler(SimpleBLE::Peripheral* pref){
   std::cout << "Sensor disconnect handler reached!\n";
}

void request_handler(SimpleBLE::ByteArray req, device_type type){
   std::cout << "Request handler reached!\n";
}

bool ble_init(){
   
   /*
   if (!SimpleBLE::Adapter::bluetooth_enabled()) {
      std::cout << "Bluetooth is not enabled" << std::endl;
      return false;
   }
   */
  
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
   
   // adapter.set_callback_on_scan_found([](SimpleBLE::Peripheral pref){connect_device(pref);});
   // adapter.scan_start();
   
   return true;
   
}

int main() {

   
   if (!ble_init()) return EXIT_FAILURE;

   std::cout << "initialization successful" << std::endl;
   
   while(true){
      sleep(1);
      std::cout << "Here some status update in the future\n";
   }
   return EXIT_SUCCESS;

}