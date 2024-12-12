// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "data_sync_config.hpp"

#include <phosphor-logging/lg2.hpp>

#include <regex>

namespace data_sync
{
namespace config
{

Retry::Retry(const int& retryAttempts,
             const std::chrono::seconds& retryIntervalInSec) :
    _retryAttempts(retryAttempts), _retryIntervalInSec(retryIntervalInSec)
{}

DataSyncConfig::DataSyncConfig(const nlohmann::json& config) :
    _path(config["Path"].get<std::string>()),
    _syncDirection(convertSyncDirectionToEnum(config["SyncDirection"])
                       .value_or(SyncDirection::Active2Passive)),
    _syncType(
        convertSyncTypeToEnum(config["SyncType"]).value_or(SyncType::Immediate))
{
    // Initiailze optional members
    if (_syncType == SyncType::Periodic)
    {
        constexpr auto defPeriodicity = 60;
        _periodicityInSec = convertISODurationToSec(config["Periodicity"])
                                .value_or(std::chrono::seconds(defPeriodicity));
    }
    else
    {
        _periodicityInSec = std::nullopt;
    }

    if (config.contains("RetryAttempts") && config.contains("RetryInterval"))
    {
        _retry =
            Retry(config["RetryAttempts"],
                  convertISODurationToSec(config["RetryInterval"])
                      .value_or(std::chrono::seconds(DEFAULT_RETRY_INTERVAL)));
    }
    else
    {
        _retry = std::nullopt;
    }

    if (config.contains("ExcludeFilesList"))
    {
        _excludeFileList = config["ExcludeFilesList"];
    }
    else
    {
        _excludeFileList = std::nullopt;
    }

    if (config.contains("IncludeFilesList"))
    {
        _includeFileList = config["IncludeFilesList"];
    }
    else
    {
        _includeFileList = std::nullopt;
    }
}

std::optional<SyncDirection>
    DataSyncConfig::convertSyncDirectionToEnum(const std::string& syncDirection)
{
    if (syncDirection == "Active2Passive")
    {
        return SyncDirection::Active2Passive;
    }
    else if (syncDirection == "Passive2Active")
    {
        return SyncDirection::Passive2Active;
    }
    else if (syncDirection == "Bidirectional")
    {
        return SyncDirection::Bidirectional;
    }
    else
    {
        lg2::error("Unsupported sync direction [{SYNC_DIRECTION}]",
                   "SYNC_DIRECTION", syncDirection);
        return std::nullopt;
    }
}

std::optional<SyncType>
    DataSyncConfig::convertSyncTypeToEnum(const std::string& syncType)
{
    if (syncType == "Immediate")
    {
        return SyncType::Immediate;
    }
    else if (syncType == "Periodic")
    {
        return SyncType::Periodic;
    }
    else
    {
        lg2::error("Unsupported sync type [{SYNC_TYPE}]", "SYNC_TYPE",
                   syncType);
        return std::nullopt;
    }
}

std::optional<std::chrono::seconds> DataSyncConfig::convertISODurationToSec(
    const std::string& timeIntervalInISO)
{
    // TODO: Parse periodicity in ISO 8601 duration format using std::chrono
    std::smatch match;
    std::regex isoDurationRegex("PT(([0-9]+)H)?(([0-9]+)M)?(([0-9]+)S)?");

    if (std::regex_search(timeIntervalInISO, match, isoDurationRegex))
    {
        return (std::chrono::seconds(
            (match.str(2).empty() ? 0 : (std::stoi(match.str(2)) * 60 * 60)) +
            (match.str(4).empty() ? 0 : (std::stoi(match.str(4)) * 60)) +
            (match.str(6).empty() ? 0 : std::stoi(match.str(6)))));
    }
    else
    {
        lg2::error("{TIME_INTERVAL} is not matching with expected "
                   "ISO 8601 duration format [PTnHnMnS]",
                   "TIME_INTERVAL", timeIntervalInISO);
        return std::nullopt;
    }
}

} // namespace config
} // namespace data_sync
