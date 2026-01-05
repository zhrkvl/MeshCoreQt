#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace MeshCore {

struct DeviceInfo {
  uint8_t firmwareVersion;  // Firmware version
  QString firmwareName;     // e.g., "RAK4631", "ESP32"
  uint32_t protocolVersion; // Protocol version supported

  DeviceInfo() : firmwareVersion(0), protocolVersion(0) {}
};

struct SelfInfo {
  QByteArray publicKey; // 32 bytes - node's public key
  QString nodeName;     // Node's advertised name
  uint8_t contactType;  // ContactType enum value
  uint8_t flags;        // Contact flags

  SelfInfo() : contactType(0), flags(0) {}
};

} // namespace MeshCore
