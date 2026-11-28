# phosphor-data-sync

The phosphor-data-sync application will be used in a redundant BMC system to
synchronize data between BMCs according to the data file details configured for
BMC applications.

## Overview

This application provides a robust data synchronization mechanism for redundant
BMC (Baseboard Management Controller) systems, ensuring data consistency between
active and passive BMCs. The synchronization behavior is fully configurable
through JSON configuration files. See
[Configuration Documentation](config/README.md) for detailed information on how
to configure data synchronization.

### Key Features

- **Multiple Sync Types**: Supports different synchronization modes to meet
  various application requirements:
  - **Immediate Sync**: Triggered automatically when files or directories are
    modified
  - **Periodic Sync**: Scheduled synchronization at configurable time intervals
  - **Full Sync**: Complete synchronization of all configured data via D-Bus
    method call. Data sync issues fullsync during startup and in all other case
    RBMC manager issues full sync when needed.

- **Multiple Sync Directions**: Configure data flow based on BMC roles:
  - Active to Passive (Active2Passive)
  - Passive to Active (Passive2Active)
  - Bidirectional sync between both BMCs

- **Automatic Retry Mechanism**: Configurable retry attempts and intervals for
  handling transient failures during synchronization operations. Default retry
  parameters are set via meson build options (`retry_attempts` and
  `retry_interval`) and can override for each individual data path in the
  configuration JSON files.

- **Sibling Notification**: After successful synchronization, automatically
  notify services on the sibling BMC to reload or restart as needed. Supports
  both systemd service notifications and D-Bus method calls.

- **Secure Data Transfer**: Uses mutual TLS (mTLS) encryption via rsync over
  stunnel to ensure secure communication between BMCs. See
  [Configuration Documentation](config/README.md) for rsync and stunnel
  configuration details.

All synchronization behavior, including paths, sync types, retry logic, and
notification settings, can be configured through JSON files in the
[config/data_sync_list](config/data_sync_list) directory.

## D-Bus Interface

The application implements the `xyz.openbmc_project.Control.SyncBMCData` D-Bus
interface to provide control and monitoring capabilities.

### Methods

#### StartFullSync()

Initiates a full synchronization of all configured files and directories between
BMCs.

**Example**:

```bash
busctl call xyz.openbmc_project.Control.SyncBMCData \
    /xyz/openbmc_project/control/sync_bmc_data \
    xyz.openbmc_project.Control.SyncBMCData \
    StartFullSync
```

### Properties

#### FullSyncStatus (read-only)

Indicates the current status of full synchronization operation.

**Values**:

- `NotStarted` - No full sync has been initiated
- `FullSyncInProgress` - Full sync is currently running
- `FullSyncCompleted` - Full sync completed successfully
- `FullSyncFailed` - Full sync failed

**Example**:

```bash
busctl get-property xyz.openbmc_project.Control.SyncBMCData \
    /xyz/openbmc_project/control/sync_bmc_data \
    xyz.openbmc_project.Control.SyncBMCData \
    FullSyncStatus
```

#### DisableSync (read-write)

Controls whether synchronization operations are enabled or disabled.

When set to `true`, all ongoing and future sync operations are stopped. Setting
it back to `false` resumes normal sync operations. RBMC manager disables sync
when sync needs to be prevented.

**Example**:

```bash
# Disable sync
busctl set-property xyz.openbmc_project.Control.SyncBMCData \
    /xyz/openbmc_project/control/sync_bmc_data \
    xyz.openbmc_project.Control.SyncBMCData \
    DisableSync b true
```

#### SyncEventsHealth (read-only)

Indicates the overall health status of synchronization events.

**Values**:

- `Ok` - All sync events are functioning normally
- `Paused` - Sync is paused (DisableSync is set to true, e.g., when sibling BMC
  is unavailable)
- `Critical` - Sync events are in a critical state due to repeated failures

**Example**:

```bash
busctl get-property xyz.openbmc_project.Control.SyncBMCData \
    /xyz/openbmc_project/control/sync_bmc_data \
    xyz.openbmc_project.Control.SyncBMCData \
    SyncEventsHealth
```

## To build

```sh
meson setup builddir
meson compile -C builddir
```

### Build Options

```sh
# Build with specific data sync configurations
meson setup builddir -Ddata_sync_list=common,ibm
```

Available options:

- `data_sync_list`: Array of configuration files to include (choices: common,
  open-power, ibm). Default: `['common']`
- `retry_attempts`: Default retry attempts for sync failures (min: 0). Default:
  `2`
- `retry_interval`: Default retry interval in seconds. Default: `30`
- `tests`: Enable/disable test suite. Default: `enabled`

## Testing

```sh
# Run all tests
meson test -C builddir
```

## License

This project is licensed under the Apache License 2.0. See [LICENSE](LICENSE)
for details.
