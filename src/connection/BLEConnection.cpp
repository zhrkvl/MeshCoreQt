#include "BLEConnection.h"
#include <QDebug>

namespace MeshCore {

// Nordic UART Service (NUS) UUIDs - standard for BLE UART services
const QBluetoothUuid BLEConnection::SERVICE_UUID =
    QBluetoothUuid(QStringLiteral("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"));
const QBluetoothUuid BLEConnection::RX_CHARACTERISTIC_UUID =
    QBluetoothUuid(QStringLiteral("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"));
const QBluetoothUuid BLEConnection::TX_CHARACTERISTIC_UUID =
    QBluetoothUuid(QStringLiteral("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"));

BLEConnection::BLEConnection(QObject *parent)
    : IConnection(parent), m_discoveryAgent(nullptr), m_controller(nullptr),
      m_service(nullptr), m_state(ConnectionState::Disconnected),
      m_filterMeshCoreOnly(false) {

  m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
  m_discoveryAgent->setLowEnergyDiscoveryTimeout(5000);

  connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
          this, &BLEConnection::onDeviceDiscovered);
  connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this,
          &BLEConnection::onDiscoveryFinished);
  connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
          this, &BLEConnection::onDiscoveryError);
}

BLEConnection::~BLEConnection() { close(); }

bool BLEConnection::open(const QString &target) {
  if (m_controller && m_controller->state() !=
                          QLowEnergyController::UnconnectedState) {
    qWarning() << "BLE already connected or connecting";
    return false;
  }

  m_targetDeviceName = target;
  qDebug() << "Starting BLE discovery for device:" << target;

  setState(ConnectionState::Connecting);
  startDiscovery();

  return true;
}

void BLEConnection::close() {
  if (m_service) {
    delete m_service;
    m_service = nullptr;
  }

  if (m_controller) {
    m_controller->disconnectFromDevice();
    delete m_controller;
    m_controller = nullptr;
  }

  if (m_discoveryAgent && m_discoveryAgent->isActive()) {
    m_discoveryAgent->stop();
  }

  setState(ConnectionState::Disconnected);
  qDebug() << "BLE connection closed";
}

bool BLEConnection::isOpen() const {
  return m_controller &&
         m_controller->state() == QLowEnergyController::ConnectedState &&
         m_service &&
         m_service->state() == QLowEnergyService::RemoteServiceDiscovered &&
         m_rxCharacteristic.isValid() && m_txCharacteristic.isValid();
}

bool BLEConnection::sendFrame(const QByteArray &data) {
  if (!isOpen()) {
    qWarning() << "Cannot send frame: BLE not connected";
    return false;
  }

  if (data.size() > MAX_FRAME_SIZE) {
    qWarning() << "Frame too large:" << data.size() << "bytes (max"
               << MAX_FRAME_SIZE << ")";
    return false;
  }

  if (!m_rxCharacteristic.isValid()) {
    qWarning() << "RX characteristic not valid";
    return false;
  }

  // For BLE, send the raw frame data without serial framing
  // The companion protocol is the same, but BLE doesn't use '<' + length prefix
  m_service->writeCharacteristic(m_rxCharacteristic, data,
                                  QLowEnergyService::WriteWithoutResponse);

  return true;
}

void BLEConnection::startDiscovery(bool filterMeshCoreOnly) {
  m_filterMeshCoreOnly = filterMeshCoreOnly;
  m_discoveredDevices.clear();
  m_discoveredBLEDevices.clear();
  m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);

  qDebug() << "BLE discovery started"
           << (filterMeshCoreOnly ? "(filtering MeshCore devices only)"
                                  : "(all devices)");
}

void BLEConnection::stopDiscovery() {
  if (m_discoveryAgent->isActive()) {
    m_discoveryAgent->stop();
  }
}

