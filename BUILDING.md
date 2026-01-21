# Building JoyManager

JoyManager is built using CMake and requires Qt 6.

## Prerequisites

- **CMake** 3.16+
- **Qt 6** (Core, Gui, Widgets, Concurrent)
- **C++17 Compiler** (GCC, Clang, or MSVC)
- **BlueZ** (Linux only, for BLE support)

## Build Instructions

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install qt6-base-dev qt6-declarative-dev build-essential cmake libdbus-1-dev

# Build
cmake -S . -B build
cmake --build build
```

### Windows

1.  Install **Visual Studio 2022** and **Qt 6**.
2.  Ensure `cmake` and the Qt `bin` directory are in your PATH.
3.  Open a developer command prompt:

```cmd
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64"
cmake --build build --config Release
```

### macOS

1.  Install **Xcode** and **Qt 6** (via Homebrew or installer).
2.  Build:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt6)"
cmake --build build
```

## Troubleshooting

- **BLE Permissions**: On Linux, ensure your user is in the `bluetooth` group or use `sudo` (not recommended for daily use).
- **Qt Not Found**: Set `-DCMAKE_PREFIX_PATH` to your Qt installation directory if CMake cannot locate it automatically.
