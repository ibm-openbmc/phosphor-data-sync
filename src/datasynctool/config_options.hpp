// SPDX-License-Identifier: Apache-2.0

#pragma once

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

} // namespace datasynctool::config_options
