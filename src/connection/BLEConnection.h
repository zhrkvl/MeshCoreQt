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
  void startDiscovery();
  void stopDiscovery();
  QList<QBluetoothDeviceInfo> getDiscoveredDevices() const {
    return m_discoveredDevices;
  }

signals:
  void deviceDiscovered(const QBluetoothDeviceInfo &device);
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

  // Nordic UART Service (NUS) UUIDs - standard for MeshCore BLE
  static const QBluetoothUuid SERVICE_UUID;
  static const QBluetoothUuid RX_CHARACTERISTIC_UUID;
  static const QBluetoothUuid TX_CHARACTERISTIC_UUID;

  QBluetoothDeviceDiscoveryAgent *m_discoveryAgent;
  QList<QBluetoothDeviceInfo> m_discoveredDevices;

  QLowEnergyController *m_controller;
  QLowEnergyService *m_service;

  QLowEnergyCharacteristic m_rxCharacteristic; // Write to device
  QLowEnergyCharacteristic m_txCharacteristic; // Read from device (notify)

  ConnectionState m_state;
  QString m_targetDeviceName;
  QBluetoothDeviceInfo m_targetDevice;
};

} // namespace MeshCore
