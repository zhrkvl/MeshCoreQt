#pragma once

#include <QDateTime>
#include <QString>
#include <cstdint>

namespace MeshCore {

class Message {
public:
  enum Type {
    CHANNEL_MESSAGE,
    CONTACT_MESSAGE // Direct message (future)
  };

  Type type;
  uint8_t channelIdx;          // For channel messages
  QByteArray senderPubKeyPrefix; // For contact messages (6-byte prefix)
  QString senderName;          // Parsed from text (format: "SenderName: msg")
  QString text;                // Message text
  uint32_t timestamp;          // Unix epoch seconds
  uint8_t pathLen;             // 0xFF = direct, else hop count
  uint8_t pathLength;          // Alias for pathLen (more readable)
  uint8_t txtType;             // TXT_TYPE_PLAIN, TXT_TYPE_CLI_DATA, etc.
  float snr;                   // Signal-to-noise ratio (dB)
  QDateTime receivedAt;        // Local receive time

  Message();

  static Message fromChannelRecv(uint8_t channelIdx, const QString &fullText,
                                 uint32_t timestamp, uint8_t pathLen,
                                 float snr = 0.0f);

private:
  // Parse "SenderName: message text" format
  static void parseSenderAndText(const QString &fullText, QString &outSender,
                                 QString &outText);
};

} // namespace MeshCore
