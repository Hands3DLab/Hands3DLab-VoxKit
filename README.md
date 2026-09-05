# Hands3DLab-VoxKit

[English](README.md) | [简体中文](README.zh-CN.md)

<table>
    <tr>
        <td width="50%"><img src="electron/assets/screenshots/en/s1.png" alt="Hands3DLab-VoxKit interface screenshot 1"></td>
        <td width="50%"><img src="electron/assets/screenshots/en/s2.png" alt="Hands3DLab-VoxKit interface screenshot 2"></td>
    </tr>
    <tr>
        <td width="50%"><img src="electron/assets/screenshots/en/s3.png" alt="Hands3DLab-VoxKit interface screenshot 3"></td>
        <td width="50%"><img src="electron/assets/screenshots/en/s4.png" alt="Hands3DLab-VoxKit interface screenshot 4"></td>
    </tr>
</table>

Hands3DLab-VoxKit is an offline Electron desktop client for converting OBJ, GLB, and STL meshes into voxel models. It supports configurable voxel resolution, Binvox, OBJ, STL, and 3MF export, print-scale settings, slicer integration, and export-history management. All conversion runs locally inside the desktop client.

This is particularly useful for 3D printing models made from many fine components—for example, traditional architectural bracket blocks, columns, and roof ridges. When these small details cannot be reproduced, especially with a 0.4 mm nozzle, VoxKit lets you voxelize the entire 3D model in one step instead of reworking each component individually.

## Features (September 5, 2026)

- Offline, local mesh-to-voxel conversion
- macOS Metal/GPU support and Windows CPU support
- Binvox, OBJ, STL, and 3MF export
- Print-scale settings for different 3D printers, slicer integration, and export-history management

## Roadmap

- SKP input support
- Support for exporting to more slicers

## Running on macOS

> **The macOS Release is not signed. After downloading it, macOS may report that the application cannot be opened. For a Release obtained from this repository and verified as trusted, remove its quarantine attribute manually before the first launch.**

Move the application into `/Applications`, open Terminal, and run:

```bash
xattr -dr com.apple.quarantine "/Applications/Hands3DLab-VoxKit.app"
```

Then open the application normally. If it is stored elsewhere, replace the path in the command with the actual `.app` path. The `-r` option applies the change to the complete application bundle.

Removing `com.apple.quarantine` bypasses a macOS download-protection check; it does not sign or notarize the application. Only run this command for a Release whose source and integrity you trust. Do not use it indiscriminately on applications from unknown sources.

## Running on Windows

1. Download the Windows x64 ZIP from [GitHub Releases](https://github.com/Hands3DLab/Hands3DLab-VoxKit/releases).
2. Extract the **entire ZIP archive** to a normal local folder. Do not run the application from inside the ZIP.
3. Start `Hands3DLab-VoxKit.exe` from the extracted folder. Do not move the EXE out of that folder.

### Windows usage notes

- **“Voxelization engine not found”**: extract the archive again and check that a `resources` folder exists beside `Hands3DLab-VoxKit.exe`, with `resources/voxkit.exe` inside it. Do not download a separate engine; a missing file usually means the archive was only partially extracted, files were quarantined, or the application files were moved.
- **Windows Defender or SmartScreen warning**: release packages are currently unsigned. Verify that the package came from the trusted GitHub repository and compare a published checksum when available. Only after verifying the source should you use Windows **More info → Run anyway**. Do not disable Defender or SmartScreen globally.
- **Missing DLL or UCRT/runtime error**: install the current Microsoft Visual C++ Redistributable for x64 from Microsoft if Windows reports a missing system runtime, then restart the client. Compatibility can vary by Windows version; this is not a reason to download a separate `voxkit` binary.
- **Slicer or printer does not open**: install Snapmaker Orca/OrcaSlicer if required and keep it in a standard installation location. If the client cannot find it, export the model first and open the exported file manually in your slicer.
- **Conversion mode limitation**: the Windows build is CPU-only and supports Pixel and Quad modes. Triangle mode requires the macOS Metal/GPU path and is unavailable on Windows.

The Windows package structure and PE/ZIP integrity are checked during packaging. A complete real-machine launch, SmartScreen behavior, and slicer integration still depend on the specific Windows system and should be verified on the target machine.

## Download and build

Download the latest complete Electron client for macOS or Windows from [GitHub Releases](https://github.com/Hands3DLab/Hands3DLab-VoxKit/releases). There is no separate native-kernel download.

### Build prerequisites

Building from source requires:

- macOS or Windows with a C++17-compatible compiler
- [CMake](https://cmake.org/) 3.20 or newer
- [Node.js](https://nodejs.org/) and npm
- Python 3, used by the CTest integration tests
- Boost headers, including `boost/json/src.hpp`, required for GLB parsing
- On macOS, Metal and Foundation frameworks are used automatically when GPU support is enabled

### Build the native voxelization engine

From the repository root, configure and build a Release version:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_GPU=ON
cmake --build build --parallel 2
```

This produces the native `voxkit` executable in `build/`. On macOS, `ENABLE_GPU=ON` enables the Metal backend when the required Apple frameworks are available. To build a CPU-only version, use `-DENABLE_GPU=OFF` instead.

The CMake configuration enables tests by default. Run them with:

```bash
ctest --test-dir build --output-on-failure
```

To disable test integration during configuration, add `-DBUILD_TESTS=OFF`.

The equivalent convenience command from the repository root is:

```bash
npm run build
```

### Build and run the Electron client

Install the Electron dependencies and start the desktop client:

```bash
npm --prefix electron install
npm --prefix electron start
```

To build the native engine through the Electron directory, run:

```bash
npm --prefix electron run build
```

The native engine is bundled as an internal application resource when packaging the complete Electron client. Do not distribute or move `voxkit` as a standalone download.

```text
.
├── CMakeLists.txt
├── LICENSE
├── README.md
├── README.zh-CN.md
└── electron/             # Desktop client
    ├── assets/            # Icons and screenshots
    ├── renderer/          # Desktop UI
    ├── src/               # Electron main process and preload bridge
    └── package.json
```

## Run the desktop client locally

```bash
npm --prefix electron install
npm --prefix electron start
```

Node.js and npm are required to run the client from source.

## License

Licensed under the [Apache License 2.0](LICENSE). See [NOTICE](NOTICE) for attribution information.
