#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace MeshCore {

class Contact {
public:
  Contact();
  Contact(const QByteArray &publicKey, const QString &name, uint8_t type);

  // Getters
  QByteArray publicKey() const { return m_publicKey; }
  QString name() const { return m_name; }
  uint8_t type() const { return m_type; }
  uint8_t flags() const { return m_flags; }
  int8_t pathLength() const { return m_pathLength; }
  QByteArray path() const { return m_path; }
  uint32_t lastAdvertTimestamp() const { return m_lastAdvertTimestamp; }
  uint32_t lastModified() const { return m_lastModified; }
  int32_t latitude() const { return m_latitude; }
  int32_t longitude() const { return m_longitude; }

  // Setters
  void setName(const QString &name);
  void setType(uint8_t type);
  void setFlags(uint8_t flags);
  void setPath(const QByteArray &path, int8_t pathLength);
  void setLastAdvertTimestamp(uint32_t timestamp);
  void setLastModified(uint32_t timestamp);
  void setLocation(int32_t latitude, int32_t longitude);

  // Utility methods
  bool isValid() const;
  QString publicKeyHex() const;

  // Equality comparison (by public key)
  bool operator==(const Contact &other) const;

private:
  QByteArray m_publicKey;           // 32 bytes
  QString m_name;                   // Max 32 chars
  uint8_t m_type;                   // Contact type (NONE, CHAT, REPEATER, ROOM)
  uint8_t m_flags;                  // Contact flags
  int8_t m_pathLength;              // -1 = flood, 0xFF = direct
  QByteArray m_path;                // Max 64 bytes
  uint32_t m_lastAdvertTimestamp;   // By their clock
  uint32_t m_lastModified;          // By our clock
  int32_t m_latitude;               // Latitude * 1E6
  int32_t m_longitude;              // Longitude * 1E6
};

} // namespace MeshCore
