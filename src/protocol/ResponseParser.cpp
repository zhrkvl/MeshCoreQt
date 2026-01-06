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

// Parse RESP_CODE_CONTACT_MSG_RECV_V3
Message ResponseParser::parseContactMsgRecvV3(const QByteArray &frame) {
  Message msg;
  msg.type = Message::CONTACT_MESSAGE;

  if (frame.size() < 16) {
    qWarning() << "ContactMsgRecvV3 frame too short:" << frame.size();
    return msg;
  }

  // Frame format (from MyMesh.cpp queueMessage):
  // Byte 0: RESP_CODE_CONTACT_MSG_RECV_V3 (16)
  // Byte 1: SNR * 4 (int8_t)
  // Byte 2-3: reserved
  // Bytes 4-9: sender public key prefix (6 bytes)
  // Byte 10: path_len (0xFF if direct/no route info)
  // Byte 11: txt_type
  // Bytes 12-15: sender_timestamp (4 bytes LE)
  // Bytes 16+: text (null-terminated string)

  int8_t snrScaled = static_cast<int8_t>(frame[1]);
  msg.snr = snrScaled / 4.0f;

  // Extract sender public key prefix
  msg.senderPubKeyPrefix = frame.mid(4, 6);

  int8_t pathLen = static_cast<int8_t>(frame[10]);
  msg.pathLength = (pathLen == -1) ? 0 : pathLen; // 0xFF means direct/no route

  msg.txtType = static_cast<uint8_t>(frame[11]);
  msg.timestamp = readUint32LE(frame, 12);

  // Text starts at byte 16 (no extra data for now)
  msg.text = readString(frame, 16);

  return msg;
}

// Parse RESP_CODE_CONTACT
Contact ResponseParser::parseContact(const QByteArray &frame) {
  Contact contact;

  if (frame.size() < 148) {
    qWarning() << "Contact frame too short:" << frame.size();
    return contact;
  }

  // Frame format (from MyMesh.cpp line 140-161):
  // Byte 0: RESP_CODE_CONTACT (3)
  // Bytes 1-32: Public key (32 bytes)
  // Byte 33: type
  // Byte 34: flags
  // Byte 35: out_path_len
  // Bytes 36-99: out_path (64 bytes)
  // Bytes 100-131: name (32 bytes, null-terminated)
  // Bytes 132-135: last_advert_timestamp (uint32 LE)
  // Bytes 136-139: gps_lat (int32 LE)
  // Bytes 140-143: gps_lon (int32 LE)
  // Bytes 144-147: lastmod (uint32 LE)

  QByteArray publicKey = frame.mid(1, 32);
  uint8_t type = readUint8(frame, 33);
  uint8_t flags = readUint8(frame, 34);
  int8_t pathLength = static_cast<int8_t>(readUint8(frame, 35));
  QByteArray path = frame.mid(36, 64);
  QString name = readString(frame, 100, 32);
  uint32_t lastAdvertTimestamp = readUint32LE(frame, 132);
  int32_t latitude = readInt32LE(frame, 136);
  int32_t longitude = readInt32LE(frame, 140);
  uint32_t lastModified = readUint32LE(frame, 144);

  contact = Contact(publicKey, name, type);
  contact.setFlags(flags);
  contact.setPath(path, pathLength);
  contact.setLastAdvertTimestamp(lastAdvertTimestamp);
  contact.setLocation(latitude, longitude);
  contact.setLastModified(lastModified);

  return contact;
}

} // namespace MeshCore
