#include <QDebug>

#include "SerialConnection.h"

namespace MeshCore {
SerialConnection::SerialConnection(QObject *parent)
    : IConnection(parent), m_serial(new QSerialPort(this)),
      m_state(ConnectionState::Disconnected), m_recvState(IDLE), m_frameLen(0) {
  connect(m_serial, &QSerialPort::readyRead, this,
          &SerialConnection::onReadyRead);
  connect(m_serial, &QSerialPort::errorOccurred, this,
          &SerialConnection::onErrorOccurred);
}

SerialConnection::~SerialConnection() { close(); }

bool SerialConnection::open(const QString &target) {
  return open(target, 115200);
}

bool SerialConnection::open(const QString &portName, int baudRate) {
  if (m_serial->isOpen()) {
    qWarning() << "Serial port already open";
    return false;
  }

  m_serial->setPortName(portName);
  m_serial->setBaudRate(baudRate);
  m_serial->setDataBits(QSerialPort::Data8);
  m_serial->setParity(QSerialPort::NoParity);
  m_serial->setStopBits(QSerialPort::OneStop);
  m_serial->setFlowControl(QSerialPort::NoFlowControl);

  setState(ConnectionState::Connecting);

  if (!m_serial->open(QIODevice::ReadWrite)) {
    QString error =
        QString("Failed to open %1: %2").arg(portName, m_serial->errorString());
    qWarning() << error;
    setState(ConnectionState::Error);
    emit errorOccurred(error);
    return false;
  }

  // Flush any stale data from previous connections
  m_serial->clear();

  // Reset frame parser state
  m_recvState = IDLE;
  m_frameLen = 0;
  m_rxBuffer.clear();

  setState(ConnectionState::Connected);
  qDebug() << "Connected to" << portName << "at" << baudRate << "baud";
  return true;
}

void SerialConnection::close() {
  if (m_serial->isOpen()) {
    m_serial->close();
    setState(ConnectionState::Disconnected);
    qDebug() << "Serial port closed";
  }
}

bool SerialConnection::isOpen() const { return m_serial->isOpen(); }

bool SerialConnection::sendFrame(const QByteArray &data) {
  if (!m_serial->isOpen()) {
    qWarning() << "Cannot send frame: serial port not open";
    return false;
  }

  if (data.size() > MAX_FRAME_SIZE) {
    qWarning() << "Frame too large:" << data.size() << "bytes (max"
               << MAX_FRAME_SIZE << ")";
    return false;
  }

  // Build frame: '<' (0x3c) + 2-byte LE length + data
  QByteArray frame;
  frame.reserve(3 + data.size());

  frame.append(static_cast<char>(FRAME_INBOUND));             // '<'
  frame.append(static_cast<char>(data.size() & 0xFF));        // LSB
  frame.append(static_cast<char>((data.size() >> 8) & 0xFF)); // MSB
  frame.append(data);

  qint64 written = m_serial->write(frame);
  if (written != frame.size()) {
    qWarning() << "Failed to write complete frame:" << written << "of"
               << frame.size() << "bytes";
    return false;
  }

  m_serial->flush();
  return true;
}

void SerialConnection::onReadyRead() {
  while (m_serial->bytesAvailable() > 0) {
    char byte;
    if (m_serial->read(&byte, 1) == 1) {
      processByte(static_cast<uint8_t>(byte));
    }
  }
}

void SerialConnection::processByte(uint8_t byte) {
  switch (m_recvState) {
  case IDLE:
    if (byte == FRAME_OUTBOUND) {
      // '>' (0x3e) - outbound frame from radio
      m_recvState = HDR_FOUND;
    }
    break;

  case HDR_FOUND:
    m_frameLen = byte; // LSB
    m_recvState = LEN1_FOUND;
    break;

  case LEN1_FOUND:
    m_frameLen |= (static_cast<uint16_t>(byte) << 8); // MSB
    m_rxBuffer.clear();
    m_recvState = (m_frameLen > 0) ? LEN2_FOUND : IDLE;
    break;

  case LEN2_FOUND:
    if (m_rxBuffer.size() < MAX_FRAME_SIZE) {
      m_rxBuffer.append(static_cast<char>(byte));
    }

    if (m_rxBuffer.size() >= m_frameLen) {
      // Frame complete
      if (m_frameLen > MAX_FRAME_SIZE) {
        qWarning() << "Frame truncated from" << m_frameLen << "to"
                   << MAX_FRAME_SIZE << "bytes";
        m_rxBuffer.resize(MAX_FRAME_SIZE);
      }

      emit frameReceived(m_rxBuffer);
      m_recvState = IDLE;
    }
    break;
  }
}

void SerialConnection::onErrorOccurred(QSerialPort::SerialPortError error) {
  if (error == QSerialPort::NoError) {
    return;
  }

  QString errorStr =
      QString("Serial port error: %1").arg(m_serial->errorString());
  qWarning() << errorStr;

  setState(ConnectionState::Error);
  emit errorOccurred(errorStr);
}

void SerialConnection::setState(ConnectionState newState) {
  if (m_state != newState) {
    m_state = newState;
    emit stateChanged(newState);
  }
}

QList<SerialPortInfo> SerialConnection::enumeratePorts() {
  QList<SerialPortInfo> result;

  // Use Qt's cross-platform serial port enumeration
  const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

  for (const QSerialPortInfo &port : ports) {
    SerialPortInfo info;
    info.portName = port.portName();
    info.description = port.description();
    info.manufacturer = port.manufacturer();
    info.serialNumber = port.serialNumber();
    info.vendorId = port.hasVendorIdentifier() ? port.vendorIdentifier() : 0;
    info.productId =
        port.hasProductIdentifier() ? port.productIdentifier() : 0;
    info.isValid = true;

    result.append(info);
  }

  qDebug() << "Enumerated" << result.size() << "serial port(s)";
  return result;
}

bool SerialConnection::isMeshCoreDevice(const SerialPortInfo &info) {
  // Common USB-Serial chip vendors used in MeshCore and similar devices:
  // - FTDI (FT232, etc.): 0x0403
  // - Silicon Labs (CP210x): 0x10C4
  // - WCH (CH340, CH341): 0x1A86
  // - Prolific (PL2303): 0x067B

  static const QList<uint16_t> knownVendors = {
      0x0403, // FTDI
      0x10C4, // Silicon Labs
      0x1A86, // WCH
      0x067B  // Prolific
  };

  // Check USB Vendor ID
  if (info.vendorId != 0 && knownVendors.contains(info.vendorId)) {
    return true;
  }

  // Fallback: Check description and manufacturer strings
  QString desc = info.description.toLower();
  QString mfg = info.manufacturer.toLower();

  // Look for USB serial keywords
  if (desc.contains("usb") &&
      (desc.contains("serial") || desc.contains("uart"))) {
    return true;
  }

  // Check manufacturer names
  QStringList knownMfgs = {"ftdi", "silicon labs", "ch340", "prolific"};
  for (const QString &known : knownMfgs) {
    if (mfg.contains(known)) {
      return true;
    }
  }

  return false;
}
} // namespace MeshCore
