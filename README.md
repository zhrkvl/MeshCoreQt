# MeshCore Qt Client

A C++ Qt application for MeshCore LoRa-based mesh network communication.

## Features

- USB serial and Bluetooth Low Energy (BLE) communication with MeshCore devices
- Group channel messaging
- Device initialization and configuration
- Send and receive messages through channels
- Command-line interface
- Transport abstraction layer supporting multiple connection types

## Requirements

- Qt 6.x
- C++17 compatible compiler
- macOS (tested), Linux (should work), or Windows (untested)

## Building

### Using Homebrew Qt (macOS - Recommended)

```bash
# Install Qt6 (if not already installed)
brew install qt@6

# Configure CMake
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build .

# Run
./MeshCoreQt
```

## Usage

### Connecting to a Device

The application supports both USB serial and Bluetooth Low Energy connections:

#### USB Serial Connection
```bash
./MeshCoreQt
connect /dev/ttyUSB0        # Linux
connect /dev/cu.usbserial-* # macOS
connect COM3                # Windows
```

#### Bluetooth Low Energy Connection
```bash
./MeshCoreQt
connect ble:MyMeshDevice        # Connect by device name
connect ble:AA:BB:CC:DD:EE:FF   # Connect by MAC address
```

**Note:** BLE discovery may take a few seconds. The device name is the advertised BLE name of your MeshCore device.

### Basic Commands
```bash
init                              # Initialize device
channels                          # List available channels
send 0 Hello World!               # Send to channel 0 (public)
msg abc123def456 Hi there!        # Send direct message (pubkey hex)
sync                              # Pull next message from queue
status                            # Show connection status
help                              # Show all commands
```

## Protocol

MeshCore Qt implements the [MeshCore Companion Radio Protocol](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol):

- **Baud Rate**: 115200
- **Frame Format**: `<` (0x3c) + 2-byte LE length + data
- **Protocol Version**: 3 (supports SNR data in messages)

### Initialization Sequence

1. `CMD_DEVICE_QUERY` → `RESP_CODE_DEVICE_INFO`
2. `CMD_APP_START` → `RESP_CODE_SELF_INFO`
3. `CMD_GET_CONTACTS` → `RESP_CODE_END_OF_CONTACTS`

### Message Flow

1. Radio receives message → sends `PUSH_CODE_MSG_WAITING`
2. App sends `CMD_SYNC_NEXT_MESSAGE`
3. Radio responds with `RESP_CODE_CHANNEL_MSG_RECV_V3`
4. Repeat until `RESP_CODE_NO_MORE_MESSAGES`

## Development

### Architecture

- **Transport Layer**:
  - `IConnection` interface for transport abstraction
  - `SerialConnection` for USB serial communication (frame parsing state machine)
  - `BLEConnection` for Bluetooth Low Energy communication (Nordic UART Service)
- **Protocol Layer**: CommandBuilder/ResponseParser for binary encoding
- **Application Layer**: MeshClient orchestrator with Qt signals/slots

### Connection Types

#### Serial (USB)
- Uses QSerialPort
- Frame format: `<` (0x3c) + 2-byte LE length + data
- Default baud rate: 115200

#### Bluetooth Low Energy (BLE)
- Uses Qt Bluetooth with Nordic UART Service (NUS)
- Service UUID: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- RX Characteristic: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` (write to device)
- TX Characteristic: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` (notifications from device)
- Frame format: Raw data (length implicit in BLE characteristic)

## References

- [MeshCore Repository](https://github.com/meshcore-dev/MeshCore)
- [MeshCore Wiki](https://github.com/meshcore-dev/MeshCore/wiki/)
- [JavaScript Implementation](https://github.com/meshcore-dev/meshcore.js)
- [Python CLI](https://github.com/meshcore-dev/meshcore-cli)

### Serial Port Permission Denied

```bash
# Linux - add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in

# macOS - usually no permission issues
```
