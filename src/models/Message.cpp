#include "Message.h"

namespace MeshCore {

Message::Message()
    : type(CHANNEL_MESSAGE), channelIdx(0), timestamp(0), pathLen(0xFF),
      pathLength(0xFF), txtType(0), snr(0.0f) {}

Message Message::fromChannelRecv(uint8_t channelIdx, const QString &fullText,
                                 uint32_t timestamp, uint8_t pathLen,
                                 float snr) {
  Message msg;
  msg.type = CHANNEL_MESSAGE;
  msg.channelIdx = channelIdx;
  msg.timestamp = timestamp;
  msg.pathLen = pathLen;
  msg.snr = snr;
  msg.receivedAt = QDateTime::currentDateTime();

  // Parse "SenderName: message text" format
  parseSenderAndText(fullText, msg.senderName, msg.text);

  return msg;
}

void Message::parseSenderAndText(const QString &fullText, QString &outSender,
                                 QString &outText) {
  // Channel messages format: "SenderName: message text"
  int colonPos = fullText.indexOf(':');

  if (colonPos > 0 && colonPos < fullText.length() - 1) {
    outSender = fullText.left(colonPos).trimmed();
    outText = fullText.mid(colonPos + 1).trimmed();
  } else {
    // No colon found or invalid format
    outSender = "Unknown";
    outText = fullText;
  }
}

} // namespace MeshCore
