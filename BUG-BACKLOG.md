# Bug backlog

Reviewed 2026-10-01 against the 0.3.0 sources.

`meson test` runs `tests/test_version.cpp` (`version`), `tests/test_instance.cpp` (`instance`), `tests/test_helper.cpp` (`helper`), and `tests/test_fetch.cpp` (`fetch`). `fetch` points a download and a release check at a local server that stalls, sets the cancel flag, and checks that each returns within a few seconds and that the partial download is removed. `helper` builds small debs with `dpkg-deb` and runs the helper's install and remove logic against a fake `apt-get`. It checks that install refuses a deb whose `Package:` field is not in the catalog, whatever the file is called, that a symlinked deb is refused, that the cache directory and copy are `0700` and `0600`, that the copy is removed when `apt-get` fails, that a published digest is checked against root's copy and a mismatch or malformed digest is refused, that the GUI passes that digest to the helper, that installing the copy a previous install left in the cache keeps it whole, and that remove refuses a package outside the catalog. `instance` checks that a second launch raises the primary and leaves its socket in place, so a third launch can still raise it. `version` checks `version_older`, `display_upstream`, and the compiled-in catalog (11 apps, including Listen-O-Matic). It does not install packages. `query_installed` passes a `dpkg-query` argv through `Glib::spawn_command_line_sync`; catalog package names are plain identifiers, and that call is not a shell injection.

## Open

### A partial refresh shows stale cache as a live result

- Severity: incorrect
- Confidence: high
- Where: `src/window.cpp:552`
- Trigger: Refresh where at least one repo returns a release and another fails with anything except HTTP 404 (timeout, 5xx). Or a 404 for one repo while another succeeds.
- Outcome: The failed row is filled from the old cache entry and marked `ok`. If any live result succeeded, the column header is set to `Available` and `save_cache` writes the cache back, including a 404's previous release. The next launch offers that release again. There is no per-row cached marker.

### Install timeout does not kill apt's children

- Severity: incorrect
- Confidence: medium
- Where: `src/helper.cpp:150`
- Trigger: `apt-get install` of the copied deb is still running after five minutes (lock wait, a slow download, or a stuck unpack).
- Outcome: The helper `kill`s only the `apt-get` pid. The child is not in its own process group, so `dpkg` and apt method children are not signaled. The GUI reports failure, but a child `dpkg` can keep installing as root or can be left holding the dpkg lock.

## Closed

### Close or Quit blocks the UI thread for the rest of the transfer

- Severity: incorrect
- Confidence: high
- Where: `src/window.cpp` `stop_refresh`, `run_download`, `src/fetch.cpp` `download_file`, `src/remote.cpp` `http_get`
- Trigger: Start a download or a refresh, then close the window or choose Quit before it finishes.
- Outcome: The destructor calls `stop_refresh()`, which joins `refresh_thread_` on the UI thread. `run_download()` never reads `cancel_`. The curl progress callback does not abort, and the download timeout is 300 seconds. A stalled download freezes quit for that long. The deb is not installed, because the done handler was disconnected before the join.
- Fixed in v0.3.7: The download and each release check read the cancel flag from curl's progress callback and abort. Close or Quit joins a worker that stops within about a second, instead of waiting out a 300-second download timeout.

### Re-install from `/var/cache/sideboard` truncates the deb

- Severity: data-loss
- Confidence: medium
- Where: `src/helper_core.cpp` `copy_fd`
- Trigger: `sideboard-helper install /var/cache/sideboard/foo_1_amd64.deb` (the copy a previous install left behind).
- Outcome: Destination is that same path. `open` uses `O_TRUNC` before the read, so the file is emptied and `apt-get` fails. The cached copy is gone.
- Fixed in v0.3.6: The helper copies into a new temporary file in `/var/cache/sideboard` and renames it over the destination only when the copy is complete, so installing the cached copy itself keeps the deb intact.

### The hash the GUI checks is not the file root installs

- Severity: security
- Confidence: high
- Where: `src/window.cpp` `on_download_done`, `src/helper_core.cpp` `install_deb`
- Trigger: Install or Upgrade a release that publishes a `sha256:` digest. While the authentication dialog is up, replace the deb in `~/.cache/sideboard/debs/`.
- Outcome: The GUI hashes the user-writable cache file, then `pkexec` installs whatever is at that path when the helper starts. The helper does not take a digest and does not hash the file. `Gtk::Main::iteration()` runs between the check and `pkexec`, so the window is real. The copy into `/var/cache/sideboard` happens too late to stop the swap.
- Fixed in v0.3.5: The GUI passes the published `sha256:` digest to `sideboard-helper install`. The helper hashes its own root-owned copy and refuses the install, removing that copy, when the digest does not match. A swap in `~/.cache/sideboard/debs/` after the GUI's check is caught.

### Helper follows symlinks and leaves a world-readable copy

- Severity: leak
- Confidence: high
- Where: `src/helper_core.cpp` `install_deb`, `copy_fd`
- Trigger: `sideboard-helper install /tmp/x_1_amd64.deb` where that path is a symlink to a root-readable regular file (for example `/etc/shadow`) within the size limit.
- Outcome: `stat` and `open` follow the symlink, so the regular-file check passes. The target bytes are written to `/var/cache/sideboard/x_1_amd64.deb` as mode `0644`. The directory is created `0755`. Nothing unlinks that copy if `apt-get` then fails. On a multi-user machine other users can read it.
- Fixed in v0.3.4: The deb is opened with `O_NOFOLLOW` and checked with `fstat` on that descriptor, so a symlink is refused. `/var/cache/sideboard` is created, or tightened, to `0700`, the copy is `0600`, and the copy is removed when `apt-get` fails.

### Helper install is not limited to catalog packages

- Severity: security
- Confidence: high
- Where: `src/helper_core.cpp` `install_deb`
- Trigger: After one authenticated Sideboard install, another process in the session runs `sideboard-helper install` on any absolute `*_amd64.deb` during the polkit keep window. `data/org.gmgauthier.sideboard.policy` is `auth_admin_keep` for the active session, pinned to the helper binary and not to its arguments.
- Outcome: `remove` checks the catalog. `install` only checks that the basename does not start with `.`, contains no `/` or `..`, and ends with `_amd64.deb`. It never reads the deb's `Package:` field. Root runs that deb's maintainer scripts.
- Fixed in v0.3.3: The helper reads the `Package:` field of its own copy with `dpkg-deb` and installs only a catalog package. A deb whose field is not in the catalog, or that `dpkg-deb` cannot read, is refused and its copy removed.

### A second launch deletes the running instance's socket

- Severity: incorrect
- Confidence: high
- Where: `src/application.cpp` `close_socket`, `src/instance.cpp` `InstanceGuard::close`
- Trigger: Start Sideboard, then start it again.
- Outcome: The second process fails the lock, `send_present()` succeeds, and `quit()` runs the destructor. `close_socket()` always `unlink`s `sideboard.sock`, even though this process never bound it (`listen_fd_` stays `-1`). The primary still holds `sideboard.lock`, but the socket is gone. A third start prints `sideboard: already running` and does not raise the window. That stays broken until the primary exits.
- Fixed in v0.3.2: Only the process that bound `sideboard.sock` removes it. A second launch raises the primary and exits without touching the socket, so a third launch still raises the window.
