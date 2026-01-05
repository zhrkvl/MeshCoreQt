#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>

#include "../protocol/ProtocolConstants.h"
#include "ConnectionState.h"

namespace MeshCore {
class SerialConnection : public QObject {
  Q_OBJECT

public:
  explicit SerialConnection(QObject *parent = nullptr);

  ~SerialConnection();

  bool open(const QString &portName, int baudRate = 115200);

  void close();

  bool isOpen() const;

  bool sendFrame(const QByteArray &data);

  ConnectionState state() const { return m_state; }

signals:
  void frameReceived(const QByteArray &frame);

  void stateChanged(ConnectionState state);

  void errorOccurred(const QString &error);

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
