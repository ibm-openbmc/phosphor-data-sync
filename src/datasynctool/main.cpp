// SPDX-License-Identifier: Apache-2.0

#include "config_options.hpp"
#include "dbus_interactions.hpp"

#include <CLI/CLI.hpp>

#include <print>

int main(int argc, char* argv[])
{
    CLI::App app{
        "Data Sync Tool - Command line utility for phosphor-data-sync"};

    auto* fullSyncGroup =
        app.add_option_group("Full Sync", "Trigger a full sync to the sibling");

    bool fullSync{false};
    fullSyncGroup->add_flag("-f,--fullSync", fullSync, "Start a full sync");

    auto* statusGroup = app.add_option_group(
        "Status Display", "Display current status of phosphor-data-sync");

    bool showStatus{false};
    statusGroup->add_flag("-s,--status", showStatus,
                          "Display all D-Bus properties hosted by data sync");

    bool jsonOutput{false};
    statusGroup->add_flag("-j,--json", jsonOutput, "Display in JSON format");

    auto* syncEnableGroup = app.add_option_group("Sync Enable",
                                                 "Enable or disable sync");

    bool enableSync{false};
    auto* enableOpt = syncEnableGroup->add_flag("-e,--enableSync", enableSync,
                                                "Enable sync");

    bool disableSync{false};
    auto* disableOpt = syncEnableGroup->add_flag("-d,--disableSync",
                                                 disableSync, "Disable sync");

    enableOpt->excludes(disableOpt);

    auto* configGroup = app.add_option_group("Config options",
                                             "Configuration related options");

    bool showConfigPaths{false};
    configGroup->add_flag("--configPaths", showConfigPaths,
                          "List all configured paths for syncing");

    // Parse command line arguments
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return app.exit(e);
    }

    int result{0};

    if (fullSync)
    {
        result = datasynctool::dbus_interactions::startFullSync();
        if (result != 0)
        {
            return result;
        }
    }

    if (enableSync)
    {
        result = datasynctool::dbus_interactions::setSyncEnabled(true);
    }

    if (disableSync)
    {
        result = datasynctool::dbus_interactions::setSyncEnabled(false);
    }

    if (showConfigPaths)
    {
        result = datasynctool::config_options::listConfigPaths(jsonOutput);
    }

    if (showStatus)
    {
        result = datasynctool::dbus_interactions::displayStatus(jsonOutput);
        return result;
    }

    // Default behavior when no options are provided
    if (argc == 1)
    {
        std::println("Data Sync Tool : Usage : datasynctool <options>");
        std::println("Use --help for available options.");
    }

    return result;
}
