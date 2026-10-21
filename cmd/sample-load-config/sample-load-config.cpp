#include "../../internal/common/config/loader.hpp"
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

struct MyConfig {
    std::string name;
    int value;
    bool enabled;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MyConfig, name, value, enabled)

int main() {
    MyConfig config;
    std::string configFilePath = "config.json";

    auto error = ConfigLoader::LoadConfig(configFilePath, config);
    if (error) {
        std::cerr << "Error loading configuration: " << *error << std::endl;
        return 1;
    }

    std::cout << "Configuration loaded successfully:" << std::endl;
    std::cout << "Name: " << config.name << std::endl;
    std::cout << "Value: " << config.value << std::endl;
    std::cout << "Enabled: " << (config.enabled ? "true" : "false") << std::endl;

    return 0;
}