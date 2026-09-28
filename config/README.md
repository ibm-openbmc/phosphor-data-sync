# Configuration

## Data sync JSON configuration files

The [JSON files](data_sync_list) specify the files and directories to be
synchronized between the BMCs. Each configuration file must follow the schema
defined in the [`config/schema/schema.json`](schema/schema.json).

### JSON Configuration Structure

The configuration file has two main sections:

- **Files**: Array of individual file configurations
- **Directories**: Array of directory configurations

At least one section (Files or Directories) must be present.

### Configuration Keys

#### Common Keys (Required for both Files and Directories)

##### Path (Required)

- **Type**: String
- **Description**: Absolute path of the file or directory to be synchronized
- **Format**:
  - Files: Must start with `/` (e.g., `/var/lib/config.json`)
  - Directories: Must start with `/` and end with `/` (e.g., `/var/lib/data/`)
- **Example**:
  ```json
  "Path": "/etc/myapp/config.json"
  "Path": "/var/lib/myapp/"
  ```

##### Description (Required)

- **Type**: String
- **Description**: A short description about the file/directory being synced and
  its purpose
- **Example**:
  ```json
  "Description": "Application configuration file for network settings"
  ```

##### SyncDirection (Required)

- **Type**: Enum
- **Description**: The direction in which sync operation should be performed
- **Values**:
  - `Active2Passive`: Sync from active BMC to passive BMC
  - `Passive2Active`: Sync from passive BMC to active BMC
  - `Bidirectional`: Sync in both directions
- **Example**:
  ```json
  "SyncDirection": "Bidirectional"
  ```

##### SyncType (Required)

- **Type**: Enum
- **Description**: The type of sync to be performed
- **Values**:
  - `Immediate`: Triggered automatically when files/directories are modified
  - `Periodic`: Scheduled synchronization at configured time intervals
- **Example**:
  ```json
  "SyncType": "Immediate"
  ```

#### Optional Keys (for both Files and Directories)

##### DestinationPath (Optional)

- **Type**: String
- **Description**: Absolute path to the destination file/directory on the
  sibling BMC. If not specified, the same path as `Path` is used
- **Format**:
  - Files: Must start with `/`
  - Directories: Must start with `/` and end with `/`
- **Example**:
  ```json
  "DestinationPath": "/etc/alternate/config.json"
  "DestinationPath": "/var/lib/alternate/"
  ```

##### Periodicity (Required if SyncType is "Periodic")

- **Type**: String (ISO 8601 duration format)
- **Description**: Time interval to perform periodic sync operation
- **Format**: `PT[hours]H[minutes]M[seconds]S`
- **Examples**:
  ```json
  "Periodicity": "PT10S"      // 10 seconds
  "Periodicity": "PT1M30S"    // 1 minute 30 seconds
  "Periodicity": "PT1H"       // 1 hour
  "Periodicity": "PT2H30M"    // 2 hours 30 minutes
  ```

##### RetryAttempts (Optional)

- **Type**: Integer (minimum: 0)
- **Description**: Number of retry attempts for this specific file/directory.
  Value of 0 means no retries. Overrides the default value set via meson build
  option
- **Example**:
  ```json
  "RetryAttempts": 3
  ```

##### RetryInterval (Required if RetryAttempts is specified)

- **Type**: String (ISO 8601 duration format)
- **Description**: Time interval between retry attempts. Overrides the default
  value set via meson build option
- **Format**: `PT[hours]H[minutes]M[seconds]S`
- **Examples**:
  ```json
  "RetryInterval": "PT30S"    // 30 seconds
  "RetryInterval": "PT5M"     // 5 minutes
  "RetryInterval": "PT10M"    // 10 minutes
  ```

##### NotifySibling (Optional)

- **Type**: Object
- **Description**: Configuration for notifying services on the sibling BMC after
  successful synchronization
- **Sub-keys**:

  ###### Mode (Required)
  - **Type**: Enum
  - **Values**:
    - `Systemd`: Invoke systemd reload/restart
    - `DBus`: Invoke reload via custom DBus call
  - **Example**:
    ```json
    "Mode": "Systemd"
    ```

  ###### Method (Required if Mode is "Systemd")
  - **Type**: Enum
  - **Values**:
    - `Restart`: Restart the service
    - `Reload`: Send reload signal without restarting
  - **Example**:
    ```json
    "Method": "Reload"
    ```

  ###### NotifyServices (Required)
  - **Type**: Array of strings
  - **Description**: List of services to be notified
    - For `Systemd` mode: Use systemd unit names (e.g., `myapp.service`)
    - For `DBus` mode: Use DBus service names (e.g.,
      `xyz.openbmc_project.MyApp`)
  - **Example**:
    ```json
    "NotifyServices": ["myapp.service", "network.service"]
    ```

  ###### NotifyOnPaths (Optional)
  - **Type**: Array of strings
  - **Description**: Absolute paths of files inside the configured directory.
    Sibling BMC will be notified only if these specific files are updated
  - **Example**:
    ```json
    "NotifyOnPaths": ["/var/lib/myapp/critical.conf"]
    ```