void BLEConnection::onDeviceDiscovered(const QBluetoothDeviceInfo &device) {
  // Check if it's a BLE device
  if (!(device.coreConfigurations() &
        QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
    return;
  }

  // Create enhanced device info
  BLEDeviceInfo info;
  info.name = device.name();
  info.address = device.address().toString();
  info.rssi = device.rssi();
  info.deviceInfo = device;
  info.hasMeshCoreService = hasMeshCoreService(device);

  // Apply filtering if requested
  if (m_filterMeshCoreOnly && !info.hasMeshCoreService) {
    qDebug() << "Skipping non-MeshCore device:" << info.displayName();
    return;
  }

  qDebug() << "BLE device discovered:" << info.displayName() << info.address
           << info.rssiString()
           << (info.hasMeshCoreService ? "[MeshCore]" : "");

  // Store in both lists
  m_discoveredDevices.append(device);
  m_discoveredBLEDevices.append(info);

  // Emit both signals for backward compatibility
  emit deviceDiscovered(device);
  emit bleDeviceDiscovered(info);

  // Auto-connect logic if this is target device (existing code)
  if (!m_targetDeviceName.isEmpty() &&
      (device.name() == m_targetDeviceName ||
       device.address().toString() == m_targetDeviceName)) {
    qDebug() << "Found target device:" << device.name();
    m_targetDevice = device;
    stopDiscovery();

    // Create controller and connect
    if (m_controller) {
      delete m_controller;
    }

    m_controller = QLowEnergyController::createCentral(device, this);

    connect(m_controller, &QLowEnergyController::connected, this,
            &BLEConnection::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected, this,
            &BLEConnection::onControllerDisconnected);
    connect(m_controller, &QLowEnergyController::errorOccurred, this,
            &BLEConnection::onControllerError);
    connect(m_controller, &QLowEnergyController::serviceDiscovered, this,
            &BLEConnection::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished, this,
            &BLEConnection::onServiceDiscoveryFinished);

    m_controller->connectToDevice();
  }
}

void BLEConnection::onDiscoveryFinished() {
  qDebug() << "BLE discovery finished. Found" << m_discoveredDevices.size()
           << "devices";
  emit discoveryFinished();

  if (!m_controller) {
    QString error = QString("Device not found: %1").arg(m_targetDeviceName);
    qWarning() << error;
    setState(ConnectionState::Error);
    emit errorOccurred(error);
  }
}

void BLEConnection::onDiscoveryError(
    QBluetoothDeviceDiscoveryAgent::Error error) {
  QString errorStr =
      QString("BLE discovery error: %1").arg(m_discoveryAgent->errorString());
  qWarning() << errorStr;
  setState(ConnectionState::Error);
  emit errorOccurred(errorStr);
}

void BLEConnection::onControllerConnected() {
  qDebug() << "BLE controller connected, discovering services...";
  m_controller->discoverServices();
}

void BLEConnection::onControllerDisconnected() {
  qDebug() << "BLE controller disconnected";
  setState(ConnectionState::Disconnected);

  if (m_service) {
    delete m_service;
    m_service = nullptr;
  }
}

void BLEConnection::onControllerError(QLowEnergyController::Error error) {
  QString errorStr = QString("BLE controller error: %1 (%2)")
                         .arg(m_controller->errorString())
                         .arg(static_cast<int>(error));
  qWarning() << errorStr;
  setState(ConnectionState::Error);
  emit errorOccurred(errorStr);
}

void BLEConnection::onServiceDiscovered(const QBluetoothUuid &uuid) {
  qDebug() << "Service discovered:" << uuid.toString();

  if (uuid == SERVICE_UUID) {
    qDebug() << "Found MeshCore UART service";
    connectToService();
  }
}

void BLEConnection::onServiceDiscoveryFinished() {
  qDebug() << "Service discovery finished";

  if (!m_service) {
    QString error =
        QString("MeshCore UART service not found on device: %1")
            .arg(m_targetDeviceName);
    qWarning() << error;
    setState(ConnectionState::Error);
    emit errorOccurred(error);
  }
}

void BLEConnection::connectToService() {
  if (m_service) {
    delete m_service;
  }

  m_service = m_controller->createServiceObject(SERVICE_UUID, this);

  if (!m_service) {
    QString error = "Failed to create service object";
    qWarning() << error;
    setState(ConnectionState::Error);
    emit errorOccurred(error);
    return;
  }

  connect(m_service, &QLowEnergyService::stateChanged, this,
          &BLEConnection::onServiceStateChanged);
  connect(m_service, &QLowEnergyService::errorOccurred, this,
          &BLEConnection::onServiceError);
  connect(m_service, &QLowEnergyService::characteristicChanged, this,
          &BLEConnection::onCharacteristicChanged);
  connect(m_service, &QLowEnergyService::characteristicWritten, this,
          &BLEConnection::onCharacteristicWritten);

  m_service->discoverDetails();
}

void BLEConnection::onServiceStateChanged(QLowEnergyService::ServiceState state) {
  qDebug() << "Service state changed:" << static_cast<int>(state);

  if (state == QLowEnergyService::RemoteServiceDiscovered) {
    // Service details discovered, find RX and TX characteristics
    m_rxCharacteristic = m_service->characteristic(RX_CHARACTERISTIC_UUID);
    m_txCharacteristic = m_service->characteristic(TX_CHARACTERISTIC_UUID);

    if (!m_rxCharacteristic.isValid()) {
      QString error = "RX characteristic not found";
      qWarning() << error;
      setState(ConnectionState::Error);
      emit errorOccurred(error);
      return;
    }

    if (!m_txCharacteristic.isValid()) {
      QString error = "TX characteristic not found";
      qWarning() << error;
      setState(ConnectionState::Error);
      emit errorOccurred(error);
      return;
    }

    qDebug() << "RX and TX characteristics found";

    // Enable notifications on TX characteristic (device -> app)
    QLowEnergyDescriptor notification =
        m_txCharacteristic.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);

    if (notification.isValid()) {
      m_service->writeDescriptor(notification,
                                  QByteArray::fromHex("0100")); // Enable notify
      qDebug() << "Enabled notifications on TX characteristic";
    } else {
      qWarning() << "TX notification descriptor not found";
    }

    setState(ConnectionState::Connected);
    qDebug() << "BLE connection established successfully";
  }
}

