// SPDX-License-Identifier: Apache-2.0

#include <CLI/CLI.hpp>

#include <print>

int main(int argc, char* argv[])
{
    CLI::App app{
        "Data Sync Tool - Command line utility for phosphor-data-sync"};

    // Parse command line arguments
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return app.exit(e);
    }

    std::println("Data Sync Tool initialized");
    std::println("Use --help for available options.");

    return 0;
}
