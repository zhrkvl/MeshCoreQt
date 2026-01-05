# MeshCore Qt Client

A C++ Qt application for MeshCore LoRa-based mesh network communication over USB serial.

## Features

- USB serial communication with MeshCore devices
- Group channel messaging
- Device initialization and configuration
- Send and receive messages through channels
- Command-line interface

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

- **Transport Layer**: SerialConnection with frame parsing state machine
- **Protocol Layer**: CommandBuilder/ResponseParser for binary encoding
- **Application Layer**: MeshClient orchestrator with Qt signals/slots

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
