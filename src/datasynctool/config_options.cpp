// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "config_options.hpp"

#include "utils.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
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

} // namespace datasynctool::config_options
