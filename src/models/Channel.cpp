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

bool Channel::operator==(const Channel &other) const {
  return index == other.index && name == other.name && secret == other.secret &&
         isValid == other.isValid;
}

} // namespace MeshCore
