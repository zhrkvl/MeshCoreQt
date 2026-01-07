#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace MeshCore {

class Channel {
public:
  uint8_t index;     // 0 = public, 1+ = custom
  QString name;      // e.g., "Public", "Team Alpha"
  QByteArray secret; // Base64-decoded PSK (16 or 32 bytes typically)
  bool isValid;

  Channel();
  Channel(uint8_t idx, const QString &channelName,
          const QByteArray &channelSecret);

  // Create the default public channel (index 0)
  static Channel createPublicChannel();

  // Validation
  bool isEmpty() const;
  bool isValidChannel() const;

  bool operator==(const Channel &other) const;
};

} // namespace MeshCore
