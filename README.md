# phosphor-data-sync

The phosphor-data-sync application will be used in a redundant BMC system to
synchronize data between BMCs according to the data file details configured for
BMC applications.

### To build

```
meson setup builddir
meson compile -C builddir
```

### Signal-Triggered Watched Paths Logging (DataSync Debug Utility)

Inspect watched paths using the `SIGUSR1` signal. This feature is helpful for
verifying which paths are currently being monitored by the phosphor-data-sync.

#### To check whether certain files or directories are currently being monitored

If you want to check if some particular paths are being monitored or not, create
a file `/tmp/data_sync_paths_info.lsv` and enter the paths line by line. Then
send the signal (Refer below), you can see the output in the same file.

```bash
$ cat data_sync_paths_info.lsv
rtest
/var/lib
$ kill -SIGUSR1 <pid_of_phosphor-rbmc-data-sync-mgr>
$ cat data_sync_paths_info.lsv
Individual user entered paths results:
Not Watching path: rtest
Watching path: /var/lib
```

If the user input file is not present, then on receiving the signal, it will
dump all the currently monitored paths to `/tmp/data_sync_paths_info.json`.
Refer below for example output

```bash
$ cat data_sync_paths_info.json
[
    "/var/lib",
    "/var/lib/phosphor-state-manager"
]
```

To trigger the utility, send a `SIGUSR1` signal to the
`phosphor-rbmc-data-sync-mgr` process:

```bash
pgrep -f phosphor-rbmc-data-sync-mgr
kill -SIGUSR1 <pid_of_phosphor-rbmc-data-sync-mgr>
```
