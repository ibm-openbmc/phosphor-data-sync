// SPDX-License-Identifier: Apache-2.0

#include "data_watcher.hpp"
#include "manager_test.hpp"

#include <sdbusplus/async.hpp>

#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

std::filesystem::path ManagerTest::dataSyncCfgDir;
std::filesystem::path ManagerTest::tmpDataSyncDataDir;
nlohmann::json ManagerTest::commonJsonData;
std::filesystem::path ManagerTest::destDir;

using FullSyncStatus = sdbusplus::common::xyz::openbmc_project::control::
    SyncBMCData::FullSyncStatus;

TEST_F(ManagerTest, testDataChangeInFile)
{
    using namespace std::literals;
    namespace extData = data_sync::ext_data;

    auto extDataIface = std::make_unique<extData::MockExternalDataIFaces>();
    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        .WillByDefault([mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Files",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile"},
           {"DestinationPath", ManagerTest::destDir.string()},
           {"Description", "File to test immediate sync upon data write"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Immediate"}}}}};

    fs::path srcPath{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destPath = destDir / fs::relative(srcPath, "/");

    writeConfig(jsonData);
    auto ctx = std::make_shared<sdbusplus::async::context>();

    std::string data{"Src: Initial Data\n"};
    ManagerTest::writeData(srcPath, data);
    ASSERT_EQ(ManagerTest::readData(srcPath), data);

    // Create dest path for adding watch.
    std::string destData{"Dest: Initial Data\n"};
    fs::create_directories(destPath.parent_path());
    ASSERT_TRUE(fs::exists(destPath.parent_path()));
    ManagerTest::writeData(destPath, destData);
    ASSERT_EQ(ManagerTest::readData(destPath), destData);

    auto manager = std::make_shared<data_sync::Manager>(
        *ctx, std::move(extDataIface), ManagerTest::dataSyncCfgDir);

    // Spawn the coroutine to trigger immediate sync and check the expectation
    // at dest upon receiving events
    auto triggerAndWatchSyncOp = [manager, srcPath, destPath,
                                  ctx]() -> sdbusplus::async::task<void> {
        // Wait for full sync to complete
        auto status = manager->getFullSyncStatus();
        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            status = manager->getFullSyncStatus();
            co_await sdbusplus::async::sleep_for(*ctx,
                                                 std::chrono::milliseconds(50));
        }

        // data to modify the file
        std::string dataToWrite{"Data is modified"};

        // Create DataWatcher(for dest path)
        auto destWatcher =
            std::make_shared<data_sync::watch::inotify::DataWatcher>(
                *ctx, IN_NONBLOCK, IN_CLOSE_WRITE, destPath);

        // Listen for data change in destination and assert in the 'then'
        // handler
        ctx->spawn(
            destWatcher->onDataChange() |
            sdbusplus::async::execution::then([srcPath, destPath, destWatcher,
                                               dataToWrite, ctx](const auto&) {
            EXPECT_EQ(dataToWrite, readData(destPath));

            // Force an inotify event so running immediate sync tasks wake up
            // handle the last write, and exit once the context stop is
            // requested
            ManagerTest::writeData(srcPath, "Dummy data to stop ctx");
            ctx->request_stop();
        }));

        // Delay to finish the watcher setup and then write into the source
        // file;
        ctx->spawn(sdbusplus::async::sleep_for(*ctx, 1s) |
                   sdbusplus::async::execution::then([srcPath, dataToWrite]() {
            ManagerTest::writeData(srcPath, dataToWrite);
            ASSERT_EQ(ManagerTest::readData(srcPath), dataToWrite);
        }));
        co_return;
    };

    ctx->spawn(triggerAndWatchSyncOp());
    ctx->run();
}

TEST_F(ManagerTest, testDataDeleteInDir)
{
    using namespace std::literals;
    namespace extData = data_sync::ext_data;

    std::unique_ptr<extData::ExternalDataIFaces> extDataIface =
        std::make_unique<extData::MockExternalDataIFaces>();

    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Directories",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/"},
           {"DestinationPath", ManagerTest::destDir.string()},
           {"Description", "Directory to test immediate sync on file deletion"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Immediate"}}}}};

    fs::path srcDir{jsonData["Directories"][0]["Path"]};
    fs::path destDir{jsonData["Directories"][0]["DestinationPath"]};

    writeConfig(jsonData);
    auto ctx = std::make_shared<sdbusplus::async::context>();

    std::string data{"Src: Initial Data\n"};
    fs::create_directory(srcDir);
    fs::path srcDirFile = srcDir / "Test";

    // Write data at the src side.
    ManagerTest::writeData(srcDirFile, data);
    ASSERT_EQ(ManagerTest::readData(srcDirFile), data);

    // Replicate the src folder structure at destination side.
    std::string destData{"Dest: Initial Data\n"};
    fs::path destDirFile = destDir / fs::relative(srcDir, "/") / "Test";
    fs::create_directories(destDirFile.parent_path());
    ASSERT_TRUE(fs::exists(destDirFile.parent_path()));

    // Write data at dest side
    ManagerTest::writeData(destDirFile, destData);
    ASSERT_EQ(ManagerTest::readData(destDirFile), destData);

    auto manager = std::make_shared<data_sync::Manager>(
        *ctx, std::move(extDataIface), ManagerTest::dataSyncCfgDir);

    // Spawn the coroutines to trigger immediate sync and check the expectation
    // at dest upon receiving events
    auto triggerAndWatchSyncOp = [manager, srcDirFile, destDirFile,
                                  ctx]() -> sdbusplus::async::task<void> {
        // Wait until full sync completes and then start watch destination for
        // inotify events
        auto status = manager->getFullSyncStatus();
        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            status = manager->getFullSyncStatus();
            co_await sdbusplus::async::sleep_for(*ctx,
                                                 std::chrono::milliseconds(50));
        }

        // Create DataWatcher(inotify watcher) for dest path
        auto destWatcher =
            std::make_shared<data_sync::watch::inotify::DataWatcher>(
                *ctx, IN_NONBLOCK, IN_DELETE, destDirFile.parent_path());

        // Listen for change and assert in 'then' handler; capture by-value
        ctx->spawn(
            destWatcher->onDataChange() |
            sdbusplus::async::execution::then(
                [srcDirFile, destWatcher, destDirFile, ctx](const auto&) {
            // the file should not exists at dest.
            EXPECT_FALSE(std::filesystem::exists(destDirFile));

            // Force an inotify event so the running immediate sync tasks wake
            // up and handle the last write, and exit as the context stop is
            // also requested
            ManagerTest::writeData(srcDirFile, "dummy data");
            ctx->request_stop();
        }));

        // Delay to finish the watcher setup and then delete the source file
        ctx->spawn(sdbusplus::async::sleep_for(*ctx, 1s) |
                   sdbusplus::async::execution::then([srcDirFile]() {
            std::filesystem::remove(srcDirFile);
            ASSERT_FALSE(std::filesystem::exists(srcDirFile));
        }));
        co_return;
    };

    ctx->spawn(triggerAndWatchSyncOp());
    ctx->run();
}

TEST_F(ManagerTest, testDataDeletePathFile)
{
    using namespace std::literals;
    namespace extData = data_sync::ext_data;

    std::unique_ptr<extData::ExternalDataIFaces> extDataIface =
        std::make_unique<extData::MockExternalDataIFaces>();

    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Files",
         {{{"Path",
            ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/TestFile"},
           {"DestinationPath", ManagerTest::destDir.string()},
           {"Description", "File to test immediate sync on self delete"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Immediate"}}}}};

    fs::path srcPath{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destPath = destDir / fs::relative(srcPath, "/");

    writeConfig(jsonData);
    auto ctx = std::make_shared<sdbusplus::async::context>();

    // Create directories in source
    fs::create_directory(ManagerTest::tmpDataSyncDataDir / "srcDir");

    std::string data{"Src: Initial Data\n"};
    ManagerTest::writeData(srcPath, data);
    ASSERT_EQ(ManagerTest::readData(srcPath), data);

    // Replicate the src folder structure at destination side.
    std::string destData{"Dest: Initial Data\n"};
    fs::create_directories(destPath.parent_path());
    ASSERT_TRUE(fs::exists(destPath.parent_path()));
    ManagerTest::writeData(destPath, destData);
    ASSERT_EQ(ManagerTest::readData(destPath), destData);

    auto manager = std::make_shared<data_sync::Manager>(
        *ctx, std::move(extDataIface), ManagerTest::dataSyncCfgDir);

    // Spawn the coroutines to trigger immediate sync and check the expectation
    // at dest upon receiving events
    auto triggerAndWatchSyncOp = [manager, srcPath, destPath,
                                  ctx]() -> sdbusplus::async::task<void> {
        // Wait for full sync to complete
        auto status = manager->getFullSyncStatus();
        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            status = manager->getFullSyncStatus();
            co_await sdbusplus::async::sleep_for(*ctx,
                                                 std::chrono::milliseconds(50));
        }

        // Create DataWatcher(for dest path)
        auto destWatcher =
            std::make_shared<data_sync::watch::inotify::DataWatcher>(
                *ctx, IN_NONBLOCK, IN_DELETE_SELF, destPath);

        // Listen for change and assert in the 'then' handler;
        ctx->spawn(destWatcher->onDataChange() |
                   sdbusplus::async::execution::then(
                       [srcPath, destPath, destWatcher, ctx](const auto&) {
            // the file should not exists at dest.
            EXPECT_FALSE(std::filesystem::exists(destPath));

            // Force an inotify event so running immediate sync tasks wake up
            // handle the last write, and exit once the context stop is
            // requested
            ManagerTest::writeData(srcPath, "dummy data to stop ctx");
            ctx->request_stop();
        }));

        // Delay and then delete the source file;
        ctx->spawn(sdbusplus::async::sleep_for(*ctx, 1s) |
                   sdbusplus::async::execution::then([srcPath]() {
            std::filesystem::remove(srcPath);
            ASSERT_FALSE(std::filesystem::exists(srcPath));
        }));
        co_return;
    };

    ctx->spawn(triggerAndWatchSyncOp());
    ctx->run();
}

/*
 * Test if the sync is triggered when Disable Sync DBus property is changed
 * from True to False, i.e. sync is changed from disabled to enabled.
 * Will be also testing that the DBus SyncEventsHealth property changes from
 * 'Paused' to 'Ok' when Disable Sync property changes from True to False.
 */
TEST_F(ManagerTest, testDataChangeWhenSyncIsDisabled)
{
    using namespace std::literals;
    using SyncEventsHealth = sdbusplus::common::xyz::openbmc_project::control::
        SyncBMCData::SyncEventsHealth;
    namespace extData = data_sync::ext_data;

    std::unique_ptr<extData::ExternalDataIFaces> extDataIface =
        std::make_unique<extData::MockExternalDataIFaces>();

    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Files",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile2"},
           {"DestinationPath", ManagerTest::destDir.string()},
           {"Description", "File to test immediate sync when sync is disabled"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Immediate"}}}}};

    fs::path srcPath{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destPath = destDir / fs::relative(srcPath, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Src: Initial Data\n"};
    ManagerTest::writeData(srcPath, data);
    ASSERT_EQ(ManagerTest::readData(srcPath), data);

    // Replicate the src folder structure at destination side.
    std::string destData{"Dest: Initial Data\n"};
    fs::create_directories(destPath.parent_path());
    ManagerTest::writeData(destPath, destData);
    ASSERT_EQ(ManagerTest::readData(destPath), destData);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};
    manager.setDisableSyncStatus(true); // disabled the sync events

    EXPECT_NE(ManagerTest::readData(destPath), data)
        << "The data should not match because the manager is spawned and"
        << " sync is disabled.";

    std::string dataToWrite{"Data is modified"};
    std::string dataToStopEvent{"Close spawned inotify event."};
    EXPECT_EQ(manager.getSyncEventsHealth(), SyncEventsHealth::Paused)
        << "SyncEventsHealth should be Paused, as sync is disabled.";

    // write data to src path to create inotify event so that the spawned
    // process is closed.
    ctx.spawn(sdbusplus::async::sleep_for(ctx, 0.1s) |
              sdbusplus::async::execution::then([&srcPath, &dataToStopEvent]() {
        ManagerTest::writeData(srcPath, dataToStopEvent);
    }));

    ctx.spawn(sdbusplus::async::sleep_for(ctx, 0.5s) |
              sdbusplus::async::execution::then(
                  [&destPath, &dataToStopEvent, &manager]() {
        EXPECT_NE(ManagerTest::readData(destPath), dataToStopEvent)
            << "The data should not match as sync is disabled even though"
            << " sync should take place at every data change.";
        manager.setDisableSyncStatus(false); // Trigger the sync events
    }));

    // NOLINTNEXTLINE
    auto triggerImmediateSync = [&]() -> sdbusplus::async::task<void> {
        // Remove file after 1s so that the background sync events will be ready
        // to catch.
        co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(1));
        ManagerTest::writeData(srcPath, dataToWrite);

        // Force an inotify event so running immediate sync tasks wake up
        // handle the last write, and exit once the context stop is requested
        co_await sdbusplus::async::sleep_for(ctx,
                                             std::chrono::milliseconds(100));
        ManagerTest::writeData(srcPath, dataToWrite);

        ctx.request_stop();
        co_return;
    };

    ctx.spawn(triggerImmediateSync());
    ctx.run();

    EXPECT_EQ(manager.getSyncEventsHealth(), SyncEventsHealth::Ok)
        << "SyncEventsHealth should be Ok, as sync was enabled.";
    EXPECT_EQ(ManagerTest::readData(destPath), dataToWrite)
        << "The data should match with the data as the src was modified"
        << " and sync should take place at every modification.";
}

TEST_F(ManagerTest, testDataCreateInSubDir)
{
    using namespace std::literals;
    namespace extData = data_sync::ext_data;

    std::unique_ptr<extData::ExternalDataIFaces> extDataIface =
        std::make_unique<extData::MockExternalDataIFaces>();

    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Directories",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/"},
           {"DestinationPath", ManagerTest::destDir.string()},
           {"Description",
            "File to test immediate sync on non existent dest path"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Immediate"}}}}};

    fs::path srcDir{jsonData["Directories"][0]["Path"]};
    fs::path destDir{jsonData["Directories"][0]["DestinationPath"]};

    // Create directories in source and destination
    std::filesystem::create_directory(srcDir);
    std::filesystem::create_directory(destDir);

    writeConfig(jsonData);
    auto ctx = std::make_shared<sdbusplus::async::context>();

    auto manager = std::make_shared<data_sync::Manager>(
        *ctx, std::move(extDataIface), ManagerTest::dataSyncCfgDir);

    // Spawn the coroutine to trigger immediate sync and check the expectation
    // at dest upon receiving events
    auto triggerAndWatchSyncOp = [manager, srcDir, destDir,
                                  ctx]() -> sdbusplus::async::task<void> {
        // Wait for full sync to complete
        auto status = manager->getFullSyncStatus();
        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            status = manager->getFullSyncStatus();
            co_await sdbusplus::async::sleep_for(*ctx,
                                                 std::chrono::milliseconds(50));
        }

        // Create DataWatcher(for dest path)
        auto destWatcher =
            std::make_shared<data_sync::watch::inotify::DataWatcher>(
                *ctx, IN_NONBLOCK, IN_CREATE, destDir);

        // Listen for change and assert in the 'then' handler; capture by-value
        ctx->spawn(destWatcher->onDataChange() |
                   sdbusplus::async::execution::then(
                       [srcDir, destDir, destWatcher, ctx](const auto&) {
            fs::path destSubDir = destDir / fs::relative(srcDir, "/") / "Test";
            // sleep to get sync and refelet in the dest
            sdbusplus::async::sleep_for(*ctx, std::chrono::milliseconds(10));
            EXPECT_TRUE(std::filesystem::exists(destSubDir));

            // Force an inotify event so running immediate sync tasks wake up
            // handle the last write, and exit once the context stop is
            // requested
            std::filesystem::create_directory(srcDir / "data");
            ctx->request_stop();
        }));

        // Write data after 1s so that the background sync events will be ready
        // to catch.
        ctx->spawn(sdbusplus::async::sleep_for(*ctx, 1s) |
                   sdbusplus::async::execution::then([srcDir]() {
            std::filesystem::create_directory(srcDir / "Test");
            ASSERT_TRUE(std::filesystem::exists(srcDir / "Test"));
        }));
        co_return;
    };

    ctx->spawn(triggerAndWatchSyncOp());
    ctx->run();
}

TEST_F(ManagerTest, testFileMoveToAnotherDir)
{
    using namespace std::literals;
    namespace extData = data_sync::ext_data;

    std::unique_ptr<extData::ExternalDataIFaces> extDataIface =
        std::make_unique<extData::MockExternalDataIFaces>();

    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Directories",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/Dir1/"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir1/"},
           {"Description", "Directory to test immediate sync on file move"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Immediate"}}}}};

    fs::path srcDir{jsonData["Directories"][0]["Path"]};
    fs::path destDir{jsonData["Directories"][0]["DestinationPath"]};
    fs::path destPath = destDir / fs::relative(srcDir, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Data written to the file\n"};
    fs::create_directory(srcDir);

    // Create directories to simulate move operation
    // File "Test" is present in dir1 and will get moved to dir2.
    fs::create_directories(srcDir / "dir1");
    fs::create_directories(srcDir / "dir2");
    ManagerTest::writeData(srcDir / "dir1" / "Test", data);
    ASSERT_EQ(ManagerTest::readData(srcDir / "dir1" / "Test"), data);
    ASSERT_FALSE(fs::exists(srcDir / "dir2" / "Test"));

    // Create dest paths
    fs::create_directories(destPath);
    fs::create_directories(destPath / "dir1");
    fs::create_directories(destPath / "dir2");
    ASSERT_TRUE(fs::exists(destPath));
    ManagerTest::writeData(destPath / "dir1" / "Test", data);
    ASSERT_EQ(ManagerTest::readData(destPath / "dir1" / "Test"), data);
    ASSERT_FALSE(fs::exists(destPath / "dir2" / "Test"));

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    // Case : File "Test" will move from dir1 to dir2
    // File "Test" will get delete from destPath1
    // File "Test" will get create at destPath2

    // Watch dest paths for data change
    data_sync::watch::inotify::DataWatcher dataWatcher1(
        ctx, IN_NONBLOCK, IN_DELETE, (destPath / "dir1"));
    data_sync::watch::inotify::DataWatcher dataWatcher2(
        ctx, IN_NONBLOCK, IN_CREATE, (destPath / "dir2"));

    ctx.spawn(dataWatcher1.onDataChange() |
              sdbusplus::async::execution::then(
                  [&destPath]([[maybe_unused]] const auto& dataOps) {
        EXPECT_FALSE(fs::exists(destPath / "dir1" / "Test"));
    }));

    // NOLINTNEXTLINE
    auto waitForDataChange = [&]() -> sdbusplus::async::task<void> {
        using namespace std::chrono_literals;
        // NOLINTNEXTLINE
        co_await dataWatcher2.onDataChange();
        co_await sdbusplus::async::sleep_for(ctx,
                                             std::chrono::milliseconds(10));
        EXPECT_TRUE(fs::exists(destPath / "dir2" / "Test"));
        EXPECT_EQ(ManagerTest::readData(destPath / "dir2" / "Test"), data);

        co_return;
    };

    // NOLINTNEXTLINE
    auto triggerImmediateSync = [&]() -> sdbusplus::async::task<void> {
        // Move file after 1s so that the background sync events will be ready
        // to catch.
        co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(1));

        fs::rename(srcDir / "dir1" / "Test", srcDir / "dir2" / "Test");
        EXPECT_FALSE(fs::exists(srcDir / "dir1" / "Test"));
        EXPECT_TRUE(fs::exists(srcDir / "dir2" / "Test"));
        EXPECT_EQ(ManagerTest::readData(srcDir / "dir2" / "Test"), data);

        // Force an inotify event so running immediate sync tasks wake up
        // handle the last write, and exit once the context stop is requested
        co_await sdbusplus::async::sleep_for(ctx,
                                             std::chrono::milliseconds(100));
        std::filesystem::create_directory(srcDir / "data");

        ctx.request_stop();
        co_return;
    };

    ctx.spawn(triggerImmediateSync());
    ctx.spawn(waitForDataChange());

    ctx.run();
}

TEST_F(ManagerTest, testExcludeFile)
{
    using namespace std::literals;
    namespace extData = data_sync::ext_data;

    std::unique_ptr<extData::ExternalDataIFaces> extDataIface =
        std::make_unique<extData::MockExternalDataIFaces>();

    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        // NOLINTNEXTLINE
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Directories",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description",
            "Test the configured exclude list while immediate sync"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Immediate"},
           {"ExcludeList",
            {ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/fileX"}}}}}};

    fs::path srcDir{jsonData["Directories"][0]["Path"]};
    fs::path destDir{jsonData["Directories"][0]["DestinationPath"]};
    fs::path excludeFile = jsonData["Directories"][0]["ExcludeList"][0];

    // Create directories in source and destination
    std::filesystem::create_directory(srcDir);
    std::filesystem::create_directory(destDir);

    writeConfig(jsonData);
    auto ctx = std::make_shared<sdbusplus::async::context>();

    // Data for 2 files inside srcDir
    std::string data1{"Data written to file1"};
    std::string dataExcludeFile{"Data written to excludeFile"};

    fs::path file1 = srcDir / "file1";
    ManagerTest::writeData(file1, data1);
    ASSERT_EQ(ManagerTest::readData(file1), data1);
    ManagerTest::writeData(excludeFile, dataExcludeFile);
    ASSERT_EQ(ManagerTest::readData(excludeFile), dataExcludeFile);

    auto manager = std::make_shared<data_sync::Manager>(
        *ctx, std::move(extDataIface), ManagerTest::dataSyncCfgDir);

    std::string dataToFile1{"Data modified in file1"};
    std::string dataToExcludeFile{"Data modified in ExcludeFile"};

    // Spawn the coroutine to trigger immediate sync and check the expectation
    // at dest upon receiving events
    auto triggerAndWatchSyncOp = [manager, srcDir, destDir, file1, excludeFile,
                                  dataToFile1, dataToExcludeFile,
                                  ctx]() -> sdbusplus::async::task<void> {
        // Wait for full sync to complete
        auto status = manager->getFullSyncStatus();
        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            status = manager->getFullSyncStatus();
            co_await sdbusplus::async::sleep_for(*ctx,
                                                 std::chrono::milliseconds(50));
        }

        // Create DataWatcher(for dest path)
        auto destWatcher =
            std::make_shared<data_sync::watch::inotify::DataWatcher>(
                *ctx, IN_NONBLOCK, IN_CREATE | IN_CLOSE_WRITE, destDir);

        // Listen for change and assert in the 'then' handler; capture by-value
        ctx->spawn(destWatcher->onDataChange() |
                   sdbusplus::async::execution::then(
                       [destDir, file1, excludeFile, dataToFile1,
                        dataToExcludeFile, destWatcher, ctx](const auto&) {
            sdbusplus::async::sleep_for(*ctx, std::chrono::milliseconds(20));

            EXPECT_TRUE(fs::exists(destDir / fs::relative(file1, "/")))
                << "file1 should be present at the dest side";
            EXPECT_FALSE(fs::exists(destDir / fs::relative(excludeFile, "/")))
                << "fileX should excluded while syncing to the dest side";

            // Force an inotify event so running immediate sync tasks wake up
            // handle the last write, and exit once the context stop is
            // requested
            ManagerTest::writeData(file1, "dummy data to stop ctx");
            ctx->request_stop();
        }));

        ctx->spawn(
            sdbusplus::async::sleep_for(*ctx, 1s) |
            sdbusplus::async::execution::then(
                [file1, excludeFile, dataToFile1, dataToExcludeFile, ctx]() {
            // Write to file after 1s so that the background sync events will be
            // ready to catch.
            sdbusplus::async::sleep_for(*ctx, std::chrono::seconds(1));

            ManagerTest::writeData(excludeFile, dataToExcludeFile);
            EXPECT_EQ(ManagerTest::readData(excludeFile), dataToExcludeFile);

            ManagerTest::writeData(file1, dataToFile1);
            EXPECT_EQ(ManagerTest::readData(file1), dataToFile1);
        }));
        co_return;
    };

    ctx->spawn(triggerAndWatchSyncOp());
    ctx->run();
}

TEST_F(ManagerTest, ImmediateSyncVanishedPathRetrySucceeds)
{
    using namespace std::literals;
    namespace extData = data_sync::ext_data;

    std::unique_ptr<extData::ExternalDataIFaces> extDataIface =
        std::make_unique<extData::MockExternalDataIFaces>();
    extData::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<extData::MockExternalDataIFaces*>(extDataIface.get());

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(extData::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });
    EXPECT_CALL(*mockExtDataIfaces, fetchBMCPosition())
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Files",
         {{{"Path", (ManagerTest::tmpDataSyncDataDir / "srcDir").string()},
           {"DestinationPath", ManagerTest::destDir.string()},
           {"Description",
            "Immediate sync on dir; delete child parent -> retry"},
           {"SyncDirection", "Active2Passive"},
           {"RetryAttempts", 2},
           {"RetryInterval", "PT1S"},
           {"SyncType", "Immediate"}}}}};

    const fs::path srcPath{jsonData["Files"][0]["Path"].get<std::string>()};
    const fs::path destRoot{
        jsonData["Files"][0]["DestinationPath"].get<std::string>()};

    const fs::path srcSubDirPath = srcPath / "srcSubDir";
    const fs::path srcSubFIlePath = srcPath / "srcSubFile";
    const fs::path srcFilePath = srcSubDirPath / "srcFile";
    const fs::path destFilePath = destRoot / fs::relative(srcPath, "/");
    const fs::path destFilePath2 = destRoot / fs::relative(srcSubDirPath, "/");

    writeConfig(jsonData);
    auto ctx = std::make_shared<sdbusplus::async::context>();

    std::filesystem::create_directories(srcPath);
    std::filesystem::create_directories(srcSubDirPath);
    std::filesystem::create_directories(destRoot);

    auto manager = std::make_shared<data_sync::Manager>(
        *ctx, std::move(extDataIface), ManagerTest::dataSyncCfgDir);

    // Spawn the coroutine to trigger immediate sync
    // and check the expectation at dest upon receiving events
    auto triggerAndWatchSyncOp = [manager, srcFilePath, srcSubFIlePath,
                                  srcSubDirPath, destFilePath, destFilePath2,
                                  ctx]() -> sdbusplus::async::task<void> {
        // Wait for full sync to complete
        auto status = manager->getFullSyncStatus();
        while (status != FullSyncStatus::FullSyncCompleted &&
               status != FullSyncStatus::FullSyncFailed)
        {
            status = manager->getFullSyncStatus();
            co_await sdbusplus::async::sleep_for(*ctx,
                                                 std::chrono::milliseconds(50));
        }

        // Create DataWatcher(for dest path)
        auto destWatcher =
            std::make_shared<data_sync::watch::inotify::DataWatcher>(
                *ctx, IN_NONBLOCK, IN_CLOSE_WRITE, destFilePath2);

        // Listen for change and assert in the 'then' handler; capture by-value
        ctx->spawn(
            destWatcher->onDataChange() |
            sdbusplus::async::execution::then(
                [destWatcher, srcSubFIlePath, destFilePath2, ctx](const auto&) {
            // waiting here, so rync will be finished with parent path
            sdbusplus::async::sleep_for(*ctx, std::chrono::milliseconds(10));

            EXPECT_FALSE(std::filesystem::exists(destFilePath2))
                << "Destination of removed subdir must not exist (we retried only the parent)";

            // Force an inotify event so running immediate sync tasks wake up
            // handle the last write, and exit once the context stop is
            // requested
            ManagerTest::writeData(srcSubFIlePath, "dummy data to stop ctx");
            ctx->request_stop();
        }));

        // Delay to finish watcher setup and then update the source file
        ctx->spawn(sdbusplus::async::sleep_for(*ctx, 1s) |
                   sdbusplus::async::execution::then(
                       [ctx, srcFilePath, srcSubDirPath]() {
            // immediate sync retry test
            // 1) write the file to trigger inotify
            // 2) wait a few ms
            // 3) delete the parent so the file vanishes (exit 24)
            // 4) retry should pick the nearest valid parent and do the sync
            const std::string data{"sample data \n"};
            ManagerTest::writeData(srcFilePath, data);

            EXPECT_TRUE(std::filesystem::exists(srcFilePath))
                << "src file(srcFilePath) must exist before we delete its parent";

            sdbusplus::async::sleep_for(*ctx, std::chrono::milliseconds(2));

            std::error_code ec;
            // deleteing the files/dir so it will trigger the rsync error
            std::filesystem::remove_all(srcSubDirPath, ec);

            EXPECT_FALSE(std::filesystem::exists(srcSubDirPath))
                << "srcSubDirPath should be gone after remove all";

            EXPECT_FALSE(std::filesystem::exists(srcFilePath))
                << "srcFilePath should be gone because its parent was removed";
        }));
        co_return;
    };

    ctx->spawn(triggerAndWatchSyncOp());
    ctx->run();
}
