#include "CommandBuilder.h"
#include <QDateTime>

namespace MeshCore {

// Helper functions
void CommandBuilder::writeUint32LE(QByteArray &buf, uint32_t value) {
  buf.append(static_cast<char>(value & 0xFF));
  buf.append(static_cast<char>((value >> 8) & 0xFF));
  buf.append(static_cast<char>((value >> 16) & 0xFF));
  buf.append(static_cast<char>((value >> 24) & 0xFF));
}

void CommandBuilder::writeUint16LE(QByteArray &buf, uint16_t value) {
  buf.append(static_cast<char>(value & 0xFF));
  buf.append(static_cast<char>((value >> 8) & 0xFF));
}

void CommandBuilder::writeUint8(QByteArray &buf, uint8_t value) {
  buf.append(static_cast<char>(value));
}

// Init sequence commands
QByteArray CommandBuilder::buildDeviceQuery(uint8_t appTargetVer) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::DEVICE_QUERY));
  writeUint8(frame, appTargetVer);
  return frame;
}

QByteArray CommandBuilder::buildAppStart(uint8_t appVer,
                                         const QString &appName) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::APP_START));
  writeUint8(frame, appVer);

  // Append app name as null-terminated string
  QByteArray nameBytes = appName.toUtf8();
  frame.append(nameBytes);
  frame.append('\0');

  return frame;
}

QByteArray CommandBuilder::buildGetContacts(uint32_t since) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::GET_CONTACTS));
  writeUint32LE(frame, since);
  return frame;
}

// Messaging operations
QByteArray CommandBuilder::buildSendTxtMsg(uint8_t txtType, uint8_t attempt,
                                           uint32_t timestamp,
                                           const QByteArray &recipientPubKeyPrefix,
                                           const QString &text) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SEND_TXT_MSG));
  writeUint8(frame, txtType);
  writeUint8(frame, attempt);
  writeUint32LE(frame, timestamp);

  // Append first 6 bytes of recipient's public key
  frame.append(recipientPubKeyPrefix.left(6));

  // Append text as null-terminated string
  QByteArray textBytes = text.toUtf8();
  frame.append(textBytes);
  frame.append('\0');

  return frame;
}

// Channel operations
QByteArray CommandBuilder::buildGetChannel(uint8_t channelIdx) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::GET_CHANNEL));
  writeUint8(frame, channelIdx);
  return frame;
}

QByteArray CommandBuilder::buildSetChannel(uint8_t channelIdx,
                                           const QString &name,
                                           const QByteArray &secret) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SET_CHANNEL));
  writeUint8(frame, channelIdx);

  // Name (32 bytes, null-padded)
  QByteArray nameBytes = name.toUtf8();
  nameBytes.resize(MAX_NAME_SIZE, '\0');
  frame.append(nameBytes);

  // Secret (variable length, 16 or 32 bytes typically)
  frame.append(secret);

  return frame;
}

QByteArray CommandBuilder::buildSendChannelTxtMsg(uint8_t txtType,
                                                  uint8_t channelIdx,
                                                  uint32_t timestamp,
                                                  const QString &text) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SEND_CHANNEL_TXT_MSG));
  writeUint8(frame, txtType);
  writeUint8(frame, channelIdx);
  writeUint32LE(frame, timestamp);

  // Append text as null-terminated string
  QByteArray textBytes = text.toUtf8();
  frame.append(textBytes);
  frame.append('\0');

  return frame;
}

// Message sync
QByteArray CommandBuilder::buildSyncNextMessage() {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SYNC_NEXT_MESSAGE));
  return frame;
}

// Time operations
QByteArray CommandBuilder::buildGetDeviceTime() {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::GET_DEVICE_TIME));
  return frame;
}

QByteArray CommandBuilder::buildSetDeviceTime(uint32_t epochSecs) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SET_DEVICE_TIME));
  writeUint32LE(frame, epochSecs);
  return frame;
}

// Node configuration
QByteArray CommandBuilder::buildSetAdvertName(const QString &name) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SET_ADVERT_NAME));

  // Append name as null-terminated string
  QByteArray nameBytes = name.toUtf8();
  frame.append(nameBytes);
  frame.append('\0');

  return frame;
}

