// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "notify_sibling.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <iostream>

namespace data_sync::notify
{
namespace file_operations
{
fs::path writeToFile(const auto& jsonData)
{
    // TODO : generate unique across machine
    fs::path notifyFilePath =
        NOTIFY_SIBLING_DIR /
        fs::path("notify_" + std::to_string(std::time(nullptr)) + ".json");
    if (!fs::exists(NOTIFY_SIBLING_DIR))
    {
        fs::create_directories(NOTIFY_SIBLING_DIR);
    }

    try
    {
        std::ofstream notifyFile(notifyFilePath);
        if (!notifyFile)
        {
            throw std::runtime_error("Failed to open the notify request file");
        }

        notifyFile << jsonData.dump(4);
        notifyFile.close();

        return notifyFilePath;
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("Failed to write the notify request to the "
                                 "file, error : " +
                                 std::string(e.what()));
    }
}
} // namespace file_operations

NotifySibling::NotifySibling(const config::DataSyncConfig& dataSyncConfig,
                             const fs::path& modifiedDataPath)
{
    try
    {
        nlohmann::json notifyInfoJson = frameNotifyRqst(dataSyncConfig,
                                                        modifiedDataPath);
        _notifyInfoFile = file_operations::writeToFile(notifyInfoJson);
    }
    catch (std::exception& e)
    {
        lg2::error(
            "Creation of sibling notification request failed!!! for [{DATA}]",
            "DATA", dataSyncConfig._path);
    }
}

fs::path NotifySibling::getNotifyFilePath() const
{
    return _notifyInfoFile;
}

nlohmann::json
    NotifySibling::frameNotifyRqst(const config::DataSyncConfig& dataSyncConfig,
                                   const fs::path& modifiedDataPath)
{
    try
    {
        return nlohmann::json::object(
            {{"ModifiedDataPath", modifiedDataPath},
             {"NotifyInfo",
              dataSyncConfig._notifySibling.value()._notifySiblingInfo}});
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            "Failed to frame the notify request JSON for path: " +
            dataSyncConfig._path.string() + ", error: " + e.what());
    }
}

} // namespace data_sync::notify
