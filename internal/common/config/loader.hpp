#pragma once

#include <exception>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

class ConfigLoader {
public:
    // Returns std::nullopt on success, or an error message on failure.
    template <typename T>
    static std::optional<std::string> LoadConfig(const std::string& configFilePath, T& config) {
        std::ifstream configFile(configFilePath);
        if (!configFile.is_open()) {
            return "Unable to open configuration file: " + configFilePath;
        }

        try {
            const nlohmann::json json = nlohmann::json::parse(configFile);
            T parsedConfig = json.get<T>();
            config = std::move(parsedConfig);
        } catch (const nlohmann::json::exception& error) {
            return "Unable to parse configuration file '" + configFilePath + "': " + error.what();
        } catch (const std::exception& error) {
            return "Unable to load configuration file '" + configFilePath + "': " + error.what();
        }

        return std::nullopt;
    }
};