// Radio configuration
QByteArray CommandBuilder::buildSetRadioParams(uint32_t frequencyKhz,
                                               uint32_t bandwidthHz,
                                               uint8_t spreadingFactor,
                                               uint8_t codingRate) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SET_RADIO_PARAMS));
  writeUint32LE(frame, frequencyKhz);
  writeUint32LE(frame, bandwidthHz);
  writeUint8(frame, spreadingFactor);
  writeUint8(frame, codingRate);

  return frame;
}

QByteArray CommandBuilder::buildSetRadioTxPower(uint8_t powerDbm) {
  QByteArray frame;
  writeUint8(frame, static_cast<uint8_t>(CommandCode::SET_RADIO_TX_POWER));
  writeUint8(frame, powerDbm);

  return frame;
}

// Contact operations
QByteArray
CommandBuilder::buildAddUpdateContact(const QByteArray &publicKey,
                                      const QString &name, uint8_t type,
                                      uint8_t flags, int8_t pathLength,
                                      const QByteArray &path, int32_t latitude,
                                      int32_t longitude,
                                      uint32_t lastAdvertTimestamp) {
  QByteArray cmd;

  // CMD_ADD_UPDATE_CONTACT (9)
  writeUint8(cmd, static_cast<uint8_t>(CommandCode::ADD_UPDATE_CONTACT));

  // Public key (32 bytes)
  cmd.append(publicKey.left(PUB_KEY_SIZE));
  if (publicKey.size() < PUB_KEY_SIZE) {
    cmd.append(QByteArray(PUB_KEY_SIZE - publicKey.size(), 0));
  }

  // Type, flags, path length
  writeUint8(cmd, type);
  writeUint8(cmd, flags);
  writeUint8(cmd, static_cast<uint8_t>(pathLength));

  // Path (64 bytes)
  cmd.append(path.left(MAX_PATH_SIZE));
  if (path.size() < MAX_PATH_SIZE) {
    cmd.append(QByteArray(MAX_PATH_SIZE - path.size(), 0));
  }

  // Name (32 bytes, null-terminated)
  QByteArray nameBytes = name.toUtf8();
  cmd.append(nameBytes.left(MAX_NAME_SIZE - 1));
  if (nameBytes.size() < MAX_NAME_SIZE) {
    cmd.append(QByteArray(MAX_NAME_SIZE - nameBytes.size(), 0));
  }

  // Last advert timestamp
  writeUint32LE(cmd, lastAdvertTimestamp);

  // GPS coordinates
  writeUint32LE(cmd, static_cast<uint32_t>(latitude));
  writeUint32LE(cmd, static_cast<uint32_t>(longitude));

  // Last modified timestamp (current time)
  writeUint32LE(cmd, static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch()));

  return cmd;
}

QByteArray CommandBuilder::buildRemoveContact(const QByteArray &publicKey) {
  QByteArray cmd;

  // CMD_REMOVE_CONTACT (15)
  writeUint8(cmd, static_cast<uint8_t>(CommandCode::REMOVE_CONTACT));

  // Public key (32 bytes)
  cmd.append(publicKey.left(PUB_KEY_SIZE));
  if (publicKey.size() < PUB_KEY_SIZE) {
    cmd.append(QByteArray(PUB_KEY_SIZE - publicKey.size(), 0));
  }

  return cmd;
}

QByteArray CommandBuilder::buildGetContactByKey(const QByteArray &publicKey) {
  QByteArray cmd;

  // CMD_GET_CONTACT_BY_KEY (30)
  writeUint8(cmd, static_cast<uint8_t>(CommandCode::GET_CONTACT_BY_KEY));

  // Public key (32 bytes)
  cmd.append(publicKey.left(PUB_KEY_SIZE));
  if (publicKey.size() < PUB_KEY_SIZE) {
    cmd.append(QByteArray(PUB_KEY_SIZE - publicKey.size(), 0));
  }

  return cmd;
}

} // namespace MeshCore
