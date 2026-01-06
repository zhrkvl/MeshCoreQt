#include "Contact.h"

namespace MeshCore {

Contact::Contact()
    : m_type(0), m_flags(0), m_pathLength(-1), m_lastAdvertTimestamp(0),
      m_lastModified(0), m_latitude(0), m_longitude(0) {}

Contact::Contact(const QByteArray &publicKey, const QString &name, uint8_t type)
    : m_publicKey(publicKey), m_name(name), m_type(type), m_flags(0),
      m_pathLength(-1), m_lastAdvertTimestamp(0), m_lastModified(0),
      m_latitude(0), m_longitude(0) {}

void Contact::setName(const QString &name) {
  m_name = name.left(32); // Limit to 32 chars
}

void Contact::setType(uint8_t type) { m_type = type; }

void Contact::setFlags(uint8_t flags) { m_flags = flags; }

void Contact::setPath(const QByteArray &path, int8_t pathLength) {
  m_path = path;
  m_pathLength = pathLength;
}

void Contact::setLastAdvertTimestamp(uint32_t timestamp) {
  m_lastAdvertTimestamp = timestamp;
}

void Contact::setLastModified(uint32_t timestamp) {
  m_lastModified = timestamp;
}

void Contact::setLocation(int32_t latitude, int32_t longitude) {
  m_latitude = latitude;
  m_longitude = longitude;
}

bool Contact::isValid() const {
  return m_publicKey.size() == 32 && !m_name.isEmpty();
}

QString Contact::publicKeyHex() const {
  return QString::fromLatin1(m_publicKey.toHex());
}

bool Contact::operator==(const Contact &other) const {
  return m_publicKey == other.m_publicKey;
}

} // namespace MeshCore
