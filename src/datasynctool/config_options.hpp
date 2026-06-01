// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace datasynctool::config_options
{

/**
 * @brief List all configured sync paths from JSON config files
 *
 * @param[in] jsonOutput : true to output in JSON format
 *                       : false for text
 * @return 0 on success
 *          -1 on error
 */
int listConfigPaths(bool jsonOutput);

/**
 * @brief List all paths currently being watched by inotify
 *
 * Reads from /run/phosphor-data-sync/watching-paths.json which is
 * maintained by the Manager class aggregating all DataWatcher instances.
 *
 * @param[in] jsonOutput : true to output in JSON format
 *                       : false for formatted text
 * @return 0 on success
 *          -1 on error
 */
int listWatchingPaths(bool jsonOutput);

/**
 * @brief Get configuration for a specific path
 */
int getPathConfig(const std::string& targetPath, bool jsonOutput);

} // namespace datasynctool::config_options
