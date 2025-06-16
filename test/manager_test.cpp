// SPDX-License-Identifier: Apache-2.0
#include "manager_test.hpp"

#include <csignal>

std::filesystem::path ManagerTest::dataSyncCfgDir;
std::filesystem::path ManagerTest::tmpDataSyncDataDir;
nlohmann::json ManagerTest::commonJsonData;

namespace fs = std::filesystem;

using FullSyncStatus = sdbusplus::common::xyz::openbmc_project::control::
    SyncBMCData::FullSyncStatus;
using SyncEventsHealth = sdbusplus::common::xyz::openbmc_project::control::
    SyncBMCData::SyncEventsHealth;

TEST_F(ManagerTest, ParseDataSyncCfg)
{
    using namespace std::literals;
    namespace ed = data_sync::ext_data;

    commonJsonData = R"(
            {
                "Files": [
                    {
                        "Path": "/file/path/to/sync",
                        "Description": "Parse test file",
                        "SyncDirection": "Active2Passive",
                        "SyncType": "Immediate"
                    }
                ],
                "Directories": [
                    {
                        "Path": "/directory/path/to/sync/",
                        "Description": "Parse test directory",
                        "SyncDirection": "Passive2Active",
                        "SyncType": "Periodic",
                        "Periodicity": "PT1S",
                        "RetryAttempts": 1,
                        "RetryInterval": "PT10M",
                        "ExcludeFilesList": ["/directory/file/to/ignore"],
                        "IncludeFilesList": ["/directory/file/to/consider"]
                    }
                ]
            }
        )"_json;

    writeConfig(commonJsonData);

    std::unique_ptr<ed::ExternalDataIFaces> extDataIface =
        std::make_unique<ed::MockExternalDataIFaces>();

    ed::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<ed::MockExternalDataIFaces*>(extDataIface.get());

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    EXPECT_CALL(*mockExtDataIfaces, fetchSiblingBmcIP())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    EXPECT_CALL(*mockExtDataIfaces, fetchRbmcCredentials())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    sdbusplus::async::context ctx;

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    EXPECT_FALSE(manager.containsDataSyncCfg(data_sync::config::DataSyncConfig(
        ManagerTest::commonJsonData["Files"][0], false)));

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 1ns) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();

    EXPECT_TRUE(manager.containsDataSyncCfg(data_sync::config::DataSyncConfig(
        ManagerTest::commonJsonData["Files"][0], false)));
}

