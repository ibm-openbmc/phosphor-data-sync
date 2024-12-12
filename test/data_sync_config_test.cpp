// SPDX-License-Identifier: Apache-2.0

#include "data_sync_config.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <optional>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

/*
 * Test when the input JSON contains the details of the file to be synced
 * immediately with no overriding retry attempt and retry interval.
 */
TEST(DataSyncConfigParserTest, TestImmediateFileSyncWithNoRetry)
{
    // JSON object with details of file to be synced.
    const auto configJSON = R"(
        {
            "Path": "/file/path/to/sync",
            "Description": "Add details about the data and purpose of the synchronization",
            "SyncDirection": "Active2Passive",
            "SyncType": "Immediate"
        }

    )"_json;

    data_sync::config::DataSyncConfig dataSyncConfig(configJSON);

    EXPECT_EQ(dataSyncConfig._path, "/file/path/to/sync");
    EXPECT_EQ(dataSyncConfig._syncDirection,
              data_sync::config::SyncDirection::Active2Passive);
    EXPECT_EQ(dataSyncConfig._syncType, data_sync::config::SyncType::Immediate);
    EXPECT_EQ(dataSyncConfig._periodicityInSec, std::nullopt);
    EXPECT_EQ(dataSyncConfig._retry, std::nullopt);
    EXPECT_EQ(dataSyncConfig._excludeFileList, std::nullopt);
    EXPECT_EQ(dataSyncConfig._includeFileList, std::nullopt);
}

/*
 * Test when the input JSON contains the details of the file to be synced
 * periodically with overriding retry attempt and retry interval.
 */
TEST(DataSyncConfigParserTest, TestPeriodicFileSyncWithRetry)
{
    // JSON object with details of file to be synced.
    const auto configJSON = R"(
        {
            "Path": "/file/path/to/sync",
            "Description": "Add details about the data and purpose of the synchronization",
            "SyncDirection": "Passive2Active",
            "SyncType": "Periodic",
            "Periodicity": "PT1M10S",
            "RetryAttempts": 1,
            "RetryInterval": "PT1M"
        }

    )"_json;

    data_sync::config::DataSyncConfig dataSyncConfig(configJSON);

    EXPECT_EQ(dataSyncConfig._path, "/file/path/to/sync");
    EXPECT_EQ(dataSyncConfig._syncDirection,
              data_sync::config::SyncDirection::Passive2Active);
    EXPECT_EQ(dataSyncConfig._syncType, data_sync::config::SyncType::Periodic);
    EXPECT_EQ(dataSyncConfig._periodicityInSec, std::chrono::seconds(70));
    EXPECT_EQ(dataSyncConfig._retry.value()._retryAttempts, 1);
    EXPECT_EQ(dataSyncConfig._retry.value()._retryIntervalInSec,
              std::chrono::seconds(60));
    EXPECT_EQ(dataSyncConfig._excludeFileList, std::nullopt);
    EXPECT_EQ(dataSyncConfig._includeFileList, std::nullopt);
}

/*
 * Test when the input JSON contains the details of the directory to be synced
 * immediately with no overriding retry attempt and retry interval.
 */
TEST(DataSyncConfigParserTest, TestImmediateDirectorySyncWithNoRetry)
{
    const auto configJSON = R"(
        {
            "Path": "/directory/path/to/sync",
            "Description": "Add details about the data and purpose of the synchronization",
            "SyncDirection": "Passive2Active",
            "SyncType": "Immediate",
            "ExcludeFilesList": ["/Path/of/files/must/be/ignored/for/sync"],
            "IncludeFilesList": ["/Path/of/files/must/be/considered/for/sync"]
        }
    )"_json;

    data_sync::config::DataSyncConfig dataSyncConfig(configJSON);

    EXPECT_EQ(dataSyncConfig._path, "/directory/path/to/sync");
    EXPECT_EQ(dataSyncConfig._syncDirection,
              data_sync::config::SyncDirection::Passive2Active);
    EXPECT_EQ(dataSyncConfig._syncType, data_sync::config::SyncType::Immediate);
    EXPECT_EQ(dataSyncConfig._periodicityInSec, std::nullopt);
    EXPECT_EQ(dataSyncConfig._retry, std::nullopt);
    EXPECT_EQ(dataSyncConfig._excludeFileList.value(),
              configJSON["ExcludeFilesList"].get<std::vector<std::string>>());
    EXPECT_EQ(dataSyncConfig._includeFileList.value(),
              configJSON["IncludeFilesList"].get<std::vector<std::string>>());
}

/*
 * Test when the input JSON contains the details of the directory to be synced
 * immediately in bidirectional way  with no overriding retry attempt and retry
 * interval.
 */
TEST(DataSyncConfigParserTest, TestImmediateAndBidirectionalDirectorySync)
{
    const auto configJSON = R"(
        {
            "Path": "/directory/path/to/sync",
            "Description": "Add details about the data and purpose of the synchronization",
            "SyncDirection": "Bidirectional",
            "SyncType": "Immediate"
        }
    )"_json;

    data_sync::config::DataSyncConfig dataSyncConfig(configJSON);

    EXPECT_EQ(dataSyncConfig._path, "/directory/path/to/sync");
    EXPECT_EQ(dataSyncConfig._syncDirection,
              data_sync::config::SyncDirection::Bidirectional);
    EXPECT_EQ(dataSyncConfig._syncType, data_sync::config::SyncType::Immediate);
    EXPECT_EQ(dataSyncConfig._periodicityInSec, std::nullopt);
    EXPECT_EQ(dataSyncConfig._retry, std::nullopt);
    EXPECT_EQ(dataSyncConfig._excludeFileList, std::nullopt);
    EXPECT_EQ(dataSyncConfig._includeFileList, std::nullopt);
}
