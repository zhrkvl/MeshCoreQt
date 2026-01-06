#pragma once

#include "ChannelManager.h"
#include "DeviceInfo.h"
#include "RadioPresets.h"
#include <QObject>
#include <QString>
#include <QVector>

#include "../connection/BLEConnection.h"
#include "../connection/ConnectionState.h"
#include "../connection/IConnection.h"
#include "../connection/SerialConnection.h"
#include "../models/Channel.h"
#include "../models/Contact.h"
#include "../models/Message.h"

namespace MeshCore {

class MeshClient : public QObject {
  Q_OBJECT

public:
  explicit MeshClient(QObject *parent = nullptr);
  explicit MeshClient(IConnection *connection, QObject *parent = nullptr);
  ~MeshClient();

  // Connection management
  bool connectToDevice(const QString &target);
  bool connectToSerialDevice(const QString &portName, int baudRate = 115200);
  bool connectToBLEDevice(const QString &deviceName);
  void disconnect();
  bool isConnected() const;
  IConnection *connection() const { return m_connection; }

  // Initialization sequence
  void startInitSequence();

  // Channel operations
  QVector<Channel> getChannels() const;
  void discoverChannels();
  void joinChannel(const QString &name, const QString &pskHex);

  // Contact operations
  QVector<Contact> getContacts() const;
  void addOrUpdateContact(const Contact &contact);
  void removeContact(const QByteArray &publicKey);
  void requestContactByKey(const QByteArray &publicKey);

  // Messaging
  void sendChannelMessage(uint8_t channelIdx, const QString &text);
  void sendDirectMessage(const QByteArray &recipientPubKey, const QString &text);
  void sendDirectMessage(const Contact &recipient, const QString &text);
  void syncNextMessage();

  // Radio configuration
  void setRadioConfig(const RadioConfig &config);
  void setRadioPreset(const QString &presetName);

  // Device discovery
  void scanBLEDevices(bool filterMeshCoreOnly = true);
  void scanSerialPorts();
  QList<BLEDeviceInfo> getDiscoveredBLEDevices() const;
  QList<SerialPortInfo> getAvailableSerialPorts() const;

  // State
  bool isInitialized() const { return m_initialized; }
  DeviceInfo getDeviceInfo() const { return m_deviceInfo; }
  SelfInfo getSelfInfo() const { return m_selfInfo; }

signals:
  void connected();
  void disconnected();
  void initializationComplete();
  void errorOccurred(const QString &error);

  // Discovery signals
  void bleDeviceFound(const BLEDeviceInfo &device);
  void bleDiscoveryFinished();

  // Channel signals
  void channelListUpdated();
  void channelDiscovered(const Channel &channel);

  // Contact signals
  void contactReceived(const Contact &contact);
  void contactRemoved(const QByteArray &publicKey);
  void contactsUpdated();

  // Message signals
  void channelMessageReceived(const Message &message);
  void contactMessageReceived(const Message &message);
  void messageSent(uint8_t channelIdx);
  void newMessageWaiting();
  void noMoreMessages();

  // Radio configuration signals
  void radioConfigured(const RadioConfig &config);

private slots:
  void onFrameReceived(const QByteArray &frame);
  void onConnectionStateChanged(ConnectionState state);
  void onSerialError(const QString &error);

private:
  enum InitState {
    NOT_STARTED,
    SENT_DEVICE_QUERY,
    SENT_APP_START,
    SENT_GET_CONTACTS,
    COMPLETE
  };

  void handleResponse(const QByteArray &frame);
  void handlePushNotification(const QByteArray &frame);
  void processInitSequence();
  void sendNextInitCommand();

  // Channel discovery
  void requestNextChannel();

  IConnection *m_connection;
  bool m_ownsConnection;
  ChannelManager *m_channelManager;

  bool m_initialized;
  InitState m_initState;
  DeviceInfo m_deviceInfo;
  SelfInfo m_selfInfo;

  // Channel discovery state
  bool m_isDiscoveringChannels;
  uint8_t m_nextChannelIdx;

  // Contact storage
  QVector<Contact> m_contacts;

  // Device discovery results
  QList<SerialPortInfo> m_serialPorts;
  QList<BLEDeviceInfo> m_bleDevices;
};

} // namespace MeshCore
