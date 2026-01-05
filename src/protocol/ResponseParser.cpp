#include "ResponseParser.h"
#include <QDebug>

namespace MeshCore {

// Helper functions
uint32_t ResponseParser::readUint32LE(const QByteArray &buf, int offset) {
  if (offset + 4 > buf.size())
    return 0;
  return static_cast<uint32_t>(static_cast<uint8_t>(buf[offset])) |
         (static_cast<uint32_t>(static_cast<uint8_t>(buf[offset + 1])) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(buf[offset + 2])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(buf[offset + 3])) << 24);
}

uint16_t ResponseParser::readUint16LE(const QByteArray &buf, int offset) {
  if (offset + 2 > buf.size())
    return 0;
  return static_cast<uint16_t>(static_cast<uint8_t>(buf[offset])) |
         (static_cast<uint16_t>(static_cast<uint8_t>(buf[offset + 1])) << 8);
}

int32_t ResponseParser::readInt32LE(const QByteArray &buf, int offset) {
  return static_cast<int32_t>(readUint32LE(buf, offset));
}

uint8_t ResponseParser::readUint8(const QByteArray &buf, int offset) {
  if (offset >= buf.size())
    return 0;
  return static_cast<uint8_t>(buf[offset]);
}

QString ResponseParser::readString(const QByteArray &buf, int offset,
                                   int maxLen) {
  if (offset >= buf.size())
    return QString();

  int len = 0;
  int limit = maxLen > 0 ? qMin(offset + maxLen, buf.size()) : buf.size();

  // Find null terminator or max length
  while (offset + len < limit && buf[offset + len] != '\0') {
    len++;
  }

  return QString::fromUtf8(buf.mid(offset, len));
}

// Response code helpers
ResponseCode ResponseParser::getResponseCode(const QByteArray &frame) {
  if (frame.isEmpty())
    return ResponseCode::ERR;
  uint8_t code = readUint8(frame, 0);
  return static_cast<ResponseCode>(code);
}

bool ResponseParser::isPushNotification(const QByteArray &frame) {
  if (frame.isEmpty())
    return false;
  return readUint8(frame, 0) >= 0x80;
}

PushCode ResponseParser::getPushCode(const QByteArray &frame) {
  if (frame.isEmpty())
    return PushCode::ADVERT;
  return static_cast<PushCode>(readUint8(frame, 0));
}

ErrorCode ResponseParser::getErrorCode(const QByteArray &frame) {
  if (frame.size() < 2)
    return ErrorCode::UNSUPPORTED_CMD;
  return static_cast<ErrorCode>(readUint8(frame, 1));
}

// Parse RESP_CODE_DEVICE_INFO
DeviceInfo ResponseParser::parseDeviceInfo(const QByteArray &frame) {
  DeviceInfo info;

  if (frame.size() < 80) {
    qWarning() << "DeviceInfo frame too short:" << frame.size();
    return info;
  }

  // Frame format (from MyMesh.cpp line 828-841):
  // Byte 0: RESP_CODE_DEVICE_INFO (13)
  // Byte 1: FIRMWARE_VER_CODE
  // Byte 2: MAX_CONTACTS / 2
  // Byte 3: MAX_GROUP_CHANNELS
  // Bytes 4-7: BLE PIN (4 bytes)
  // Bytes 8-19: Build date (12 bytes)
  // Bytes 20-59: Manufacturer name (40 bytes)
  // Bytes 60-79: Firmware version string (20 bytes)

  info.firmwareVersion = readUint8(frame, 1);
  info.protocolVersion = 3; // Assume v3 if we got here
  info.firmwareName = readString(frame, 20, 40).trimmed();

  QString firmwareVer = readString(frame, 60, 20).trimmed();
  if (!firmwareVer.isEmpty()) {
    info.firmwareName += " " + firmwareVer;
  }

  return info;
}

// Parse RESP_CODE_SELF_INFO
SelfInfo ResponseParser::parseSelfInfo(const QByteArray &frame) {
  SelfInfo info;

  if (frame.size() < 46) {
    qWarning() << "SelfInfo frame too short:" << frame.size();
    return info;
  }

  // Frame format (from MyMesh.cpp line 851-866):
  // Byte 0: RESP_CODE_SELF_INFO (5)
  // Byte 1: ADV_TYPE (contact type)
  // Byte 2: TX power (dBm)
  // Byte 3: MAX_LORA_TX_POWER
  // Bytes 4-35: Public key (32 bytes)
  // Bytes 36-39: Latitude (int32 LE, * 1E6)
  // Bytes 40-43: Longitude (int32 LE, * 1E6)
  // Byte 44: multi_acks (optional)
  // Byte 45: advert_loc_policy (optional)

  info.contactType = readUint8(frame, 1);
  info.publicKey = frame.mid(4, 32);

  // Node name is not in SELF_INFO frame, will be set elsewhere
  info.nodeName = "Node";

  return info;
}

// Parse RESP_CODE_CHANNEL_INFO
Channel ResponseParser::parseChannelInfo(const QByteArray &frame) {
  if (frame.size() < 50) {
    qWarning() << "ChannelInfo frame too short:" << frame.size();
    return Channel();
  }

  // Frame format (from MyMesh.cpp line 1406-1412):
  // Byte 0: RESP_CODE_CHANNEL_INFO (18)
  // Byte 1: channel_idx
  // Bytes 2-33: Channel name (32 bytes, null-terminated)
  // Bytes 34-49: Secret (16 bytes for 128-bit)

  uint8_t idx = readUint8(frame, 1);
  QString name = readString(frame, 2, 32);
  QByteArray secret = frame.mid(34, 16);

  return Channel(idx, name, secret);
}

// Parse RESP_CODE_CHANNEL_MSG_RECV_V3
Message ResponseParser::parseChannelMsgRecvV3(const QByteArray &frame) {
  if (frame.size() < 12) {
    qWarning() << "ChannelMsgRecvV3 frame too short:" << frame.size();
    return Message();
  }

  // Frame format (from MyMesh.cpp line 444-459):
  // Byte 0: RESP_CODE_CHANNEL_MSG_RECV_V3 (17)
  // Byte 1: SNR * 4 (int8)
  // Bytes 2-3: reserved (0x00)
  // Byte 4: channel_idx
  // Byte 5: path_len (0xFF if direct, else hop count)
  // Byte 6: txt_type
  // Bytes 7-10: timestamp (uint32 LE)
  // Bytes 11+: text (null-terminated, format: "SenderName: message")

  int8_t snrRaw = static_cast<int8_t>(frame[1]);
  float snr = snrRaw / 4.0f;

  uint8_t channelIdx = readUint8(frame, 4);
  uint8_t pathLen = readUint8(frame, 5);
  uint32_t timestamp = readUint32LE(frame, 7);
  QString fullText = readString(frame, 11);

  return Message::fromChannelRecv(channelIdx, fullText, timestamp, pathLen,
                                  snr);
}

// Parse RESP_CODE_CONTACT_MSG_RECV_V3 (for future direct messages)
Message ResponseParser::parseContactMsgRecvV3(const QByteArray &frame) {
  // Similar structure to channel messages but with contact-specific fields
  // Not fully implemented yet - placeholder
  Message msg;
  msg.type = Message::CONTACT_MESSAGE;

  if (frame.size() < 12) {
    qWarning() << "ContactMsgRecvV3 frame too short:" << frame.size();
    return msg;
  }

  // Basic parsing - to be expanded when implementing direct messages
  msg.timestamp = readUint32LE(frame, 7);
  msg.text = readString(frame, 11);

  return msg;
}

} // namespace MeshCore
