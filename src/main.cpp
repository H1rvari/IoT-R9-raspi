#include <simpleble/SimpleBLE.h>
#include <iostream>

int main() {
    auto adapters = SimpleBLE::Adapter::get_adapters();

    std::cout << "Found " << adapters.size() << " adapters\n";

    for (auto& adapter : adapters) {
        std::cout << "Adapter: " << adapter.identifier() << "\n";
    }
}