**Complete NotifySibling Example**:

```json
"NotifySibling": {
    "Mode": "Systemd",
    "Method": "Reload",
    "NotifyServices": ["myapp.service"]
}
```

#### Directory-Specific Keys

##### ExcludeList (Optional, mutually exclusive with IncludeList)

- **Type**: Array of strings
- **Description**: List of absolute paths within the directory that should be
  excluded from synchronization. Cannot be used together with `IncludeList`
- **Example**:
  ```json
  "ExcludeList": ["/var/lib/myapp/temp.log", "/var/lib/myapp/cache/"]
  ```

##### IncludeList (Optional, mutually exclusive with ExcludeList)

- **Type**: Array of strings
- **Description**: List of absolute paths within the directory that should be
  synchronized. All other paths will be excluded. Cannot be used together with
  `ExcludeList`
- **Example**:
  ```json
  "IncludeList": ["/var/lib/myapp/config.json", "/var/lib/myapp/settings/"]
  ```

### Available Configuration Files

Currently 3 JSON config files are defined under
[`config/data_sync_list`](data_sync_list):

1. **common.json** - Contains files and directories of applications which run
   across all OpenBMC vendors
2. **open-power.json** - Contains files and directories of Open-Power
   applications
3. **ibm.json** - Contains files and directories specific to applications on IBM
   systems

### Adding Files/Directories to Sync

Users can either:

1. Add files or directories to one of the existing JSON files if they fit the
   categories above
2. Create a new JSON file following the schema in
   [`config/schema/schema.json`](schema/schema.json)

#### Add a new config JSON file

- If another vendor wants to add a new config JSON file into
  [`config/data_sync_list`](data_sync_list) with their list of data to be
  synced, define the schema compliant JSON file as `<vendor_name>.json`
- Update the `choices` field of `data_sync_list` option in
  [meson.options](../meson.options) with the JSON file name without .json file
  extension

## Rsync and Stunnel Configuration Generation

The data sync application uses rsync and stunnel to securely synchronize data,
and both components require configuration files with the necessary details. For
redundant BMC systems (BMC0 and BMC1), these configuration files are generated
at build time using `Meson` along with
[a helper shell script](../scripts/gen_rsync_stunnel_cfg.sh).

### Configuration file generation flow

- The build extracts the necessary configuration from the
  [sync socket configuration file](sync_socket) as per `Meson` build option
  `data_sync_list`. For example, `-Ddata_sync_list=common,<vendor>`, the last
  entry overrides earlier ones (vendor overrides common).

```sh
config/sync_socket/common_sync_socket.cfg
config/sync_socket/<vendor>_sync_socket.cfg
```

- Verify that the extracted configuration parameters match the required
  parameters listed below; otherwise, the process will fail.

```sh
BMC0_RSYNC_PORT
BMC1_RSYNC_PORT
BMC0_STUNNEL_PORT
BMC1_STUNNEL_PORT
BMC0_IP
BMC1_IP
```

- These extracted configuration parameters are substituted into the placeholders
  within the [rsync](rsync) and [stunnel](stunnel) configuration template files,
  producing the required BMC-specific rsync and stunnel configuration files,
  which are installed as shown below. The same configuration values are also
  used to generate the `config.h` file, enabling those parameters to be applied
  in the rsync CLI.

```sh
/usr/share/phosphor-data-sync/config/rsync/bmc0_rsyncd.conf
/usr/share/phosphor-data-sync/config/rsync/bmc1_rsyncd.conf
/usr/share/phosphor-data-sync/config/stunnel/bmc0_stunnel.conf
/usr/share/phosphor-data-sync/config/stunnel/bmc1_stunnel.conf
```

- These generated configuration file will be picked as per the local BMC's
  position. Refer [rsync and stunnel service files](../service_files).

**Note:** The data sync supports static IP addresses for syncing. The vendor is
expected to config their own static IP in the sync socket configuration file.

### How to add a new vendor sync socket config file

1. Create a new file `config/sync_socket/<vendor>_sync_socket.cfg`.
1. Build with `-Ddata_sync_list=common,<vendor>`.

**Note:** The vendor configuration override values in common.
