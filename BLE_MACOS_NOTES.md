# BLE Bluetooth on macOS - Known Issues & Workarounds

## Current Status

✅ **Serial USB connections work perfectly**
❌ **BLE scanning currently NOT supported on Qt 6.5+ Homebrew** due to permission plugin limitations

## The Problem

Qt 6.5+ on macOS requires:
1. `Info.plist` with Bluetooth permission descriptions (✅ we have this)
2. Qt's permission plugin to load and request runtime permissions (❌ doesn't load with `QCoreApplication`)

The permission plugin requires `QGuiApplication` or `QApplication`, but Qt 6.8 from Homebrew has a bug where it tries to link the deprecated AGL (Apple Graphics Layer) framework, causing build failures.

## Workarounds

### Option 1: Use Serial Connections Only (Recommended)

The `scan serial` command works perfectly and will show all USB serial devices:

```bash
./MeshCoreQt.app/Contents/MacOS/MeshCoreQt
> scan serial
> connect /dev/cu.usbserial-XXXX
```

This is the most reliable option and supports all features except BLE discovery.

### Option 2: Install Qt from Official Qt Installer

The official Qt binaries don't have the AGL framework bug:

1. Download Qt from https://www.qt.io/download-qt-installer
2. Install Qt 6.8 (or 6.6 LTS)
3. Reconfigure MeshCoreQt to use official Qt:
   ```bash
   cmake -B build -S . -DCMAKE_PREFIX_PATH=~/Qt/6.8.0/macos
   cmake --build build
   ```

### Option 3: Downgrade to Qt 6.4

Qt 6.4 and earlier don't have the strict permission requirements:

```bash
brew uninstall qt@6
brew install qt@5  # Or compile Qt 6.4 from source
```

### Option 4: Use Direct Connect (Skip Scanning)

If you know your device name/address, skip scanning and connect directly:

```bash
> connect ble:MeshCore-XXXX
> connect ble:AA:BB:CC:DD:EE:FF
```

BLE connections work; only discovery/scanning is affected by the permission issue.

### Option 5: Native CoreBluetooth Implementation (Future)

For a permanent fix, we could bypass Qt Bluetooth entirely and use macOS's native CoreBluetooth APIs directly via Objective-C++. This would require significant development effort but would be the most robust solution.

## Technical Details

### Why QCoreApplication?

MeshCoreQt is a CLI application using `QCoreApplication`, which:
- ✅ Minimal resource usage
- ✅ No GUI overhead
- ✅ Perfect for headless/embedded use
- ❌ Doesn't load Qt's permission plugins

### Why Not Switch to QApplication?

Switching to `QApplication` (Qt Widgets) would enable permission plugins but:
- ❌ Qt 6.8 Homebrew has AGL framework linker bug
- ❌ Adds ~10MB of GUI framework overhead
- ❌ Unnecessary dependencies for a CLI tool

### The AGL Framework Issue

```
ld: framework 'AGL' not found
```

AGL (Apple Graphics Layer) was deprecated by Apple in macOS 10.14 and completely removed. Qt 6.8 from Homebrew incorrectly tries to link this framework when using QtWidgets or QtGui.

## Future Plans

- [ ] Add native CoreBluetooth wrapper (Objective-C++)
- [ ] Support both Qt Bluetooth and native CoreBluetooth
- [ ] Automatic fallback to native APIs when Qt permission plugin unavailable
- [ ] Linux/Windows unaffected - continue using Qt Bluetooth

## References

- [Qt Permission API Documentation](https://doc.qt.io/qt-6/permissions.html)
- [macOS Bluetooth Permissions](https://developer.apple.com/documentation/bundleresources/information_property_list/nsbluetoothalwaysusagedescription)
- [Qt AGL Framework Issue](https://bugreports.qt.io/browse/QTBUG-117484)

## Questions?

If you need BLE scanning, please:
1. Try Option 2 (official Qt installer) - most likely to work
2. Open an issue describing your setup if problems persist
3. Consider using serial connections as a reliable alternative
