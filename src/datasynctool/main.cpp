// SPDX-License-Identifier: Apache-2.0

#include "dbus_interactions.hpp"

#include <CLI/CLI.hpp>

#include <print>

int main(int argc, char* argv[])
{
    CLI::App app{
        "Data Sync Tool - Command line utility for phosphor-data-sync"};

    bool showStatus{false};
    app.add_flag("-s,--status", showStatus,
                 "Display all D-Bus properties hosted by data sync");

    bool jsonOutput{false};
    app.add_flag("-j,--json", jsonOutput, "Display in JSON format");

    // Parse command line arguments
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return app.exit(e);
    }

    if (showStatus)
    {
        return datasynctool::dbus_interactions::displayStatus(jsonOutput);
    }

    // Default behavior when no options are provided
    std::println("Data Sync Tool initialized");
    std::println("Use --help for available options.");

    return 0;
}
