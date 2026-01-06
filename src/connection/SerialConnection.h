#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

#include "../protocol/ProtocolConstants.h"
#include "ConnectionState.h"
#include "IConnection.h"

namespace MeshCore {

// Serial port information for discovery
struct SerialPortInfo {
  QString portName;     // e.g., "/dev/ttyUSB0", "COM3"
  QString description;  // e.g., "USB Serial Port"
  QString manufacturer; // e.g., "FTDI", "Silicon Labs"
  QString serialNumber; // Device serial number
  uint16_t vendorId;    // USB Vendor ID (0 if unavailable)
  uint16_t productId;   // USB Product ID (0 if unavailable)
  bool isValid;

  SerialPortInfo() : vendorId(0), productId(0), isValid(false) {}

  // Human-readable VID:PID string
  QString usbIdString() const {
    if (vendorId == 0)
      return QString();
    return QString("0x%1:0x%2")
        .arg(vendorId, 4, 16, QChar('0'))
        .arg(productId, 4, 16, QChar('0'));
  }
};

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

  // Static port enumeration - doesn't require connection
  static QList<SerialPortInfo> enumeratePorts();

  // Heuristic to identify likely MeshCore devices
  static bool isMeshCoreDevice(const SerialPortInfo &info);

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