void BLEConnection::onServiceError(QLowEnergyService::ServiceError error) {
  QString errorStr = QString("BLE service error: %1")
                         .arg(static_cast<int>(error));
  qWarning() << errorStr;
  setState(ConnectionState::Error);
  emit errorOccurred(errorStr);
}

void BLEConnection::onCharacteristicChanged(
    const QLowEnergyCharacteristic &characteristic,
    const QByteArray &newValue) {
  if (characteristic.uuid() == TX_CHARACTERISTIC_UUID) {
    // Received data from device - emit as frame
    // For BLE, the frame is the raw data without serial framing
    emit frameReceived(newValue);
  }
}

void BLEConnection::onCharacteristicWritten(
    const QLowEnergyCharacteristic &characteristic,
    const QByteArray &newValue) {
  // Write confirmed (if needed for debugging)
  Q_UNUSED(characteristic);
  Q_UNUSED(newValue);
}

void BLEConnection::setState(ConnectionState newState) {
  if (m_state != newState) {
    m_state = newState;
    emit stateChanged(newState);
  }
}

bool BLEConnection::hasMeshCoreService(const QBluetoothDeviceInfo &device) {
  // Check if device advertises Nordic UART Service
  QList<QBluetoothUuid> serviceUuids = device.serviceUuids();

  for (const QBluetoothUuid &uuid : serviceUuids) {
    if (uuid == SERVICE_UUID) {
      return true;
    }
  }

  // Note: Many BLE devices don't advertise all service UUIDs during discovery
  // They're only visible after connecting. This is best-effort filtering.
  // User can still connect to any device to check for the service.
  return false;
}

} // namespace MeshCore
