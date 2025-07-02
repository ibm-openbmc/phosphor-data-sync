# phosphor-data-sync

The phosphor-data-sync application will be used in a redundant BMC system to
synchronize data between BMCs according to the data file details configured for
BMC applications.

### To build

```
meson setup builddir
meson compile -C builddir
```

### Signal-Triggered Watched Paths Logging (Debug Utility)

Inspect watched paths using the `SIGUSR1` signal. This feature is helpful for
verifying which paths are currently being monitored by the system.

If you want to check if some particular paths are being monitored or not, create
a file `/tmp/data_sync_check_watched_paths.txt` and enter the paths line by
line. Then send the signal, you can see the output in
`/tmp/data_sync_check_output.txt`.

If the user input file is not present, then on receiving the signal, it will
write all the currently monitored path to
`/tmp/data_sync_current_watched_paths.json`.

To trigger the utility, send a `SIGUSR1` signal to the
`phosphor-rbmc-data-sync-mgr` process:

```bash
kill -10 <pid_of_phosphor-rbmc-data-sync-mgr>
```
