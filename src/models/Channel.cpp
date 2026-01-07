#include "Channel.h"
#include "../protocol/ProtocolConstants.h"
#include <QByteArray>

namespace MeshCore {

Channel::Channel() : index(0), isValid(false) {}

Channel::Channel(uint8_t idx, const QString &channelName,
                 const QByteArray &channelSecret)
    : index(idx), name(channelName), secret(channelSecret), isValid(true) {}

Channel Channel::createPublicChannel() {
  // Decode base64 PSK
  QByteArray pskDecoded = QByteArray::fromBase64(PUBLIC_GROUP_PSK);
  return Channel(0, "Public", pskDecoded);
}

bool Channel::isEmpty() const {
  // Check for blank name
  QString trimmedName = name.trimmed();
  if (trimmedName.isEmpty()) {
    return true;
  }

  // Check for all-zeros secret (uninitialized)
  bool hasNonZero = false;
  for (int i = 0; i < secret.size(); ++i) {
    if (secret[i] != 0x00) {
      hasNonZero = true;
      break;
    }
  }

  return !hasNonZero; // Empty if all zeros
}

bool Channel::isValidChannel() const { return !isEmpty(); }

bool Channel::operator==(const Channel &other) const {
  return index == other.index && name == other.name && secret == other.secret &&
         isValid == other.isValid;
}

} // namespace MeshCore
