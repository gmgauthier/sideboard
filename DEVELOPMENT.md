# Sideboard — directional guide for Grok Build

Guest-suite installer / updater for the seven third-party LCOS applications.
This file is the plan of record. Implement against it; do not invent a store.

Written 2026-09-18. Audience for the first cut: Greg + Grok Build the same night.

Repo: [github.com/gmgauthier/sideboard](https://github.com/gmgauthier/sideboard). Origin: Gitea (`gitea.scriptorium/gmgauthier/sideboard`).
License: Unlicense, same as the suite.
House style: C++17, gtkmm-3.0, GTK3 CSS, Meson, Clearlooks chrome, no daemon, no online account, no AI features, no systemd dependency, no PackageKit.

---

## 1. What this is

A single gtkmm-3 window that lists every guest application intended for LCOS, shows the version installed (if any), shows the version available on that app’s GitHub Releases page, and offers Install or Upgrade. The button downloads the `.deb` from the release and installs it with a privileged helper.

It is a *cabinet* for side-loaded programs that sit beside the official ISO, not on it.

## 2. What this is not

- Not an “LCOS Store.”
- Not a Synaptic skin.
- Not PackageKit, Flatpak, AppImage, or snap.
- Not `lcos-updates`. Official LCOS updates stay in Bryan’s app (`apt-get update` / `upgrade` against the signed overlay at `lcos.lunduke.com`).
- Not a way to paste an arbitrary GitHub URL and install a random `.deb`.
- Not branded with Bryan Lunduke’s seal. Borrow the navy/ice/gray palette; do not pretend this is house software.

Window copy must say guest / GitHub / not official, every time the window is open.

The old backlog rule still holds: Synaptic + `lcos-updates` remain OS policy.

---

## 3. Name, binary, paths

**Product name:** Sideboard.
Furniture, British, accurate. These programs are side-loaded.

| Item | Value |
|---|---|
| Display name | Sideboard |
| Binary / Debian package | `sideboard` |
| Helper | `/usr/libexec/sideboard-helper` |
| Polkit action | `org.gmgauthier.sideboard.pkexec` |
| Polkit policy file | `data/org.gmgauthier.sideboard.policy` |
| Desktop file | System menu, *next to but not pretending to be* `lcos-updates` |
| Config | `~/.config/sideboard/sideboard.ini` |
| Cache | `~/.cache/sideboard/` (`catalog.json` + `debs/`) |
| About line | “Sideboard — guest software for The Lunduke Computer Operating System.” |

Rejected names: Updates, Store, anything with `lcos-` in the package name.
Acceptable alternates if Sideboard is taken: Cupboard, The Rack. Do not bikeshed. Ship Sideboard.

---

## 4. The seven apps (allow-list)

All under `https://github.com/gmgauthier`. All Unlicense. All gtkmm-3 / Meson. All ship a release `.deb` named:

```
{package}_{upstream}-1_amd64.deb
```

Inventory as of 2026-09-18. Versions move; the *pattern* does not.

| Display | Package / binary | Repo | Latest tag | `.deb` asset |
|---|---|---|---|---|
| EarBlaster | `earblaster` | `gmgauthier/earblaster` | v1.0.0 (2026-09-17) | `earblaster_1.0.0-1_amd64.deb` |
| Read-O-Matic | `readomatic` | `gmgauthier/readomatic` | v0.2.0 | `readomatic_0.2.0-1_amd64.deb` |
| YOLO-dex | `yolodex` | `gmgauthier/yolodex` | v0.1.2 | `yolodex_0.1.2-1_amd64.deb` |
| Kablamo! | `kablamo` | `gmgauthier/kablamo` | v0.1.1 | `kablamo_0.1.1-1_amd64.deb` |
| Ephemeris | `ephemeris` | `gmgauthier/ephemeris` | v0.1.1 | `ephemeris_0.1.1-1_amd64.deb` |
| Partyline | `partyline` | `gmgauthier/partyline` | v0.2.0 | `partyline_0.2.0-1_amd64.deb` |
| Dispatch | `dispatch` | `gmgauthier/dispatch` | v0.1.1 | `dispatch_0.1.1-1_amd64.deb` |

Each release also publishes an AppImage and a `.tar.xz`. **Sideboard ignores those.** Preferred install on LCOS is the `.deb`, same as every suite README:

```
sudo apt install ./earblaster_1.0.0-1_amd64.deb
```

Latest-release API (public, no token):

```
GET https://api.github.com/repos/gmgauthier/{repo}/releases/latest
```

Example asset URL:

```
https://github.com/gmgauthier/earblaster/releases/download/v1.0.0/earblaster_1.0.0-1_amd64.deb
```

GitHub release JSON includes `tag_name`, `assets[].name`, `assets[].browser_download_url`, `assets[].size`, and (as of these releases) `assets[].digest` in the form `sha256:<hex>`. Use the digest when present.

`debian/control` Package field matches the repo name in every case checked (EarBlaster: `Package: earblaster`). Trust `package == repo` for v1.

Architecture: every published `.deb` is `amd64`. If `dpkg --print-architecture` is not `amd64`, show a one-line dialog and refuse to install.

---

## 5. Catalog (compiled in)

One table in source. Adding an eighth app is a row plus a rebuild, not a protocol and not a URL the user types.

```cpp
struct App {
  const char* id;        // "earblaster"
  const char* display;   // "EarBlaster"
  const char* package;   // dpkg name, also the .deb prefix
  const char* owner;     // "gmgauthier"
  const char* repo;      // "earblaster"
  const char* summary;   // one line for a future details pane
};
```

v1 rows: the seven above, in that order (suite chronology / screenshot value).

v1.1: add Sideboard itself as row eight so the cabinet can update itself. Do **not** put Sideboard in the catalog until it has shipped its own `.deb` (milestone M4). Apt can replace a running binary the usual way; do not invent a fancy self-update dance.

No remote `catalog.json` in v1. Revisit only if adding apps without a rebuild becomes a real pain.

---

## 6. Window

Object to copy: Win98 Add/Remove Programs list + Norton LiveUpdate buttons.
WM provides chrome. No custom title bar.

A generated mock exists from the 2026-09-18 planning session (Clearlooks-ish dialog on a navy XFCE desktop). Treat it as tone, not pixel law. Real GTK3 + Clearlooks-Phenix on LCOS wins if they disagree.

```
+------------------------------------------------------------------+
| File   Help                                                      |
+------------------------------------------------------------------+
| Guest software for LCOS                                          |
| Not an official LCOS update. These install from GitHub releases. |
+------------------------------------------------------------------+
| App            Installed     Available     Status          [  ]  |
| EarBlaster     0.2.3         1.0.0         Update          [Upgrade]
| Read-O-Matic   —             0.2.0         Not installed   [Install]
| YOLO-dex       0.1.2         0.1.2         Up to date      [   —  ]
| Kablamo!       0.1.1         0.1.1         Up to date      [   —  ]
| Ephemeris      —             0.1.1         Not installed   [Install]
| Partyline      0.1.2         0.2.0         Update          [Upgrade]
| Dispatch       0.1.1         0.1.1         Up to date      [   —  ]
+------------------------------------------------------------------+
| Last checked: 18 Sep 2026, 12:18    [Refresh]  [Install all updates]
+------------------------------------------------------------------+
| Downloading partyline_0.2.0-1_amd64.deb … 140 KB                 |
+------------------------------------------------------------------+
```

### Widgets

- `Gtk::Window` + `Gtk::Box` vertical.
- Menu bar: **File** (Refresh, Install all updates, Quit), **Help** (About).
- Header band: LCOS navy `#0B1D38`, ice text `#E8F2FF` for the title line. Caption under it in ordinary dark gray on client gray `#E6E6E1`.
- `Gtk::TreeView` (or a `Gtk::ListBox` of rows if TreeView-plus-button is miserable). Columns: App, Installed, Available, Status, action.
- Action cell is a real button per row: **Install**, **Upgrade**, or a disabled em dash when up to date.
- Bottom strip: “Last checked: …” + Refresh + Install all updates.
- Status line + a determinate progress bar while a `.deb` is downloading or apt is running.

### Status strings

| State | Status text | Button |
|---|---|---|
| No network yet, never cached | Available = “—” | disabled |
| Not installed, latest known | Not installed | Install |
| `dpkg` version older than latest | Update available | Upgrade |
| Versions equal | Up to date | disabled “—” |
| Check or install in flight | Checking… / Installing… | disabled |
| Failure | Error | Install/Upgrade still enabled so they can retry |

Colour on Status: Update available = dark amber, Up to date = dark green, Not installed = ordinary text, Error = dark red. Do not invent a traffic-light badge language.

### About

Third-party, not LCOS house software. List the seven apps by name. Link the GitHub org in plain text, not a browser chrome.

### Parked UI (not v1)

Uninstall, release-notes pane, “Open homepage”, row icons as identity, auto-check on login.

Optional 16×16 row icons are fine if they are cheap and do not become the product. The mock used speaker / book / card / mine / calendar / chat / envelope. Do not block M0 on icons.

---

## 7. How versions are decided

### Installed

```
dpkg-query -W -f='${Version}\n' earblaster
```

Missing package → Installed = “—”. Do not sniff `/usr/bin` or `.desktop` files. The `.deb` *is* the install.

dpkg version is `{upstream}-1` (example `1.0.0-1`). Display the upstream in the grid; keep the full Debian version internally.

### Available

Unauthenticated GitHub:

```
GET https://api.github.com/repos/gmgauthier/{repo}/releases/latest
Accept: application/vnd.github+json
User-Agent: sideboard/0.1 (+https://github.com/gmgauthier/sideboard)
```

Read `tag_name` (`v1.0.0` → `1.0.0`). Pick the asset whose name matches `{package}_*_amd64.deb`. Store `browser_download_url`, size, and `digest` if present.

Seven GETs per Refresh.

### Compare

```
dpkg --compare-versions "$installed" lt "$available_upstream-1"
```

Use dpkg’s comparer, not string compare. Display upstream in the grid.

### Cache

Write the last successful payload to `~/.cache/sideboard/catalog.json`. On a machine with no network, still open the window, still show Installed from dpkg, and show Available from cache labelled so it is obvious it is cached (“Available (cached)” is enough). Cache is an offline courtesy, not a freshness protocol. No TTL required. Refresh always hits the network.

### Network policy (locked 2026-09-18)

- Unauthenticated `/releases/latest` only.
- No GitHub token, no PAT, no login.
- No second catalog repo.
- No HTML scrape fallback in v1.
- Expected audience is tiny (tens of users a month, fifty at the outside). **Do not design around API rate limits.** Unauth GitHub’s 60 req/hour/IP is irrelevant at that scale.
- If Refresh gets a 403/429, the row (or a single banner) says the check failed and they try later.

libcurl. Do not rely on GVfs http.

JSON: the latest-release document is small. A dedicated extractor for the fields we need is enough. Do not add nlohmann-json as a new runtime dependency if a 200-line reader will do. json-c is acceptable if it is already on the ISO and saves time.

---

## 8. Install / upgrade path

1. Download the `.deb` to `~/.cache/sideboard/debs/{filename}`.
2. If the API gave a `sha256:` digest, verify it. Mismatch → Error dialog, do not install.
3. Elevate:

```
pkexec /usr/libexec/sideboard-helper install /absolute/path/to/file.deb
```

4. Helper runs only:

```
DEBIAN_FRONTEND=noninteractive
LANG=C.UTF-8
LC_ALL=C.UTF-8
/usr/bin/apt-get install -y /absolute/path/to/file.deb
```

`apt-get install /path/to.deb` pulls Depends. **Never** `dpkg -i` alone. That is how you strand someone on a missing gtkmm shlib.

5. Re-run `dpkg-query` for that package and refresh the row.

Failed download, bad hash, cancelled polkit, apt error: Status = Error, dialog with the first apt error line.

“Install all updates” runs the pending Upgrade rows sequentially. One polkit prompt should cover the batch if the action is `auth_admin_keep`. One progress line at the bottom. Stop the batch on the first hard failure (auth cancel, apt failure); leave already-finished rows as they are.

Do not install rows that are already up to date. “Install all updates” means updates, not “install everything missing.” A separate “Install missing” can wait.

---

## 9. Privileged helper

Copy the *shape* of `lcos-updates-helper`, not its verbs.

Official helper (do not call it, do not share its polkit action):

- Path: `/usr/libexec/lcos-updates-helper`
- Action: `org.lunduke.lcos-updates.pkexec`
- Verbs: `simulate` | `upgrade`
- Runs `/usr/bin/apt-get update` then `upgrade`
- `allow_gui=true`, `auth_admin_keep` when active

Sideboard helper:

- Path: `/usr/libexec/sideboard-helper`
- Action: `org.gmgauthier.sideboard.pkexec`
- Verb: exactly one, `install <absolute-path>`
- `argv` must be `sideboard-helper install /abs/path.deb` or exit 2
- Path must be an absolute regular file ending in `.deb`
- Size cap: 50 MB (current suite debs are 40–160 KB; AppImages are 11–140 MB and must never be accepted)
- After validating, the helper may copy the file into `/var/cache/sideboard/` (root-owned) and install from there, so the user cannot swap the file between check and apt. That copy is recommended, not optional if it is cheap.
- Never `/bin/sh -c`
- Never `apt-get update`
- Never `apt-get upgrade`
- Never install a path the user typed that is not a catalog download
- Capture apt stdout/stderr; emit a short result on stdout the GUI can parse (`OK` / `ERR <first line>`)
- Timeouts. Official helper uses poll + timeouts; do the same. Five minutes is plenty for these debs.

Polkit policy sketch:

```xml
<action id="org.gmgauthier.sideboard.pkexec">
  <description>Install guest LCOS software from a downloaded .deb</description>
  <message>Authentication is required to install guest software</message>
  <defaults>
    <allow_any>auth_admin</allow_any>
    <allow_inactive>auth_admin</allow_inactive>
    <allow_active>auth_admin_keep</allow_active>
  </defaults>
  <annotate key="org.freedesktop.policykit.exec.path">/usr/libexec/sideboard-helper</annotate>
  <annotate key="org.freedesktop.policykit.exec.allow_gui">true</annotate>
</action>
```

Vendor: Greg Gauthier / the Sideboard repo. Not “LCOS”. Not lunduke.com.

Devuan/LCOS already has pkexec. No systemd unit. No user-bus daemon required to launch Sideboard.

---

## 10. Stack and tree

Match EarBlaster / the rest of the suite.

```
sideboard/
  UNLICENSE
  README.md
  DEVELOPMENT.md          # can start as a short pointer at this file’s decisions
  meson.build
  debian/                 # same layout as earblaster
  data/
    sideboard.desktop
    org.gmgauthier.sideboard.policy
    sideboard.css
  src/
    main.cpp
    window.cpp / window.hpp
    catalog.cpp / catalog.hpp   # static allow-list
    local.cpp / local.hpp       # dpkg-query
    remote.cpp / remote.hpp     # libcurl + latest-release parse
    compare.cpp / compare.hpp   # dpkg --compare-versions wrapper
    fetch.cpp / fetch.hpp       # download + sha256
    helper.cpp                  # built as sideboard-helper
  scripts/
    lint.sh
    release.sh                  # copy from an existing suite app and rename
```

Dependencies (runtime / build):

- `libgtkmm-3.0-dev`
- `libcurl4-openssl-dev` (or whatever Devuan Excalibur names the curl dev package)
- `libpolkit-gobject-1` is not required in-process if we only exec `pkexec`
- `apt`, `dpkg`, `pkexec` assumed present on LCOS
- `openssl` or a small SHA-256 (libcrypto is already on the box)

Theming: same process-only fallback the suite standardized on 2026-09-16 — Clearlooks-Phenix, then Clearlooks, then Adwaita:light. `GTK_THEME` in the environment still wins.

Single instance: flock + Unix socket, same as EarBlaster. No D-Bus.

Config ini: last-check timestamp only in v1.

---

## 11. Milestones

Ship in this order. Do not start M2 until M1 shows real tags in the grid on a live network.

### M0 — window

Catalog hardcoded. Installed from `dpkg-query`. Available = “—”. Buttons disabled. Enough to screenshot on LCOS. This is the “the OS has a cabinet” shot.

### M1 — Refresh

libcurl + parse latest release. Status column live. Cache file written. A failed check does not crash the window.

### M2 — Install one

Download, hash, pkexec helper, `apt-get install`, row refresh. **This is the product.**

### M3 — Install all updates

Sequential pending upgrades, one progress line, stop on first hard failure.

### M4 — Packaging

`debian/`, desktop file, polkit policy, helper in `/usr/libexec`, first GitHub release with `sideboard_0.1.0-1_amd64.deb`.

Bootstrap remains manual on purpose:

```
sudo apt install ./sideboard_0.1.0-1_amd64.deb
```

After M4, adding Sideboard as catalog row eight is a point release.

### Parked (not this night)

- Uninstall
- Release-notes pane
- AppImage install path
- Auto-check on login
- Remote catalog
- Any architecture other than amd64
- GitHub authentication
- Anything that talks to `lcos.lunduke.com`
- “Install missing” as a separate verb
- Becoming a general `.deb` sideload tool

---

## 12. Locked decisions

| Decision | Lock |
|---|---|
| Name | Sideboard |
| Scope | Allow-listed guest apps only |
| Asset | Release `.deb`, amd64, `{pkg}_{ver}-1_amd64.deb` |
| Installed version | `dpkg-query` |
| Available version | GitHub `releases/latest`, no token |
| Rate limits | Ignore as a design input. Audience is tens of people a month. |
| Cache | Last-good payload for offline display only |
| Privilege | pkexec + dedicated helper, not `sudo` in a shell, not PackageKit |
| Helper verbs | `install <deb>` only. Never `upgrade`, never `update`. |
| Apt invocation | `apt-get install -y /abs/path.deb` |
| Toolkit | C++17, gtkmm-3, Meson, Clearlooks |
| Title bar | Window manager |
| Store language | Forbidden in UI copy and in the README |
| Self-update | After M4 only |
| License | Unlicense |

---

## 13. Risks (do not “fix” with new product surface)

1. **Name collision with “updates.”** The header sentence exists so someone does not file a bug at Bryan. Keep it.
2. **Helper as a root installer.** Pin verb, suffix, size, and preferably a copy into `/var/cache/sideboard/`.
3. **GitHub API shape changes.** If `/releases/latest` ever requires auth, stop and talk. Do not scrape HTML in a hurry.
4. **amd64 only.** Refuse clearly.
5. **Running-binary replace.** Apt already handles this for M4 self-update. Do not write a second installer inside the installer.

---

## 14. Differentiation from `lcos-updates`

| | `lcos-updates` | Sideboard |
|---|---|---|
| Owner | Bryan / LCOS | Greg / guest suite |
| Source | signed apt overlay | GitHub release `.deb`s |
| Helper verbs | simulate, upgrade | install one local `.deb` |
| `apt-get update` | yes | no |
| `apt-get upgrade` | yes | no |
| Catalog | whatever apt knows | seven compiled-in rows |
| Menu | official System | official System, different name and icon |

Do not share a polkit action. Do not shell out to `lcos-updates-helper`.

---

## 15. First README paragraph (use this tone)

Sideboard is a cabinet for the guest applications written for The Lunduke Computer Operating System. It is not an official LCOS updater and it is not a store. It lists EarBlaster, Read-O-Matic, YOLO-dex, Kablamo!, Ephemeris, Partyline, and Dispatch; shows the version `dpkg` has versus the version on GitHub Releases; and installs the published `.deb`.

---

## 16. Suggested first commit shape for Grok Build

1. Create `gmgauthier/sideboard` on GitHub, Unlicense, default branch `master`.
2. Scaffold meson + gtkmm window with the header copy and an empty TreeView.
3. Hardcode the catalog struct with the seven rows.
4. Fill Installed via `dpkg-query`.
5. Stop and screenshot (M0). Then M1 against live GitHub.

Do not implement Uninstall, a store banner, a search box, or a “paste a URL” field. If a decision is not in this file, it is not in v1.
