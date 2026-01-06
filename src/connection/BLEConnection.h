#pragma once

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QObject>
#include <QString>
#include <QTimer>

#include "../protocol/ProtocolConstants.h"
#include "ConnectionState.h"
#include "IConnection.h"

namespace MeshCore {

// Enhanced BLE device information
struct BLEDeviceInfo {
  QString name;             // Device name (may be empty)
  QString address;          // MAC address
  int16_t rssi;             // Signal strength in dBm
  bool hasMeshCoreService;  // Has Nordic UART Service UUID
  QBluetoothDeviceInfo deviceInfo; // Full Qt device info for connection

  BLEDeviceInfo() : rssi(-999), hasMeshCoreService(false) {}

  // Human-readable RSSI with signal quality
  QString rssiString() const {
    if (rssi == -999)
      return "N/A";
    QString str = QString::number(rssi) + " dBm";
    if (rssi >= -50)
      str += " (Excellent)";
    else if (rssi >= -70)
      str += " (Good)";
    else if (rssi >= -85)
      str += " (Fair)";
    else
      str += " (Weak)";
    return str;
  }

  // Display name (address if name is empty)
  QString displayName() const { return name.isEmpty() ? address : name; }
};

class BLEConnection : public IConnection {
  Q_OBJECT

public:
  explicit BLEConnection(QObject *parent = nullptr);
  ~BLEConnection() override;

  // IConnection interface implementation
  bool open(const QString &target) override;
  void close() override;
  bool isOpen() const override;
  bool sendFrame(const QByteArray &data) override;
  ConnectionState state() const override { return m_state; }
  QString connectionType() const override { return QStringLiteral("BLE"); }

  // BLE-specific methods
  void startDiscovery(bool filterMeshCoreOnly = false);
  void stopDiscovery();
  QList<QBluetoothDeviceInfo> getDiscoveredDevices() const {
    return m_discoveredDevices;
  }
  QList<BLEDeviceInfo> getDiscoveredBLEDevices() const {
    return m_discoveredBLEDevices;
  }

signals:
  void deviceDiscovered(const QBluetoothDeviceInfo &device);
  void bleDeviceDiscovered(const BLEDeviceInfo &device);
  void discoveryFinished();

private slots:
  void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
  void onDiscoveryFinished();
  void onDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error);

  void onControllerConnected();
  void onControllerDisconnected();
  void onControllerError(QLowEnergyController::Error error);
  void onServiceDiscovered(const QBluetoothUuid &uuid);
  void onServiceDiscoveryFinished();

  void onServiceStateChanged(QLowEnergyService::ServiceState state);
  void onServiceError(QLowEnergyService::ServiceError error);
  void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                const QByteArray &newValue);
  void onCharacteristicWritten(const QLowEnergyCharacteristic &characteristic,
                                const QByteArray &newValue);

private:
  void setState(ConnectionState newState);
  void connectToService();
  bool hasMeshCoreService(const QBluetoothDeviceInfo &device);

  // Nordic UART Service (NUS) UUIDs - standard for MeshCore BLE
  static const QBluetoothUuid SERVICE_UUID;
  static const QBluetoothUuid RX_CHARACTERISTIC_UUID;
  static const QBluetoothUuid TX_CHARACTERISTIC_UUID;

  QBluetoothDeviceDiscoveryAgent *m_discoveryAgent;
  QList<QBluetoothDeviceInfo> m_discoveredDevices;
  QList<BLEDeviceInfo> m_discoveredBLEDevices;
  bool m_filterMeshCoreOnly;

  QLowEnergyController *m_controller;
  QLowEnergyService *m_service;

  QLowEnergyCharacteristic m_rxCharacteristic; // Write to device
  QLowEnergyCharacteristic m_txCharacteristic; // Read from device (notify)

  ConnectionState m_state;
  QString m_targetDeviceName;
  QBluetoothDeviceInfo m_targetDevice;
};

} // namespace MeshCore
