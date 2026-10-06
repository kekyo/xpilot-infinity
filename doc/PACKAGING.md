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

The root [`Dockerfile`](../Dockerfile) is a multi-stage build dedicated to
`xpilot-infinity-server`. Its builder stage disables the SDL and X11 clients,
replay tools, map editor, and sound. The runtime stage uses the same pinned
Debian slim base and contains only the stripped server, standard server data,
`libexpat`, `zlib`, and their base-system dependencies. It does not contain a
compiler or the client-side SDL dependency tree.

The runtime contract is:

- Linux `amd64` and `arm64` images are supported.
- The process runs as the fixed non-root UID/GID `10001:10001`.
- `/var/lib/xpilot-infinity-server` is the working directory for relative
  output, but it is not declared as an automatic anonymous volume.
- TCP port 15345 and the `ndh.xp2` map are the safe defaults.
- `SIGTERM` is the image stop signal.
- The image has no generic health check because server readiness requires an
  XPilot protocol exchange rather than a TCP-open check.

Build the native platform with the Podman wrapper:

```sh
version=$(./build_container_image.sh --print-version)
image="docker.io/kekyo/xpilot-infinity-server:$version"
./build_container_image.sh --tag "$image"
podman image inspect "$image"
```

The wrapper embeds the resolved product version, source revision, and OCI
source metadata. `--version`, `--revision`, `--jobs`, `--base-image`, and
`--source-url` provide explicit release overrides. It only creates local
images and never logs in to a registry or pushes.

For one manifest containing both supported architectures, install
`qemu-user-static` and run:

```sh
version=$(./build_container_image.sh --print-version)
image="docker.io/kekyo/xpilot-infinity-server:$version"
./build_container_image.sh \
  --platform linux/amd64,linux/arm64 \
  --tag "$image"
podman manifest inspect "$image"
```

Every `RUN` instruction executes for each target architecture, so registered
user-mode emulation is required when the build host is not native to that
architecture. Inspect the resulting manifest and test at least the native
image before publishing.

Publishing is a separate, operator-controlled release step. After
authenticating and validating the local manifest, the release operator may
push the immutable version tag and, if desired, the same manifest as `latest`:

```sh
podman login docker.io
podman manifest push --all "$image" "docker://$image"
podman manifest push --all "$image" \
  docker://docker.io/kekyo/xpilot-infinity-server:latest
```

For a single-platform image, use `podman push "$image"` instead. Replace the
Docker Hub namespace in the examples and in
[`containers/xpilot-infinity-server.container`](../containers/xpilot-infinity-server.container)
if the final repository is not owned by `kekyo`. The runtime and optional
rootless Quadlet instructions are documented in the main
[`README.md`](../README.md#running-the-server-with-podman).

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
