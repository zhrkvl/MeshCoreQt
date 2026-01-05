#include "CommandBuilder.h"

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

} // namespace MeshCore
