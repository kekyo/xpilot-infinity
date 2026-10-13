# Distribution packages

XPilot Infinity builds release artifacts from a Linux host.  Debian packages are
built inside target-specific Podman containers so that their runtime
dependencies are resolved against the selected Debian or Ubuntu release.

## Host prerequisites

Install Podman, `qemu-user-static`, `dpkg-dev`, binutils, Node.js, Info-ZIP
`zip`, and [`screw-up`](https://github.com/kekyo/screw-up).  Initialize the
pinned source dependencies before packaging:

```sh
git submodule update --init --recursive
```

Prepare the reusable Linux package images, install the MinGW/Wine prerequisites
described in `doc/WINDOWS.md`, then build every package:

```sh
./prereq.sh
./build_package_all.sh
```

This generates the complete Debian/Ubuntu matrix and then the Windows x86
(32-bit) and x86_64 (64-bit) ZIP archives and NSIS installers. Distribution,
release, and architecture filters apply only to the Debian/Ubuntu matrix; both
Windows architectures are always built.  The `--version`, `--jobs`, and
`--debug` options apply to both package families.

Run `./prereq.sh` again after dependency requirements change. It updates the
images from the current recipe and reuses unchanged build layers. Use
`./prereq.sh --force` to rebuild without cached layers. If packaging reports
missing server tray development dependencies, refresh the prerequisite images
before retrying.

The supported Debian package matrix is:

| Distribution | Release | Architectures |
| --- | --- | --- |
| Debian | bookworm | amd64, i386, arm64, armhf |
| Debian | trixie | amd64, i386, arm64, armhf, riscv64 |
| Ubuntu | 22.04 | amd64, arm64 |
| Ubuntu | 24.04 | amd64, arm64 |
| Ubuntu | 26.04 | amd64, arm64 |

Both image preparation and package generation accept `--distro`, `--release`,
and `--arch` comma-separated filters.  For example:

```sh
./prereq.sh --distro ubuntu --release 24.04 --arch amd64
./build_package.sh --target deb --distro ubuntu --release 24.04 --arch amd64
```

Architecture aliases include `x86_64|amd64`, `i686|i386`,
`arm64|aarch64`, and `armv7l|armv7|armhf`.  Ubuntu `jammy` and `noble` are
accepted as release aliases.  Add `--debug` for an unoptimized build, or use
`--version` to override the version derived by `screw-up`.

Each package contains both graphical clients, the dedicated server, utilities,
game data including the sound map and samples, documentation, manual pages, an
XDG desktop launcher, and the application icon. Its generated `Depends` field
includes the OpenAL and freealut runtime packages (`libopenal1` and
`libalut0`). The build validates those dependencies, installed paths, and the
ELF architecture of every executable. Artifacts are written as:

```text
artifacts/deb/xpilot-infinity-<version>-<distro>-<release>-<deb-arch>.deb
```

## Dedicated server container image

The root [`Dockerfile`](../Dockerfile) packages the Node.js/Wasm server and
browser client on a pinned Node.js image. Both CPU images use the same
application artifacts and product identity. For prerequisites, port mappings,
saved data, HTTPS, and Quadlet setup, see the
[runtime guide](../README.md#running-the-server-with-podman), translated from
the [Japanese guide](../README_ja.md#podman-でサーバーを実行する).

Build a local Linux amd64/arm64 manifest with Podman:

```sh
image=localhost/xpilot-infinity-server:local
./build_container_image.sh --tag "$image"
podman manifest inspect "$image"
```

Foreign CPU verification needs registered QEMU/binfmt support. The script
executes both CPU images and verifies the Node.js architecture, HTTP assets,
WebSocket handshake, graceful shutdown, and saved rankings before updating
the final tag. A failed build or check prevents publishing. Individual
`${image}-amd64` and `${image}-arm64` tags are also available for local tests.

`--platform linux/amd64` or `--platform linux/arm64` builds a single CPU image.
`--jobs 1` builds sequentially. `--skip-app-build` reuses existing application
artifacts only when server and browser build identities match the requested
version and revision. Omit it when application sources have changed.

The wrapper embeds the resolved product version, revision, and OCI source
metadata. `--version`, `--revision`, `--node-image`, and `--source-url` override
those inputs. `--print-version` prints the resolved version; install the npm
dependencies first, or supply both version and revision explicitly.

`build_container_image_all.sh` forwards to the same builder with both CPUs.
Publishing requires an explicit `--push`, which sends the verified manifest
and its images under both the version and `latest` tags:

```sh
podman login docker.io
./build_container_image_all.sh --push \
  --registry-image docker.io/YOUR_NAMESPACE/xpilot-infinity-server
```

The default repository is `docker.io/kekyo/xpilot-infinity-server`. Replace it in
the command and in
[`containers/xpilot-infinity-server.container`](../containers/xpilot-infinity-server.container)
when publishing to another namespace. The builder does not log in to a registry.

## Windows packages

The Linux-hosted MinGW build creates separate 32-bit and 64-bit Windows ZIP
archives and NSIS installers because both packages use the same executable
names. `build_package_all.sh` creates both forms after the Linux packages. To
build and test only the Windows packages, run:

```sh
./build.sh --target windows --arch all --test
```

The ZIP files contain the SDL client, dedicated server, game data including
sound samples, `OpenAL32.dll`, `libalut.dll`, and the corresponding
project licenses from the `windows-package` directory. Info-ZIP `zip` uses its
maximum compression setting. Entries are sorted, assigned a fixed timestamp
and permissions, and stripped of host-specific extra fields so the result is
reproducible for identical input. Artifacts are written as:

```text
artifacts/windows/xpilot-infinity-<version>-windows-x86.zip
artifacts/windows/xpilot-infinity-<version>-windows-x86_64.zip
artifacts/windows/xpilot-infinity-<version>-windows-x86-setup.exe
artifacts/windows/xpilot-infinity-<version>-windows-x86_64-setup.exe
```

The ZIP files are the portable distributions. The installers contain the same
runtime tree, add the Start menu shortcut and product uninstaller, and offer an
unchecked manual-start server-service component. The Wine suite verifies both
the default and service-enabled installation paths before running the portable
network and graphical integration cases.

Use `--artifact-root` to select another output directory and
`--package-version` to override the version derived by `screw-up`.  The full
cross-build and Wine prerequisites are documented in `doc/WINDOWS.md`.