TEST_F(ManagerTest, testDBusDataPersistency)
{
    using namespace std::literals;
    namespace ed = data_sync::ext_data;

    std::unique_ptr<ed::ExternalDataIFaces> extDataIface =
        std::make_unique<ed::MockExternalDataIFaces>();

    ed::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<ed::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        // NOLINTNEXTLINE
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(ed::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchSiblingBmcIP())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    EXPECT_CALL(*mockExtDataIfaces, fetchRbmcCredentials())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Files",
         {
             {{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile1"},
              {"DestinationPath",
               ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
              {"Description", "FullSync from Active to Passive bmc"},
              {"SyncDirection", "Active2Passive"},
              {"SyncType", "Immediate"}},
             {{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile2"},
              {"DestinationPath",
               ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
              {"Description", "FullSync from Active to Passive bmc"},
              {"SyncDirection", "Active2Passive"},
              {"SyncType", "Immediate"}},
         }}};

    fs::path srcFile1{jsonData["Files"][0]["Path"]};
    fs::path srcFile2{jsonData["Files"][1]["Path"]};

    fs::path destDir1{jsonData["Files"][0]["DestinationPath"]};
    fs::path destDir2{jsonData["Files"][1]["DestinationPath"]};

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data1{"Data written on the file1\n"};
    std::string data2{"Data written on the file2\n"};

    ManagerTest::writeData(srcFile1, data1);
    ManagerTest::writeData(srcFile2, data2);

    ASSERT_EQ(ManagerTest::readData(srcFile1), data1);
    ASSERT_EQ(ManagerTest::readData(srcFile2), data2);

    // Before starting manager we write to persistent file: SyncEventsHealth as
    // Critical and FullSyncStatus as In Progress. After manager starts, it
    // should load the persistent data as defined.
    data_sync::persist::update(data_sync::persist::key::fullSyncStatus,
                               FullSyncStatus::FullSyncInProgress);
    data_sync::persist::update(data_sync::persist::key::syncEventsHealth,
                               SyncEventsHealth::Critical);
    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    EXPECT_EQ(data_sync::persist::read<FullSyncStatus>(
                  data_sync::persist::key::fullSyncStatus),
              FullSyncStatus::FullSyncInProgress);
    EXPECT_EQ(manager.getFullSyncStatus(), FullSyncStatus::FullSyncInProgress)
        << "FullSyncInProgress is not set to InProgress in persistent file.";

    EXPECT_EQ(data_sync::persist::read<SyncEventsHealth>(
                  data_sync::persist::key::syncEventsHealth),
              SyncEventsHealth::Critical);
    EXPECT_EQ(manager.getSyncEventsHealth(), SyncEventsHealth::Critical)
        << "SyncEventsHealth is not set to Critical in persistent file.";

    auto waitingForFullSyncToFinish =
        // NOLINTNEXTLINE
        [&](sdbusplus::async::context& ctx) -> sdbusplus::async::task<void> {
        auto status = manager.getFullSyncStatus();

        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            co_await sdbusplus::async::sleep_for(ctx,
                                                 std::chrono::milliseconds(50));
            status = manager.getFullSyncStatus();
        }

        EXPECT_EQ(status, FullSyncStatus::FullSyncCompleted)
            << "FullSync status is not Completed!";

        EXPECT_EQ(ManagerTest::readData(destDir1 / fs::relative(srcFile1, "/")),
                  data1);
        EXPECT_EQ(ManagerTest::readData(destDir2 / fs::relative(srcFile2, "/")),
                  data2);

        ctx.request_stop();

        // After successfull completion of full sync, the DBUS property must be
        // updated and stored. Reading and confirming the values.
        EXPECT_EQ(data_sync::persist::read<FullSyncStatus>(
                      data_sync::persist::key::fullSyncStatus),
                  FullSyncStatus::FullSyncCompleted);
        EXPECT_EQ(data_sync::persist::read<SyncEventsHealth>(
                      data_sync::persist::key::syncEventsHealth),
                  SyncEventsHealth::Ok);

        // Forcing to trigger inotify events so that all running immediate
        // sync tasks will resume and stop since the context is requested to
        // stop in the above.
        ManagerTest::writeData(srcFile1, data1);
        ManagerTest::writeData(srcFile2, data2);
        co_return;
    };

    ctx.spawn(waitingForFullSyncToFinish(ctx));

    ctx.run();
}

TEST_F(ManagerTest, testSignalReceiverLogic)
{
    using namespace std::literals;
    namespace ed = data_sync::ext_data;

    std::unique_ptr<ed::ExternalDataIFaces> extDataIface =
        std::make_unique<ed::MockExternalDataIFaces>();

    ed::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<ed::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        // NOLINTNEXTLINE
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(ed::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchSiblingBmcIP())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    EXPECT_CALL(*mockExtDataIfaces, fetchRbmcCredentials())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Files",
         {
             {{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile1"},
              {"DestinationPath",
               ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
              {"Description", "FullSync from Active to Passive bmc"},
              {"SyncDirection", "Active2Passive"},
              {"SyncType", "Immediate"}},
             {{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile2"},
              {"DestinationPath",
               ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
              {"Description", "FullSync from Active to Passive bmc"},
              {"SyncDirection", "Active2Passive"},
              {"SyncType", "Immediate"}},
         }}};

    fs::path srcFile1{jsonData["Files"][0]["Path"]};
    fs::path srcFile2{jsonData["Files"][1]["Path"]};

    fs::path destDir1{jsonData["Files"][0]["DestinationPath"]};
    fs::path destDir2{jsonData["Files"][1]["DestinationPath"]};

    writeConfig(jsonData);
    std::filesystem::remove("/tmp/data_sync_paths_info.lsv");
    sdbusplus::async::context ctx;

    std::string data1{"Data written on the file1\n"};
    std::string data2{"Data written on the file2\n"};

    ManagerTest::writeData(srcFile1, data1);
    ManagerTest::writeData(srcFile2, data2);

    ASSERT_EQ(ManagerTest::readData(srcFile1), data1);
    ASSERT_EQ(ManagerTest::readData(srcFile2), data2);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    auto waitingForFullSyncToFinish =
        // NOLINTNEXTLINE
        [&](sdbusplus::async::context& ctx) -> sdbusplus::async::task<void> {
        auto status = manager.getFullSyncStatus();

        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            co_await sdbusplus::async::sleep_for(ctx,
                                                 std::chrono::milliseconds(50));
            status = manager.getFullSyncStatus();
        }

        EXPECT_EQ(status, FullSyncStatus::FullSyncCompleted)
            << "FullSync status is not Completed!";

        // Raising signal after full sync completes, so that sync events starts
        // and the watchers are being created.
        sdbusplus::async::sleep_for(ctx, 0.1s);
        // Raising a signal to trigger the signal handler logic.
        std::raise(SIGUSR1);
        //  waiting to write all the watched paths to a file as per
        // signal handler logic.
        sdbusplus::async::sleep_for(ctx, 0.5s);

        ctx.request_stop();

        // Forcing to trigger inotify events so that all running immediate
        // sync tasks will resume and stop since the context is requested to
        // stop in the above.
        ManagerTest::writeData(srcFile1, data1);
        ManagerTest::writeData(srcFile2, data2);

        // Reading the file where all the watched paths are written by signal
        // handler logic.
        auto watchersF =
            ManagerTest::readData("/tmp/data_sync_paths_info.json");
        auto parsed = nlohmann::json::parse(watchersF);
        EXPECT_TRUE(parsed.is_array());
        // Checking if the srcFile1 and srcFile2 are present in the parsed json
        EXPECT_NE(watchersF.find(srcFile1), std::string::npos);
        EXPECT_NE(watchersF.find(srcFile2), std::string::npos);
        co_return;
    };

    ctx.spawn(waitingForFullSyncToFinish(ctx));
    ctx.run();
}
