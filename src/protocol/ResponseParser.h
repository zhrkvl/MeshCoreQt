#pragma once

#include <QByteArray>
#include <QString>

#include "../core/DeviceInfo.h"
#include "../models/Channel.h"
#include "../models/Message.h"
#include "ProtocolConstants.h"

namespace MeshCore {

class ResponseParser {
public:
  // Parse response frames
  static DeviceInfo parseDeviceInfo(const QByteArray &frame);
  static SelfInfo parseSelfInfo(const QByteArray &frame);
  static Channel parseChannelInfo(const QByteArray &frame);
  static Message parseChannelMsgRecvV3(const QByteArray &frame);
  static Message parseContactMsgRecvV3(const QByteArray &frame);

  // Get response code from frame
  static ResponseCode getResponseCode(const QByteArray &frame);

  // Check if frame is a push notification (code >= 0x80)
  static bool isPushNotification(const QByteArray &frame);
  static PushCode getPushCode(const QByteArray &frame);

  // Error handling
  static ErrorCode getErrorCode(const QByteArray &frame);

private:
  // Helper functions for little-endian decoding
  static uint32_t readUint32LE(const QByteArray &buf, int offset);
  static uint16_t readUint16LE(const QByteArray &buf, int offset);
  static int32_t readInt32LE(const QByteArray &buf, int offset);
  static uint8_t readUint8(const QByteArray &buf, int offset);

  // Read null-terminated string
  static QString readString(const QByteArray &buf, int offset, int maxLen = -1);
};

} // namespace MeshCore
