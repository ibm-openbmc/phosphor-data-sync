#include "manager_test.hpp"

namespace fs = std::filesystem;

std::filesystem::path ManagerTest::dataSyncCfgDir;
std::filesystem::path ManagerTest::tmpDataSyncDataDir;
nlohmann::json ManagerTest::commonJsonData;

TEST_F(ManagerTest, PeriodicDataSyncTest)
{
    using namespace std::literals;
    namespace ed = data_sync::ext_data;

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

    nlohmann::json jsonData = {
        {"Files",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile1"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Parse test file"},
           {"SyncDirection", "Bidirectional"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"}}}}};

    fs::path srcFile{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destFile = destDir / fs::relative(srcFile, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Initial Data\n"};
    ManagerTest::writeData(srcFile, data);

    ASSERT_EQ(ManagerTest::readData(srcFile), data);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    EXPECT_NE(ManagerTest::readData(destFile), data)
        << "The data should not match because the manager is spawned and "
        << "is waiting for the periodic interval to initiate the sync.";

    std::string updated_data{"Data got updated\n"};
    ctx.spawn(sdbusplus::async::sleep_for(ctx, 2s) |
              sdbusplus::async::execution::then(
                  [&srcFile, &destFile, &data, &updated_data]() {
        EXPECT_EQ(ManagerTest::readData(destFile), data);
        ManagerTest::writeData(srcFile, updated_data);
    }));

    EXPECT_NE(ManagerTest::readData(destFile), updated_data);

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 0.5s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();

    EXPECT_NE(ManagerTest::readData(destFile), updated_data)
        << "ctx is stopped before sync could take place therefore modified data"
        << "should not sync to dest path.";
}

TEST_F(ManagerTest, PeriodicDataSyncDelayFileTest)
{
    using namespace std::literals;
    namespace ed = data_sync::ext_data;

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

    nlohmann::json jsonData = {
        {"Files",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile1"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Parse test file"},
           {"SyncDirection", "Bidirectional"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"}}}}};

    fs::path srcFile{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destFile = destDir / fs::relative(srcFile, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Initial Data\n"};
    ASSERT_NE(ManagerTest::readData(srcFile), data);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    EXPECT_NE(ManagerTest::readData(destFile), data)
        << "The data should not match because the source data is "
        << "not present";

    ctx.spawn(sdbusplus::async::sleep_for(ctx, 1.1s) |
              sdbusplus::async::execution::then([&srcFile, &destFile, &data]() {
        EXPECT_NE(ManagerTest::readData(destFile), data);
        ManagerTest::writeData(srcFile, data);
    }));

    EXPECT_NE(ManagerTest::readData(destFile), data)
        << "Source file just created, sync haven't took place yet.";

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 1.5s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();

    EXPECT_EQ(ManagerTest::readData(destFile), data)
        << "ctx is stopped after sync take place therefore data"
        << "should sync to dest path.";
}

TEST_F(ManagerTest, PeriodicDataSyncMultiRWTest)
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
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile2"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Parse test file"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"}}}}};

    fs::path srcFile{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destFile = destDir / fs::relative(srcFile, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Initial Data\n"};
    ManagerTest::writeData(srcFile, data);

    ASSERT_EQ(ManagerTest::readData(srcFile), data);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    EXPECT_NE(ManagerTest::readData(destFile), data)
        << "The data should not match because the manager is spawned and"
        << " is waiting for the periodic interval to initiate the sync.";

    std::string updated_data{"Data got updated\n"};
    ctx.spawn(sdbusplus::async::sleep_for(ctx, 2.1s) |
              sdbusplus::async::execution::then(
                  [&srcFile, &destFile, &data, &updated_data]() {
        EXPECT_EQ(ManagerTest::readData(destFile), data)
            << "The data should match as 2.1s is passed and "
            << "sync should take place every 1s as per config";
        ManagerTest::writeData(srcFile, updated_data);
    }));

    EXPECT_NE(ManagerTest::readData(destFile), updated_data);

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 2.2s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();
    EXPECT_EQ(ManagerTest::readData(destFile), updated_data)
        << "The data should match with the updated data as 2.2s is passed"
        << " and sync should take place every 1s as per config.";
}

TEST_F(ManagerTest, PeriodicDataSyncP2ATest)
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
        mockExtDataIfaces->setBMCRole(ed::BMCRole::Passive);
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
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile3"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Parse test file"},
           {"SyncDirection", "Passive2Active"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"}}}}};

    fs::path srcFile{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destFile = destDir / fs::relative(srcFile, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Initial Data\n"};
    ManagerTest::writeData(srcFile, data);

    ASSERT_EQ(ManagerTest::readData(srcFile), data);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};
    EXPECT_NE(ManagerTest::readData(destFile), data)
        << "The data should not match because the manager is spawned and"
        << "is waiting for the periodic interval to initiate the sync.";

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 1.1s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();

    EXPECT_EQ(ManagerTest::readData(destFile), data)
        << "The sync direction is from Passive to Active, mocks the role"
        << " as Passive and verifies that the data matches from the P-BMC.";
}

TEST_F(ManagerTest, PeriodicDisablePropertyTest)
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
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcFile2"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Parse test file"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"}}}}};

    fs::path srcFile{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destFile = destDir / fs::relative(srcFile, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Initial Data\n"};
    ManagerTest::writeData(srcFile, data);

    ASSERT_EQ(ManagerTest::readData(srcFile), data);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};
    manager.setDisableSyncStatus(true); // disabled the sync events

    EXPECT_NE(ManagerTest::readData(destFile), data)
        << "The data should not match because the manager is spawned and"
        << " is waiting for the periodic interval to initiate the sync.";

    ctx.spawn(sdbusplus::async::sleep_for(ctx, 1.1s) |
              sdbusplus::async::execution::then([&destFile, &data, &manager]() {
        EXPECT_NE(ManagerTest::readData(destFile), data)
            << "The data should not match as sync is disabled even though "
            << "sync should take place every 1s as per config";
        manager.setDisableSyncStatus(false); // Trigger the sync events
    }));

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 2.2s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();
    EXPECT_EQ(ManagerTest::readData(destFile), data)
        << "The data should match with the data as 2.2s is passed"
        << " and sync should take place every 1s as per config.";
}

TEST_F(ManagerTest, PeriodicDataSyncTestDataDeleteInDir)
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
        co_return;
    });

    EXPECT_CALL(*mockExtDataIfaces, fetchSiblingBmcIP())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    EXPECT_CALL(*mockExtDataIfaces, fetchRbmcCredentials())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    nlohmann::json jsonData = {
        {"Directories",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Directory to test periodic sync on file deletion"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"}}}}};

    fs::path srcDir{jsonData["Directories"][0]["Path"]};
    fs::path destDir{jsonData["Directories"][0]["DestinationPath"]};
    fs::path destDirPath = destDir / fs::relative(srcDir, "/");

    // Create directories in source and destination
    std::filesystem::create_directory(srcDir);
    std::filesystem::create_directories(destDirPath);
    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    fs::path srcDirFile = srcDir / "Test";
    fs::path destDirFile = destDirPath / "Test";

    std::string data{"Src: Initial Data\n"};
    std::string destData{"Dest: Initial Data\n"};
    ManagerTest::writeData(srcDirFile, data);
    ManagerTest::writeData(destDirFile, destData);

    ASSERT_EQ(ManagerTest::readData(srcDirFile), data);
    ASSERT_EQ(ManagerTest::readData(destDirFile), destData);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    ASSERT_TRUE(std::filesystem::exists(srcDirFile));
    ASSERT_TRUE(std::filesystem::exists(destDirFile));

    // expecting that full sync will be completed under 1s
    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 1.1s) |
        sdbusplus::async::execution::then([&srcDirFile, &destDirFile, &data]() {
        EXPECT_EQ(ManagerTest::readData(destDirFile), data);
        // remove the file from srcDir
        std::filesystem::remove(srcDirFile);
        // check if it exists
        ASSERT_FALSE(std::filesystem::exists(srcDirFile));
    }));

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 1.5s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();

    EXPECT_FALSE(std::filesystem::exists(destDirFile));
}

TEST_F(ManagerTest, PeriodicDataSyncTestDataDeleteFile)
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
         {{{"Path",
            ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/TestFile"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Directory to test periodic sync on file deletion"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"}}}}};

    fs::path srcPath{jsonData["Files"][0]["Path"]};
    fs::path destDir{jsonData["Files"][0]["DestinationPath"]};
    fs::path destPath = destDir / fs::relative(srcPath, "/");

    // Create directories in source and destination
    std::filesystem::create_directory(ManagerTest::tmpDataSyncDataDir.string() +
                                      "/srcDir");
    std::filesystem::create_directories(
        destDir / fs::relative(srcPath.parent_path(), "/"));

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    std::string data{"Src: Initial Data\n"};
    std::string destData{"Dest: Initial Data\n"};
    ManagerTest::writeData(srcPath, data);
    ManagerTest::writeData(destPath, destData);

    ASSERT_EQ(ManagerTest::readData(srcPath), data);
    ASSERT_EQ(ManagerTest::readData(destPath), destData)
        << "DestPath data not matching..";

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    ASSERT_TRUE(std::filesystem::exists(srcPath));
    ASSERT_TRUE(std::filesystem::exists(destPath));

    ctx.spawn(sdbusplus::async::sleep_for(ctx, 1.1s) |
              sdbusplus::async::execution::then([&srcPath, &destPath, &data]() {
        EXPECT_EQ(ManagerTest::readData(destPath), data);
        // remove the file
        std::filesystem::remove(srcPath);
        // check if it exists
        ASSERT_FALSE(std::filesystem::exists(srcPath));
    }));

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 1.5s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();

    EXPECT_FALSE(std::filesystem::exists(destPath));
}

TEST_F(ManagerTest, PeriodicDataSyncTestWithExcludeList)
{
    using namespace std::literals;
    namespace ed = data_sync::ext_data;

    std::unique_ptr<ed::ExternalDataIFaces> extDataIface =
        std::make_unique<ed::MockExternalDataIFaces>();

    ed::MockExternalDataIFaces* mockExtDataIfaces =
        dynamic_cast<ed::MockExternalDataIFaces*>(extDataIface.get());

    EXPECT_CALL(*mockExtDataIfaces, fetchSiblingBmcIP())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    EXPECT_CALL(*mockExtDataIfaces, fetchRbmcCredentials())
        // NOLINTNEXTLINE
        .WillRepeatedly([]() -> sdbusplus::async::task<> { co_return; });

    ON_CALL(*mockExtDataIfaces, fetchBMCRedundancyMgrProps())
        // NOLINTNEXTLINE
        .WillByDefault([&mockExtDataIfaces]() -> sdbusplus::async::task<> {
        mockExtDataIfaces->setBMCRole(ed::BMCRole::Active);
        mockExtDataIfaces->setBMCRedundancy(true);
        co_return;
    });

    nlohmann::json jsonData = {
        {"Directories",
         {{{"Path", ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/"},
           {"DestinationPath",
            ManagerTest::tmpDataSyncDataDir.string() + "/destDir/"},
           {"Description", "Test periodic sync with multiple exclude paths"},
           {"SyncDirection", "Active2Passive"},
           {"SyncType", "Periodic"},
           {"Periodicity", "PT1S"},
           {"ExcludeList",
            {ManagerTest::tmpDataSyncDataDir.string() + "/srcDir/subDirX/",
             ManagerTest::tmpDataSyncDataDir.string() +
                 "/srcDir/dirFileX"}}}}}};

    fs::path srcPath{jsonData["Directories"][0]["Path"]};
    fs::path destDir{jsonData["Directories"][0]["DestinationPath"]};
    fs::path dirFile1 = srcPath / "dirFile1";
    fs::path dirFileX = srcPath / "dirFileX";
    fs::path subDirX = srcPath / "subDirX";
    fs::path destPath = destDir / fs::relative(srcPath, "/");
    fs::path destDirFile1 = destDir / fs::relative(dirFile1, "/");
    fs::path destDirFileX = destDir / fs::relative(dirFileX, "/");
    fs::path destSubDirX = destDir / fs::relative(subDirX, "/");

    writeConfig(jsonData);
    sdbusplus::async::context ctx;

    // Create src and dest directories.
    fs::create_directory(srcPath);
    fs::create_directories(subDirX);
    fs::create_directory(destDir);

    std::string dataDirFile1{"Data in dirFile1"};
    std::string dataDirFileX{"Data in dirFileX"};
    std::string dataSubDirXFile{"Data in subDirXFile"};

    ManagerTest::writeData(dirFile1, dataDirFile1);
    ASSERT_EQ(ManagerTest::readData(dirFile1), dataDirFile1);
    ManagerTest::writeData(dirFileX, dataDirFileX);
    ASSERT_EQ(ManagerTest::readData(dirFileX), dataDirFileX);
    ManagerTest::writeData(subDirX / "file", dataSubDirXFile);
    ASSERT_EQ(ManagerTest::readData(subDirX / "file"), dataSubDirXFile);

    data_sync::Manager manager{ctx, std::move(extDataIface),
                               ManagerTest::dataSyncCfgDir};

    EXPECT_FALSE(fs::exists(destPath))
        << "In destination no src files shouldn't exists as sync doesn't "
        << "initiated so far after manager is spawned";

    std::string updatedData{"Data is updated"};
    ManagerTest::writeData(dirFileX, updatedData);
    ASSERT_EQ(ManagerTest::readData(dirFileX), updatedData);
    ctx.spawn(sdbusplus::async::sleep_for(ctx, 1.2s) |
              sdbusplus::async::execution::then([&destPath, &destDirFile1,
                                                 &dataDirFile1, &destDirFileX,
                                                 &destSubDirX]() {
        EXPECT_TRUE(fs::exists(destPath));
        EXPECT_EQ(ManagerTest::readData(destDirFile1), dataDirFile1);
        EXPECT_FALSE(fs::exists(destDirFileX));
        EXPECT_FALSE(fs::exists(destSubDirX));
    }));

    ctx.spawn(
        sdbusplus::async::sleep_for(ctx, 1.5s) |
        sdbusplus::async::execution::then([&ctx]() { ctx.request_stop(); }));
    ctx.run();
}
