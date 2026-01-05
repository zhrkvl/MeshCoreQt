#pragma once

#include <QByteArray>
#include <QString>

#include "ProtocolConstants.h"

namespace MeshCore {

class CommandBuilder {
public:
  // Init sequence commands
  static QByteArray buildDeviceQuery(uint8_t appTargetVer = PROTOCOL_VERSION);
  static QByteArray buildAppStart(uint8_t appVer, const QString &appName);
  static QByteArray buildGetContacts(uint32_t since = 0);

  // Channel operations
  static QByteArray buildGetChannel(uint8_t channelIdx);
  static QByteArray buildSetChannel(uint8_t channelIdx, const QString &name,
                                    const QByteArray &secret);
  static QByteArray buildSendChannelTxtMsg(uint8_t txtType, uint8_t channelIdx,
                                           uint32_t timestamp,
                                           const QString &text);

  // Message sync
  static QByteArray buildSyncNextMessage();

  // Time operations
  static QByteArray buildGetDeviceTime();
  static QByteArray buildSetDeviceTime(uint32_t epochSecs);

  // Node configuration
  static QByteArray buildSetAdvertName(const QString &name);

  // Radio configuration
  static QByteArray buildSetRadioParams(uint32_t frequencyKhz,
                                        uint32_t bandwidthHz,
                                        uint8_t spreadingFactor,
                                        uint8_t codingRate);
  static QByteArray buildSetRadioTxPower(uint8_t powerDbm);

private:
  // Helper functions for little-endian encoding
  static void writeUint32LE(QByteArray &buf, uint32_t value);
  static void writeUint16LE(QByteArray &buf, uint16_t value);
  static void writeUint8(QByteArray &buf, uint8_t value);
};

} // namespace MeshCore
