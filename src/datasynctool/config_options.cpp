// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "config_options.hpp"

#include "utils.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <string>
#include <vector>

namespace datasynctool::config_options
{

using json = nlohmann::ordered_json;

int listConfigPaths(bool jsonOutput)
{
    namespace fs = std::filesystem;

    try
    {
        const fs::path configDir = DATA_SYNC_CONFIG_DIR;

        if (!fs::exists(configDir) || !fs::is_directory(configDir))
        {
            std::cerr << "Config directory not found: " << configDir << "\n";
            return -1;
        }

        std::vector<std::string> files;
        std::vector<std::string> directories;

        // Read all JSON files in the config directory
        for (const auto& entry : fs::directory_iterator(configDir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                std::ifstream configFile(entry.path());
                if (!configFile.is_open())
                {
                    std::cerr << "Failed to open: " << entry.path() << "\n";
                    continue;
                }

                try
                {
                    json config = json::parse(configFile);

                    if (config.contains("Files") && config["Files"].is_array())
                    {
                        for (const auto& fileEntry : config["Files"])
                        {
                            if (fileEntry.contains("Path"))
                            {
                                files.push_back(
                                    fileEntry["Path"].get<std::string>());
                            }
                        }
                    }

                    if (config.contains("Directories") &&
                        config["Directories"].is_array())
                    {
                        for (const auto& dirEntry : config["Directories"])
                        {
                            if (dirEntry.contains("Path"))
                            {
                                directories.push_back(
                                    dirEntry["Path"].get<std::string>());
                            }
                        }
                    }
                }
                catch (const json::exception& e)
                {
                    std::cerr << "JSON parse error in " << entry.path() << ": "
                              << e.what() << "\n";
                    continue;
                }
            }
        }

        // Build JSON output
        json output;
        output["Files"] = files;
        output["Directories"] = directories;

        if (jsonOutput)
        {
            std::println("{}", output.dump(4));
        }
        else
        {
            utils::displayJsonAsText(output);
        }

        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error listing config paths: " << e.what() << "\n";
        return -1;
    }
}

// Helper function to search for a path in a JSON array
static std::optional<json> findPathInArray(const json& array,
                                           const std::string& normalizedTarget)
{
    if (!array.is_array())
    {
        return std::nullopt;
    }

    for (const auto& jsonEntry : array)
    {
        if (jsonEntry.contains("Path"))
        {
            std::string configPath = jsonEntry["Path"].get<std::string>();
            if (utils::normalizePath(configPath) == normalizedTarget)
            {
                return jsonEntry;
            }
        }
    }
    return std::nullopt;
}

int getPathConfig(const std::string& targetPath, bool jsonOutput)
{
    namespace fs = std::filesystem;

    try
    {
        const fs::path configDir = DATA_SYNC_CONFIG_DIR;

        if (!fs::exists(configDir) || !fs::is_directory(configDir))
        {
            std::cerr << "Config directory not found: " << configDir << "\n";
            return -1;
        }

        std::string normalizedTarget = utils::normalizePath(targetPath);

        // Search through all JSON config files
        for (const auto& entry : fs::directory_iterator(configDir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            std::ifstream configFile(entry.path());
            if (!configFile.is_open())
            {
                std::cerr << "Failed to open: " << entry.path() << "\n";
                continue;
            }

            try
            {
                json config = json::parse(configFile);

                auto matchedConfig = findPathInArray(config["Files"],
                                                     normalizedTarget);
                if (!matchedConfig)
                {
                    matchedConfig = findPathInArray(config["Directories"],
                                                    normalizedTarget);
                }

                if (matchedConfig)
                {
                    // Add default retry values if not present
                    if (!matchedConfig->contains("RetryAttempts"))
                    {
                        (*matchedConfig)["RetryAttempts"] =
                            DEFAULT_RETRY_ATTEMPTS;
                    }
                    if (!matchedConfig->contains("RetryInterval"))
                    {
                        (*matchedConfig)["RetryInterval"] =
                            std::to_string(DEFAULT_RETRY_INTERVAL) + " seconds";
                    }

                    // Append config file name
                    (*matchedConfig)["Config File"] = entry.path().string();

                    // Output the configuration
                    if (jsonOutput)
                    {
                        std::println("{}", matchedConfig->dump(4));
                    }
                    else
                    {
                        utils::displayJsonAsText(*matchedConfig);
                    }
                    return 0;
                }
            }
            catch (const json::exception& e)
            {
                std::cerr << "JSON parse error in " << entry.path() << ": "
                          << e.what() << "\n";
                continue;
            }
        }

        std::cerr << "Error: Path '" << targetPath
                  << "' not found in configuration\n";
        return -1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error getting path config: " << e.what() << "\n";
        return -1;
    }
}

} // namespace datasynctool::config_options
