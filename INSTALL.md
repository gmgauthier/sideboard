# Installing Sideboard

Two ways to get a binary, in the order LCOS cares about:

| Artifact | Who it is for |
|---|---|
| **`.deb`** | LCOS, Devuan Excalibur, Debian Trixie. Preferred. |
| **Source tarball** | Distro packagers and `meson setup && ninja install`. |
| **Git build** | Developers. See below. |

There is no AppImage. Version comes from `meson.build` (currently `0.1.0`).

## Runtime needs

- GTK 3 / gtkmm-3.0
- libcurl
- pkexec (polkit)
- apt

On Debian / Devuan / LCOS:

```
sudo apt install libgtkmm-3.0-1t64 libcurl4t64 pkexec apt
```

## 1. Debian package (preferred)

From a release `.deb`:

```
sudo apt install ./dist/sideboard_0.1.0-1_amd64.deb
```

Or, from this tree:

```
./scripts/release.sh deb
sudo apt install ./dist/sideboard_0.1.0-1_amd64.deb
```

That installs:

- `/usr/bin/sideboard`
- `/usr/libexec/sideboard-helper`
- `/usr/share/polkit-1/actions/org.gmgauthier.sideboard.policy`
- `/usr/share/applications/sideboard.desktop`
- `/usr/share/icons/hicolor/scalable/apps/sideboard.svg`
- `/usr/share/sideboard/sideboard.css`

Launch from the menu or `sideboard`. Install and uninstall of guest apps go through `pkexec` and the helper.

## 2. From source

```
meson setup build
ninja -C build
sudo ninja -C build install
```

Helper and polkit policy must be installed under `/usr` for `pkexec` to accept them. Running only `build/sideboard` is fine for Refresh; Install needs the helper at `/usr/libexec/sideboard-helper`.
