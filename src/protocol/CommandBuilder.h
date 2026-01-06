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

  // Messaging operations
  static QByteArray buildSendTxtMsg(uint8_t txtType, uint8_t attempt,
                                    uint32_t timestamp,
                                    const QByteArray &recipientPubKeyPrefix,
                                    const QString &text);

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

  // Contact operations
  static QByteArray buildAddUpdateContact(const QByteArray &publicKey,
                                          const QString &name, uint8_t type,
                                          uint8_t flags, int8_t pathLength,
                                          const QByteArray &path,
                                          int32_t latitude, int32_t longitude,
                                          uint32_t lastAdvertTimestamp);
  static QByteArray buildRemoveContact(const QByteArray &publicKey);
  static QByteArray buildGetContactByKey(const QByteArray &publicKey);

private:
  // Helper functions for little-endian encoding
  static void writeUint32LE(QByteArray &buf, uint32_t value);
  static void writeUint16LE(QByteArray &buf, uint16_t value);
  static void writeUint8(QByteArray &buf, uint8_t value);
};

} // namespace MeshCore
