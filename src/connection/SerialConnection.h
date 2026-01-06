#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>

#include "../protocol/ProtocolConstants.h"
#include "ConnectionState.h"
#include "IConnection.h"

namespace MeshCore {
class SerialConnection : public IConnection {
  Q_OBJECT

public:
  explicit SerialConnection(QObject *parent = nullptr);

  ~SerialConnection() override;

  // IConnection interface implementation
  bool open(const QString &target) override;
  void close() override;
  bool isOpen() const override;
  bool sendFrame(const QByteArray &data) override;
  ConnectionState state() const override { return m_state; }
  QString connectionType() const override { return QStringLiteral("Serial"); }

  // Serial-specific overload with baud rate
  bool open(const QString &portName, int baudRate);

private slots:
  void onReadyRead();

  void onErrorOccurred(QSerialPort::SerialPortError error);

private:
  void processByte(uint8_t byte);

  void setState(ConnectionState newState);

  enum RecvState {
    IDLE,       // Waiting for '<' (0x3c)
    HDR_FOUND,  // Found '<', reading LSB of length
    LEN1_FOUND, // Read LSB, reading MSB of length
    LEN2_FOUND  // Reading frame payload
  };

  QSerialPort *m_serial;
  ConnectionState m_state;

  // Frame parsing state
  RecvState m_recvState;
  uint16_t m_frameLen;
  QByteArray m_rxBuffer;
};
} // namespace MeshCore
