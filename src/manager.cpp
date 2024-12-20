// SPDX-License-Identifier: Apache-2.0

#include "manager.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <exception>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>

namespace data_sync
{

Manager::Manager(const fs::path& dataSyncCfgDir)
{
    parseConfiguration(dataSyncCfgDir);
}

void Manager::parseConfiguration(const fs::path& dataSyncCfgDir)
{
    auto parse = [this](const auto& configFile) {
        try
        {
            std::ifstream file;
            file.open(configFile);

            nlohmann::json configJSON(nlohmann::json::parse(file));

            if (configJSON.contains("Files"))
            {
                std::ranges::transform(
                    configJSON["Files"],
                    std::back_inserter(this->_dataSyncConfiguration),
                    [](const auto& element) {
                    return config::DataSyncConfig(element);
                });
            }
            if (configJSON.contains("Directories"))
            {
                std::ranges::transform(
                    configJSON["Directories"],
                    std::back_inserter(this->_dataSyncConfiguration),
                    [](const auto& element) {
                    return config::DataSyncConfig(element);
                });
            }
        }
        catch (const std::exception& e)
        {
            // TODO Create error log
            lg2::error("Failed to parse the configuration file : {CONFIG_FILE}",
                       "CONFIG_FILE", configFile);
        }
    };

    if (fs::exists(dataSyncCfgDir) && fs::is_directory(dataSyncCfgDir))
    {
        std::ranges::for_each(
            fs::directory_iterator(dataSyncCfgDir),
            [&parse](const auto& configFile) { parse(fs::path(configFile)); });
    }
}
} // namespace data_sync
