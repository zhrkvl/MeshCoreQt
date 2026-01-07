#include <QDateTime>
#include <QDebug>

#include "../connection/BLEConnection.h"
#include "../connection/SerialConnection.h"
#include "../protocol/CommandBuilder.h"
#include "../protocol/ResponseParser.h"
#include "../storage/DatabaseManager.h"
#include "../storage/SettingsManager.h"
#include "MeshClient.h"

namespace MeshCore {

MeshClient::MeshClient(QObject *parent)
    : QObject(parent), m_connection(nullptr), m_ownsConnection(true),
      m_channelManager(new ChannelManager(this)), m_initialized(false),
      m_initState(NOT_STARTED), m_isDiscoveringChannels(false),
      m_nextChannelIdx(0), m_databaseManager(new DatabaseManager(this)),
      m_persistenceEnabled(true) {
  // Initialize channel manager with public channel
  m_channelManager->initialize();

  // Connect channel manager signals for persistence
  connect(m_channelManager, &ChannelManager::channelAdded, this,
          [this](const Channel &channel) {
            if (m_persistenceEnabled && m_databaseManager && m_databaseManager->isOpen()) {
              m_databaseManager->saveChannel(channel);
            }
          });
}

MeshClient::MeshClient(IConnection *connection, QObject *parent)
    : QObject(parent), m_connection(connection), m_ownsConnection(false),
      m_channelManager(new ChannelManager(this)), m_initialized(false),
      m_initState(NOT_STARTED), m_isDiscoveringChannels(false),
      m_nextChannelIdx(0), m_databaseManager(new DatabaseManager(this)),
      m_persistenceEnabled(true) {
  // Connect connection signals
  connect(m_connection, &IConnection::frameReceived, this,
          &MeshClient::onFrameReceived);
  connect(m_connection, &IConnection::stateChanged, this,
          &MeshClient::onConnectionStateChanged);
  connect(m_connection, &IConnection::errorOccurred, this,
          &MeshClient::onSerialError);

  // Auto-start initialization when connection is established
  connect(m_connection, &IConnection::stateChanged, this,
          [this](ConnectionState state) {
            if (state == ConnectionState::Connected && !m_initialized) {
              qDebug() << "Connection established, starting auto-init...";
              startInitSequence();
            }
          });

  // Initialize channel manager with public channel
  m_channelManager->initialize();

  // Connect channel manager signals for persistence
  connect(m_channelManager, &ChannelManager::channelAdded, this,
          [this](const Channel &channel) {
            if (m_persistenceEnabled && m_databaseManager && m_databaseManager->isOpen()) {
              m_databaseManager->saveChannel(channel);
            }
          });
}

MeshClient::~MeshClient() {
  disconnect();
  if (m_ownsConnection && m_connection) {
    delete m_connection;
  }
}

bool MeshClient::connectToDevice(const QString &target) {
  if (m_connection && m_connection->isOpen()) {
    qWarning() << "Already connected";
    return false;
  }

  // If no connection exists, try to determine type from target
  // For now, default to serial
  if (!m_connection) {
    m_connection = new SerialConnection(this);
    m_ownsConnection = true;

    connect(m_connection, &IConnection::frameReceived, this,
            &MeshClient::onFrameReceived);
    connect(m_connection, &IConnection::stateChanged, this,
            &MeshClient::onConnectionStateChanged);
    connect(m_connection, &IConnection::errorOccurred, this,
            &MeshClient::onSerialError);
  }

  qDebug() << "Connecting to" << target << "...";
  return m_connection->open(target);
}

bool MeshClient::connectToSerialDevice(const QString &portName, int baudRate) {
  if (m_connection && m_connection->isOpen()) {
    qWarning() << "Already connected";
    return false;
  }

  if (m_ownsConnection && m_connection) {
    delete m_connection;
  }

  m_connection = new SerialConnection(this);
  m_ownsConnection = true;

  connect(m_connection, &IConnection::frameReceived, this,
          &MeshClient::onFrameReceived);
  connect(m_connection, &IConnection::stateChanged, this,
          &MeshClient::onConnectionStateChanged);
  connect(m_connection, &IConnection::errorOccurred, this,
          &MeshClient::onSerialError);

  qDebug() << "Connecting to serial port" << portName << "at" << baudRate
           << "baud...";

  SerialConnection *serialConn = qobject_cast<SerialConnection *>(m_connection);
  if (serialConn) {
    return serialConn->open(portName, baudRate);
  }

  return false;
}

bool MeshClient::connectToBLEDevice(const QString &deviceName) {
  if (m_connection && m_connection->isOpen()) {
    qWarning() << "Already connected";
    return false;
  }

  if (m_ownsConnection && m_connection) {
    delete m_connection;
  }

  m_connection = new BLEConnection(this);
  m_ownsConnection = true;

  connect(m_connection, &IConnection::frameReceived, this,
          &MeshClient::onFrameReceived);
  connect(m_connection, &IConnection::stateChanged, this,
          &MeshClient::onConnectionStateChanged);
  connect(m_connection, &IConnection::errorOccurred, this,
          &MeshClient::onSerialError);

  qDebug() << "Connecting to BLE device:" << deviceName;
  return m_connection->open(deviceName);
}

void MeshClient::disconnect() {
  if (m_connection && m_connection->isOpen()) {
    // Update last connected time and close database
    if (m_persistenceEnabled && m_databaseManager && m_databaseManager->isOpen()) {
      m_databaseManager->updateLastConnectedTime();
      m_databaseManager->closeDatabase();
      qDebug() << "Database closed";
    }

    m_connection->close();
    m_initialized = false;
    m_initState = NOT_STARTED;
    qDebug() << "Disconnected from device";
    emit disconnected();
  }
}

bool MeshClient::isConnected() const {
  return m_connection && m_connection->isOpen();
}

void MeshClient::startInitSequence() {
  if (!m_connection || !m_connection->isOpen()) {
    emit errorOccurred("Cannot initialize: not connected");
    return;
  }

  if (m_initialized) {
    qDebug() << "Already initialized";
    emit initializationComplete();
    return;
  }

  qDebug() << "Starting initialization sequence...";
  m_initState = NOT_STARTED;
  sendNextInitCommand();
}

void MeshClient::sendNextInitCommand() {
  QByteArray cmd;

  switch (m_initState) {
  case NOT_STARTED:
    // Step 1: Send DEVICE_QUERY
    qDebug() << "Sending CMD_DEVICE_QUERY...";
    cmd = CommandBuilder::buildDeviceQuery(PROTOCOL_VERSION);
    m_connection->sendFrame(cmd);
    m_initState = SENT_DEVICE_QUERY;
    break;

  case SENT_DEVICE_QUERY:
    // Step 2: Send APP_START
    qDebug() << "Sending CMD_APP_START...";
    cmd = CommandBuilder::buildAppStart(1, "MeshCoreQt");
    m_connection->sendFrame(cmd);
    m_initState = SENT_APP_START;
    break;

  case SENT_APP_START:
    // Step 3: Send GET_CONTACTS
    qDebug() << "Sending CMD_GET_CONTACTS...";
    cmd = CommandBuilder::buildGetContacts(0);
    m_connection->sendFrame(cmd);
    m_initState = SENT_GET_CONTACTS;
    break;

  case SENT_GET_CONTACTS:
    // Wait for contacts to be received
    // After END_OF_CONTACTS, handleResponse will start channel discovery
    break;

  case DISCOVERING_CHANNELS:
    // Wait for channel discovery to complete
    // Discovery is handled in requestNextChannel() loop
    break;

  case COMPLETE:
    // Already complete
    break;
  }
}

void MeshClient::discoverChannels() {
  if (!m_initialized) {
    emit errorOccurred("Cannot discover channels: not initialized");
    return;
  }

  qDebug() << "Starting channel discovery...";
  m_isDiscoveringChannels = true;
  m_nextChannelIdx = 0;
  m_channelManager->setDiscovering(true);
  requestNextChannel();
}

void MeshClient::joinChannel(const QString &name, const QString &pskHex) {
  if (!m_initialized) {
    emit errorOccurred("Cannot join channel: not initialized");
    return;
  }

  // Validate PSK hex string
  if (pskHex.length() != 32 && pskHex.length() != 64) {
    emit errorOccurred(
        "Invalid PSK: must be 32 or 64 hex characters (16 or 32 bytes)");
    return;
  }

  // Convert hex string to bytes
  QByteArray pskBytes = QByteArray::fromHex(pskHex.toUtf8());
  if (pskBytes.isEmpty()) {
    emit errorOccurred("Invalid PSK: failed to decode hex string");
    return;
  }

  // Get next available channel index
  uint8_t channelIdx = m_channelManager->getNextAvailableIndex();

  qDebug() << "Joining channel" << name << "at index" << channelIdx;

  // Build and send SET_CHANNEL command
  QByteArray cmd = CommandBuilder::buildSetChannel(channelIdx, name, pskBytes);
  m_connection->sendFrame(cmd);

  // Add to local channel manager
  Channel channel;
  channel.index = channelIdx;
  channel.name = name;
  channel.secret = pskBytes;
  channel.isValid = true;
  m_channelManager->addOrUpdateChannel(channel);
}

void MeshClient::requestNextChannel() {
  if (!m_isDiscoveringChannels)
    return;

  qDebug() << "Requesting channel" << m_nextChannelIdx << "...";
  QByteArray cmd = CommandBuilder::buildGetChannel(m_nextChannelIdx);
  m_connection->sendFrame(cmd);
}

QVector<Channel> MeshClient::getChannels() const {
  return m_channelManager->getChannels();
}

// Contact operations
QVector<Contact> MeshClient::getContacts() const { return m_contacts; }

void MeshClient::addOrUpdateContact(const Contact &contact) {
  if (!m_initialized) {
    emit errorOccurred("Cannot add contact: not initialized");
    return;
  }

  if (!contact.isValid()) {
    emit errorOccurred("Cannot add invalid contact");
    return;
  }

  // Send command to device
  QByteArray cmd = CommandBuilder::buildAddUpdateContact(
      contact.publicKey(), contact.name(), contact.type(), contact.flags(),
      contact.pathLength(), contact.path(), contact.latitude(),
      contact.longitude(), contact.lastAdvertTimestamp());

  qDebug() << "Adding/updating contact:" << contact.name();
  m_connection->sendFrame(cmd);

  // Update local storage
  bool found = false;
  for (int i = 0; i < m_contacts.size(); ++i) {
    if (m_contacts[i].publicKey() == contact.publicKey()) {
      m_contacts[i] = contact;
      found = true;
      break;
    }
  }

  if (!found) {
    m_contacts.append(contact);
  }

  emit contactReceived(contact);
  emit contactsUpdated();
}

void MeshClient::removeContact(const QByteArray &publicKey) {
  if (!m_initialized) {
    emit errorOccurred("Cannot remove contact: not initialized");
    return;
  }

  if (publicKey.size() != 32) {
    emit errorOccurred("Invalid public key size (must be 32 bytes)");
    return;
  }

  // Send command to device
  QByteArray cmd = CommandBuilder::buildRemoveContact(publicKey);
  qDebug() << "Removing contact:" << publicKey.toHex();
  m_connection->sendFrame(cmd);

  // Remove from local storage
  for (int i = 0; i < m_contacts.size(); ++i) {
    if (m_contacts[i].publicKey() == publicKey) {
      m_contacts.removeAt(i);
      break;
    }
  }

  emit contactRemoved(publicKey);
  emit contactsUpdated();
}

void MeshClient::requestContactByKey(const QByteArray &publicKey) {
  if (!m_initialized) {
    emit errorOccurred("Cannot request contact: not initialized");
    return;
  }

  if (publicKey.size() != 32) {
    emit errorOccurred("Invalid public key size (must be 32 bytes)");
    return;
  }

  QByteArray cmd = CommandBuilder::buildGetContactByKey(publicKey);
  qDebug() << "Requesting contact:" << publicKey.toHex();
  m_connection->sendFrame(cmd);
}

void MeshClient::sendSelfAdvert(bool floodMode) {
  if (!m_initialized) {
    emit errorOccurred("Cannot send advert: not initialized");
    return;
  }

  qDebug() << "Sending self advertisement" << (floodMode ? "(flood mode)" : "(direct)");
  QByteArray cmd = CommandBuilder::buildSendSelfAdvert(floodMode ? 1 : 0);
  m_connection->sendFrame(cmd);
}

void MeshClient::setAdvertName(const QString &name) {
  if (!m_connection || !m_connection->isOpen()) {
    emit errorOccurred("Cannot set advert name: not connected");
    return;
  }

  qDebug() << "Setting advert name:" << name;
  QByteArray cmd = CommandBuilder::buildSetAdvertName(name);
  m_connection->sendFrame(cmd);
}

void MeshClient::setAdvertLocation(double latitude, double longitude) {
  if (!m_initialized) {
    emit errorOccurred("Cannot set location: not initialized");
    return;
  }

  // Convert to int32 (multiply by 1E6)
  int32_t lat = static_cast<int32_t>(latitude * 1000000.0);
  int32_t lon = static_cast<int32_t>(longitude * 1000000.0);

  qDebug() << "Setting advert location:" << latitude << "," << longitude;
  QByteArray cmd = CommandBuilder::buildSetAdvertLatLon(lat, lon);
  m_connection->sendFrame(cmd);
}

void MeshClient::sendChannelMessage(uint8_t channelIdx, const QString &text) {
  if (!m_initialized) {
    emit errorOccurred("Cannot send message: not initialized");
    return;
  }

  if (!m_channelManager->hasChannel(channelIdx)) {
    emit errorOccurred(QString("Channel %1 not found").arg(channelIdx));
    return;
  }

  uint32_t timestamp = QDateTime::currentSecsSinceEpoch();
  QByteArray cmd = CommandBuilder::buildSendChannelTxtMsg(
      static_cast<uint8_t>(TextType::PLAIN), channelIdx, timestamp, text);

  qDebug() << "Sending message to channel" << channelIdx << ":" << text;
  m_connection->sendFrame(cmd);
}

void MeshClient::sendDirectMessage(const QByteArray &recipientPubKey,
                                    const QString &text) {
  if (!m_initialized) {
    emit errorOccurred("Cannot send message: not initialized");
    return;
  }

  if (recipientPubKey.size() < 6) {
    emit errorOccurred("Invalid recipient public key (too short)");
    return;
  }

  uint32_t timestamp = QDateTime::currentSecsSinceEpoch();
  uint8_t attempt = 0; // First attempt
  QByteArray cmd = CommandBuilder::buildSendTxtMsg(
      TXT_TYPE_PLAIN, attempt, timestamp, recipientPubKey, text);

  qDebug() << "Sending direct message to"
           << recipientPubKey.left(6).toHex() << ":" << text;
  m_connection->sendFrame(cmd);
}

void MeshClient::sendDirectMessage(const Contact &recipient,
                                    const QString &text) {
  sendDirectMessage(recipient.publicKey(), text);
}

void MeshClient::syncNextMessage() {
  if (!m_initialized) {
    emit errorOccurred("Cannot sync messages: not initialized");
    return;
  }

  QByteArray cmd = CommandBuilder::buildSyncNextMessage();
  m_connection->sendFrame(cmd);
}

void MeshClient::setRadioConfig(const RadioConfig &config) {
  if (!m_connection || !m_connection->isOpen()) {
    emit errorOccurred("Cannot set radio config: not connected");
    return;
  }

  if (!config.isValid()) {
    emit errorOccurred("Invalid radio configuration");
    return;
  }

  qDebug() << "Setting radio config:" << config.toString();

  QByteArray cmd = CommandBuilder::buildSetRadioParams(
      config.frequencyKhz, config.bandwidthHz, config.spreadingFactor,
      config.codingRate);

  m_connection->sendFrame(cmd);
  // Will receive RESP_CODE_OK or RESP_CODE_ERR as confirmation
}

void MeshClient::setRadioPreset(const QString &presetName) {
  QMap<QString, RadioConfig> presets = RadioPresets::getAllPresets();

  if (!presets.contains(presetName)) {
    emit errorOccurred(QString("Unknown preset: %1").arg(presetName));
    return;
  }

  RadioConfig config = presets[presetName];
  setRadioConfig(config);
}

void MeshClient::onFrameReceived(const QByteArray &frame) {
  if (frame.isEmpty())
    return;

  if (ResponseParser::isPushNotification(frame)) {
    handlePushNotification(frame);
  } else {
    handleResponse(frame);
  }
}

void MeshClient::handleResponse(const QByteArray &frame) {
  ResponseCode code = ResponseParser::getResponseCode(frame);

  // Handle initialization responses
  if (m_initState != COMPLETE) {
    switch (m_initState) {
    case SENT_DEVICE_QUERY:
      if (code == ResponseCode::DEVICE_INFO) {
        m_deviceInfo = ResponseParser::parseDeviceInfo(frame);
        qDebug() << "Device info:" << m_deviceInfo.firmwareName << "v"
                 << m_deviceInfo.firmwareVersion;
        sendNextInitCommand();
        return;
      }
      break;

    case SENT_APP_START:
      if (code == ResponseCode::SELF_INFO) {
        m_selfInfo = ResponseParser::parseSelfInfo(frame);
        qDebug() << "Self info received, public key:"
                 << m_selfInfo.publicKey.toHex();

        // Open database for persistence
        if (m_persistenceEnabled && m_databaseManager) {
          if (m_databaseManager->openDatabase(m_selfInfo.publicKey)) {
            qDebug() << "Database opened:" << m_databaseManager->getDatabasePath(m_selfInfo.publicKey);
            // Save device info
            m_databaseManager->saveDeviceInfo(m_deviceInfo, m_selfInfo);

            // Save current channels to database (especially public channel)
            QVector<Channel> currentChannels = m_channelManager->getChannels();
            for (const Channel &ch : currentChannels) {
              m_databaseManager->saveChannel(ch);
              qDebug() << "Saved existing channel to DB:" << ch.index << ch.name;
            }

            // Load cached contacts and channels
            QVector<Contact> cachedContacts = m_databaseManager->loadAllContacts();
            QVector<Channel> cachedChannels = m_databaseManager->loadAllChannels();
            qDebug() << "Loaded" << cachedContacts.size() << "cached contacts,"
                     << cachedChannels.size() << "cached channels";
            // Merge will happen after device sends contacts
          } else {
            qWarning() << "Failed to open database:" << m_databaseManager->getLastError();
          }
        }

        sendNextInitCommand();
        return;
      }
      break;

    case SENT_GET_CONTACTS:
      if (code == ResponseCode::CONTACTS_START) {
        qDebug() << "Contacts sync started";
        m_contacts.clear();
        return;
      } else if (code == ResponseCode::CONTACT) {
        Contact contact = ResponseParser::parseContact(frame);
        if (contact.isValid()) {
          m_contacts.append(contact);
          qDebug() << "Contact received:" << contact.name();
          emit contactReceived(contact);
        }
        return;
      } else if (code == ResponseCode::END_OF_CONTACTS) {
        qDebug() << "Contacts sync complete - received" << m_contacts.size()
                 << "contacts";
        emit contactsUpdated();

        // Start automatic channel discovery
        m_initState = DISCOVERING_CHANNELS;
        qDebug() << "Starting automatic channel discovery...";
        m_isDiscoveringChannels = true;
        m_nextChannelIdx = 0;
        requestNextChannel();
        return;
      } else if (code == ResponseCode::ERR) {
        qDebug() << "Got error during contact sync, completing init anyway";
        sendNextInitCommand();
        return;
      }
      break;

    default:
      break;
    }
  }

  // Handle other responses
  switch (code) {
  case ResponseCode::OK:
    qDebug() << "Received OK response";
    // Radio configuration confirmed, message sent, etc.
    break;

  case ResponseCode::ERR: {
    ErrorCode errCode = ResponseParser::getErrorCode(frame);
    if (m_isDiscoveringChannels && errCode == ErrorCode::NOT_FOUND) {
      // No more channels to discover
      qDebug() << "Channel discovery complete - no more channels";
      m_isDiscoveringChannels = false;
      m_channelManager->setDiscovering(false);
      emit channelListUpdated();

      // If discovery was part of init sequence, complete initialization
      if (m_initState == DISCOVERING_CHANNELS) {
        m_initState = COMPLETE;
        m_initialized = true;
        qDebug() << "Initialization complete (with channel discovery)";
        emit initializationComplete();
      }
    } else {
      qWarning() << "Error response:" << static_cast<int>(errCode);
      emit errorOccurred(
          QString("Device error: %1").arg(static_cast<int>(errCode)));
    }
    break;
  }

  case ResponseCode::CHANNEL_INFO: {
    Channel channel = ResponseParser::parseChannelInfo(frame);

    // Filter out empty channels
    if (channel.isEmpty()) {
      qDebug() << "Skipping empty channel at index" << channel.index;
    } else {
      qDebug() << "Channel discovered:" << channel.index << channel.name;
      m_channelManager->addOrUpdateChannel(channel);

      // Save to database
      if (m_persistenceEnabled && m_databaseManager && m_databaseManager->isOpen()) {
        m_databaseManager->saveChannel(channel);
      }

      emit channelDiscovered(channel);
    }

    if (m_isDiscoveringChannels) {
      // Continue discovery regardless of whether channel was empty
      m_nextChannelIdx++;
      requestNextChannel();
    }
    break;
  }

  case ResponseCode::CONTACT: {
    Contact contact = ResponseParser::parseContact(frame);
    if (contact.isValid()) {
      qDebug() << "Contact received:" << contact.name();

      // Update local storage
      bool found = false;
      for (int i = 0; i < m_contacts.size(); ++i) {
        if (m_contacts[i].publicKey() == contact.publicKey()) {
          m_contacts[i] = contact;
          found = true;
          break;
        }
      }

      if (!found) {
        m_contacts.append(contact);
      }

      // Save to database
      if (m_persistenceEnabled && m_databaseManager && m_databaseManager->isOpen()) {
        m_databaseManager->saveContact(contact);
      }

      emit contactReceived(contact);
      emit contactsUpdated();
    }
    break;
  }

  case ResponseCode::CHANNEL_MSG_RECV_V3: {
    Message msg = ResponseParser::parseChannelMsgRecvV3(frame);
    qDebug() << "Channel message received from" << msg.senderName
             << "on channel" << msg.channelIdx;

    // Save to database
    if (m_persistenceEnabled && m_databaseManager && m_databaseManager->isOpen()) {
      if (!m_databaseManager->saveMessage(msg, false)) {
        qWarning() << "Failed to save message:" << m_databaseManager->getLastError();
      }
    }

    emit channelMessageReceived(msg);
    break;
  }

  case ResponseCode::NO_MORE_MESSAGES:
    qDebug() << "No more messages in queue";
    emit noMoreMessages();
    break;

  case ResponseCode::SENT:
    qDebug() << "Message sent confirmation";
    break;

  case ResponseCode::CONTACT_MSG_RECV_V3: {
    Message msg = ResponseParser::parseContactMsgRecvV3(frame);
    qDebug() << "Direct message received from"
             << msg.senderPubKeyPrefix.toHex() << ":" << msg.text;

    // Save to database
    if (m_persistenceEnabled && m_databaseManager && m_databaseManager->isOpen()) {
      if (!m_databaseManager->saveMessage(msg, false)) {
        qWarning() << "Failed to save direct message:" << m_databaseManager->getLastError();
      }
    }

    emit contactMessageReceived(msg);
    break;
  }

  default:
    qDebug() << "Unhandled response code:" << static_cast<int>(code);
    break;
  }
}

void MeshClient::handlePushNotification(const QByteArray &frame) {
  PushCode code = ResponseParser::getPushCode(frame);

  switch (code) {
  case PushCode::MSG_WAITING:
    qDebug() << "New message waiting - use syncNextMessage() to retrieve";
    emit newMessageWaiting();
    break;

  case PushCode::SEND_CONFIRMED:
    qDebug() << "Message send confirmed";
    break;

  case PushCode::PATH_UPDATED:
    qDebug() << "Path updated notification";
    break;

  case PushCode::LOG_RX_DATA: {
    // Raw RX data logging - useful for debugging
    if (frame.size() >= 3) {
      int8_t snrRaw = static_cast<int8_t>(frame[1]);
      float snr = snrRaw / 4.0f;
      int8_t rssi = static_cast<int8_t>(frame[2]);
      QByteArray rawData = frame.mid(3);

      qDebug() << "Raw RX data logged: SNR=" << snr
               << "dB, RSSI=" << rssi
               << "dBm, payload=" << rawData.toHex();
    }
    break;
  }

  default:
    qDebug() << "Unhandled push notification:" << static_cast<int>(code);
    break;
  }
}

void MeshClient::onConnectionStateChanged(ConnectionState state) {
  qDebug() << "Connection state changed:" << static_cast<int>(state);

  if (state == ConnectionState::Connected) {
    emit connected();
  } else if (state == ConnectionState::Disconnected) {
    m_initialized = false;
    m_initState = NOT_STARTED;
    emit disconnected();
  }
}

void MeshClient::onSerialError(const QString &error) {
  qWarning() << "Serial error:" << error;
  emit errorOccurred(error);
}

void MeshClient::scanBLEDevices(bool filterMeshCoreOnly) {
  qDebug() << "Starting BLE device scan (filter:" << filterMeshCoreOnly << ")";

  // Note: With Info.plist properly configured, macOS will automatically
  // prompt for Bluetooth permission when we attempt to use Bluetooth.
  // No need for Qt's permission API which requires GUI framework.

  // Create temporary BLE connection just for discovery
  // Don't use m_connection as we might already be connected
  BLEConnection *bleConn = new BLEConnection(this);

  // Forward device discoveries
  connect(bleConn, &BLEConnection::bleDeviceDiscovered, this,
          [this](const BLEDeviceInfo &device) {
            m_bleDevices.append(device);
            emit bleDeviceFound(device);
          });

  // Clean up and signal completion
  connect(bleConn, &BLEConnection::discoveryFinished, this,
          [this, bleConn]() {
            qDebug() << "BLE discovery finished. Found" << m_bleDevices.size()
                     << "device(s)";
            emit bleDiscoveryFinished();
            bleConn->deleteLater(); // Qt will delete after event loop
          });

  // Handle discovery errors
  connect(bleConn, &BLEConnection::errorOccurred, this,
          [this, bleConn](const QString &error) {
            qWarning() << "BLE discovery error:" << error;
            emit errorOccurred(error);
            bleConn->deleteLater();
          });

  m_bleDevices.clear();
  bleConn->startDiscovery(filterMeshCoreOnly);
}

void MeshClient::scanSerialPorts() {
  qDebug() << "Scanning serial ports...";
  m_serialPorts = SerialConnection::enumeratePorts();
  qDebug() << "Found" << m_serialPorts.size() << "serial port(s)";
}

QList<BLEDeviceInfo> MeshClient::getDiscoveredBLEDevices() const {
  return m_bleDevices;
}

QList<SerialPortInfo> MeshClient::getAvailableSerialPorts() const {
  return m_serialPorts;
}

// Persistence methods

void MeshClient::enablePersistence(bool enable) {
  m_persistenceEnabled = enable;
  qDebug() << "Persistence" << (enable ? "enabled" : "disabled");
}

QVector<Message> MeshClient::getMessageHistory(int limit, int offset) {
  if (!m_persistenceEnabled || !m_databaseManager || !m_databaseManager->isOpen()) {
    qWarning() << "Cannot get message history: persistence not enabled or database not open";
    return QVector<Message>();
  }

  return m_databaseManager->loadMessages(limit, offset);
}

QVector<Message> MeshClient::getChannelMessageHistory(uint8_t channelIdx, int limit) {
  if (!m_persistenceEnabled || !m_databaseManager || !m_databaseManager->isOpen()) {
    qWarning() << "Cannot get channel message history: persistence not enabled or database not open";
    return QVector<Message>();
  }

  return m_databaseManager->loadChannelMessages(channelIdx, limit);
}

} // namespace MeshCore
