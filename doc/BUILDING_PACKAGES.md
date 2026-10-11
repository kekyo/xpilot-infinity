# Building release packages

Build from an independent checkout of xpilot-infinity. The native targets use
the existing GCC/MinGW Makefile build. The npm release requires Node.js 26.4 or
later, npm, make, CMake, and the host tools listed in [INSTALL](../INSTALL).
`emsdk-env` installs and selects the fixed Emscripten SDKs; a system Emscripten
installation is not used. The SDK download requires network access on first use.

```sh
./build_package_npm.sh
```

This installs the two npm workspaces' locked development dependencies, builds
the browser and server, and writes the release archive to `artifacts/npm/`.
The archive contains the runtime and assets and has no installation scripts or
runtime npm dependencies. The public entry point is the `xpilot-infinity` CLI;
the internal host modules are not a published library API.

To build every package, first prepare the existing Linux container images with
`./prereq.sh`, and install MinGW, NSIS, and the native prerequisites from INSTALL:

```sh
./build_package_all.sh
```

The wrapper resolves one screw-up version and commit ID, builds the browser
once as part of the npm release, then embeds that browser in the native deb and
Windows packages. Artifacts are placed under `artifacts/npm`, `artifacts/deb`,
and `artifacts/windows`. `--version` overrides the product version; Linux
distribution filters limit only the Debian/Ubuntu matrix. For example:

```sh
./build_package_all.sh --distro debian --release bookworm --arch amd64 --jobs 8
```

For native-only development, `./build.sh --target native` and
`./build.sh --target windows` continue to work without the npm toolchain. Native
packaging includes `xpilot-browser/dist` when it has been built. Use the complete
package wrapper for releases so all formats contain matching browser assets.

`npm test` in `xpilot-server` builds both applications and runs server tests,
including installation of the archive without development dependencies or
install scripts. Set `XPILOT_CLIENT_BINARY` to a built SDL client to include the
three-protocol gameplay test. Set `XPILOT_CONTACT_TARGET_PROBE` to the built
native probe for the pending-login/shutdown regression.
The installed native client's complete E2E can also use the Node.js server by
setting `XPILOT_E2E_SERVER_BINARY` to the built Node.js CLI when running
`tests/run-e2e.sh` with its usual native test paths. Automatic-exit cases
explicitly select `+noQuit`, since the npm CLI stays available by default.
The browser's `npm test` uses Playwright MCP. Set
`XPILOT_SERVER_BINARY` to the native executable or the built Node.js CLI.
The native regression suites are `make check` and `./build.sh --target windows
--arch all --test` (Wine). Windows desktop behavior that Wine cannot support
requires a Windows machine; it must not be reported as verified by Wine.

See [plan4](plans/plan4-node-wasm.md) for the implementation and validation record.
