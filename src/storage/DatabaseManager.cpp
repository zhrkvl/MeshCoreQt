#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QMutexLocker>
#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>
#include <QVariant>

namespace MeshCore {

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent), m_currentDbPath(""), m_currentDeviceKey() {}

DatabaseManager::~DatabaseManager() { closeDatabase(); }

QString DatabaseManager::getDatabasePath(const QByteArray &devicePublicKey) const {
  QString appDataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(appDataPath); // Ensure directory exists

  QString publicKeyHex = devicePublicKey.toHex();
  return QString("%1/device_%2.db").arg(appDataPath, publicKeyHex);
}

bool DatabaseManager::openDatabase(const QByteArray &devicePublicKey) {
  QMutexLocker locker(&m_mutex);

  if (m_db.isOpen() && m_currentDeviceKey == devicePublicKey) {
    return true; // Already open for this device
  }

  // Close existing connection if open
  if (m_db.isOpen()) {
    closeDatabase();
  }

  m_currentDbPath = getDatabasePath(devicePublicKey);
  m_currentDeviceKey = devicePublicKey;

  QString connectionName = QString("MeshCoreQt_%1").arg(devicePublicKey.toHex());

  // Remove old connection if exists
  if (QSqlDatabase::contains(connectionName)) {
    QSqlDatabase::removeDatabase(connectionName);
  }

  m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
  m_db.setDatabaseName(m_currentDbPath);

  if (!m_db.open()) {
    m_lastError = QString("Failed to open database: %1").arg(m_db.lastError().text());
    qWarning() << m_lastError;
    emit errorOccurred(m_lastError);
    return false;
  }

  // Enable Write-Ahead Logging for better performance and concurrency
  QSqlQuery query(m_db);
  query.exec("PRAGMA journal_mode=WAL");
  query.exec("PRAGMA foreign_keys=ON");

  if (!initializeSchema()) {
    m_lastError = "Failed to initialize database schema";
    qWarning() << m_lastError;
    closeDatabase();
    emit errorOccurred(m_lastError);
    return false;
  }

  qDebug() << "Database opened:" << m_currentDbPath;
  emit databaseOpened(m_currentDbPath);
  return true;
}

void DatabaseManager::closeDatabase() {
  QMutexLocker locker(&m_mutex);

  if (m_db.isOpen()) {
    QString connectionName = m_db.connectionName();
    m_db.close();
    QSqlDatabase::removeDatabase(connectionName);
    qDebug() << "Database closed:" << m_currentDbPath;
    emit databaseClosed();
  }

  m_currentDbPath.clear();
  m_currentDeviceKey.clear();
}

bool DatabaseManager::isOpen() const {
  QMutexLocker locker(&m_mutex);
  return m_db.isOpen();
}

bool DatabaseManager::initializeSchema() {
  // Check if schema exists
  int version = getCurrentSchemaVersion();

  if (version == 0) {
    // New database, create schema
    if (!createTables()) {
      return false;
    }
    if (!insertSchemaVersion(CURRENT_SCHEMA_VERSION)) {
      return false;
    }
  } else if (version < CURRENT_SCHEMA_VERSION) {
    // Schema upgrade needed
    if (!migrateSchema(version, CURRENT_SCHEMA_VERSION)) {
      m_lastError = QString("Failed to migrate schema from v%1 to v%2")
                        .arg(version)
                        .arg(CURRENT_SCHEMA_VERSION);
      return false;
    }
  }

  return true;
}

bool DatabaseManager::createTables() {
  QSqlQuery query(m_db);

  // Start transaction
  if (!m_db.transaction()) {
    m_lastError = "Failed to start transaction";
    return false;
  }

  // Schema version table
  if (!query.exec("CREATE TABLE IF NOT EXISTS schema_version ("
                  "version INTEGER PRIMARY KEY, "
                  "applied_at INTEGER NOT NULL)")) {
    m_lastError = QString("Failed to create schema_version table: %1")
                      .arg(query.lastError().text());
    m_db.rollback();
    return false;
  }

  // Device info table
  if (!query.exec("CREATE TABLE IF NOT EXISTS device_info ("
                  "id INTEGER PRIMARY KEY CHECK (id = 1), "
                  "public_key BLOB NOT NULL, "
                  "node_name TEXT, "
                  "firmware_version INTEGER, "
                  "firmware_name TEXT, "
                  "protocol_version INTEGER, "
                  "contact_type INTEGER, "
                  "flags INTEGER, "
                  "last_connected_at INTEGER, "
                  "created_at INTEGER NOT NULL)")) {
    m_lastError =
        QString("Failed to create device_info table: %1").arg(query.lastError().text());
    m_db.rollback();
    return false;
  }

  // Contacts table
  if (!query.exec("CREATE TABLE IF NOT EXISTS contacts ("
                  "public_key BLOB PRIMARY KEY, "
                  "name TEXT NOT NULL, "
                  "type INTEGER NOT NULL, "
                  "flags INTEGER NOT NULL, "
                  "path_length INTEGER, "
                  "path BLOB, "
                  "last_advert_timestamp INTEGER, "
                  "last_modified INTEGER, "
                  "latitude INTEGER, "
                  "longitude INTEGER, "
                  "created_at INTEGER NOT NULL, "
                  "updated_at INTEGER NOT NULL)")) {
    m_lastError =
        QString("Failed to create contacts table: %1").arg(query.lastError().text());
    m_db.rollback();
    return false;
  }

  query.exec("CREATE INDEX IF NOT EXISTS idx_contacts_name ON contacts(name)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_contacts_updated_at ON contacts(updated_at)");

  // Channels table
  if (!query.exec("CREATE TABLE IF NOT EXISTS channels ("
                  "idx INTEGER PRIMARY KEY, "
                  "name TEXT NOT NULL, "
                  "secret BLOB NOT NULL, "
                  "created_at INTEGER NOT NULL, "
                  "updated_at INTEGER NOT NULL)")) {
    m_lastError =
        QString("Failed to create channels table: %1").arg(query.lastError().text());
    m_db.rollback();
    return false;
  }

  // Messages table
  if (!query.exec("CREATE TABLE IF NOT EXISTS messages ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "message_type INTEGER NOT NULL, "
                  "channel_idx INTEGER, "
                  "sender_pubkey_prefix BLOB, "
                  "sender_name TEXT, "
                  "text TEXT NOT NULL, "
                  "timestamp INTEGER NOT NULL, "
                  "received_at INTEGER NOT NULL, "
                  "path_length INTEGER, "
                  "txt_type INTEGER, "
                  "snr REAL, "
                  "is_sent_by_me INTEGER DEFAULT 0, "
                  "FOREIGN KEY (channel_idx) REFERENCES channels(idx) ON DELETE SET NULL)")) {
    m_lastError =
        QString("Failed to create messages table: %1").arg(query.lastError().text());
    m_db.rollback();
    return false;
  }

  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_messages_channel ON messages(channel_idx, timestamp "
      "DESC)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_messages_sender ON messages(sender_pubkey_prefix, "
      "timestamp DESC)");
  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_messages_received_at ON messages(received_at DESC)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp DESC)");

  // Message hashes for deduplication
  if (!query.exec("CREATE TABLE IF NOT EXISTS message_hashes ("
                  "hash TEXT PRIMARY KEY, "
                  "message_id INTEGER NOT NULL, "
                  "created_at INTEGER NOT NULL, "
                  "FOREIGN KEY (message_id) REFERENCES messages(id) ON DELETE CASCADE)")) {
    m_lastError =
        QString("Failed to create message_hashes table: %1").arg(query.lastError().text());
    m_db.rollback();
    return false;
  }

  query.exec(
      "CREATE INDEX IF NOT EXISTS idx_message_hashes_created_at ON message_hashes(created_at)");

  // Commit transaction
  if (!m_db.commit()) {
    m_lastError = "Failed to commit transaction";
    m_db.rollback();
    return false;
  }

  return true;
}

bool DatabaseManager::insertSchemaVersion(int version) {
  QSqlQuery query(m_db);
  query.prepare("INSERT INTO schema_version (version, applied_at) VALUES (?, ?)");
  query.addBindValue(version);
  query.addBindValue(QDateTime::currentSecsSinceEpoch());

  if (!query.exec()) {
    m_lastError =
        QString("Failed to insert schema version: %1").arg(query.lastError().text());
    return false;
  }

  return true;
}

int DatabaseManager::getCurrentSchemaVersion() {
  QSqlQuery query(m_db);
  if (!query.exec("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1")) {
    return 0; // Table doesn't exist or error
  }

  if (query.next()) {
    return query.value(0).toInt();
  }

  return 0;
}

bool DatabaseManager::migrateSchema(int fromVersion, int toVersion) {
  // Future migrations will be implemented here
  // For now, we only have version 1
  qDebug() << "Migration from version" << fromVersion << "to" << toVersion
           << "not yet implemented";
  return false;
}

// Device info operations

bool DatabaseManager::saveDeviceInfo(const DeviceInfo &deviceInfo,
                                     const SelfInfo &selfInfo) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("INSERT OR REPLACE INTO device_info "
                "(id, public_key, node_name, firmware_version, firmware_name, "
                "protocol_version, contact_type, flags, last_connected_at, created_at) "
                "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

  query.addBindValue(selfInfo.publicKey);
  query.addBindValue(selfInfo.nodeName);
  query.addBindValue(deviceInfo.firmwareVersion);
  query.addBindValue(deviceInfo.firmwareName);
  query.addBindValue(deviceInfo.protocolVersion);
  query.addBindValue(selfInfo.contactType);
  query.addBindValue(selfInfo.flags);
  query.addBindValue(QDateTime::currentSecsSinceEpoch());
  query.addBindValue(QDateTime::currentSecsSinceEpoch());

  if (!query.exec()) {
    m_lastError = QString("Failed to save device info: %1").arg(query.lastError().text());
    qWarning() << m_lastError;
    return false;
  }

  return true;
}

bool DatabaseManager::loadDeviceInfo(DeviceInfo &deviceInfo, SelfInfo &selfInfo) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);
  if (!query.exec("SELECT public_key, node_name, firmware_version, firmware_name, "
                  "protocol_version, contact_type, flags FROM device_info WHERE id = 1")) {
    m_lastError = QString("Failed to load device info: %1").arg(query.lastError().text());
    return false;
  }

  if (query.next()) {
    selfInfo.publicKey = query.value(0).toByteArray();
    selfInfo.nodeName = query.value(1).toString();
    deviceInfo.firmwareVersion = query.value(2).toUInt();
    deviceInfo.firmwareName = query.value(3).toString();
    deviceInfo.protocolVersion = query.value(4).toUInt();
    selfInfo.contactType = query.value(5).toUInt();
    selfInfo.flags = query.value(6).toUInt();
    return true;
  }

  return false; // No device info found
}

bool DatabaseManager::updateLastConnectedTime() {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("UPDATE device_info SET last_connected_at = ? WHERE id = 1");
  query.addBindValue(QDateTime::currentSecsSinceEpoch());

  if (!query.exec()) {
    m_lastError =
        QString("Failed to update last connected time: %1").arg(query.lastError().text());
    return false;
  }

  return true;
}

// Contact operations

bool DatabaseManager::saveContact(const Contact &contact) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare(
      "INSERT OR REPLACE INTO contacts "
      "(public_key, name, type, flags, path_length, path, last_advert_timestamp, "
      "last_modified, latitude, longitude, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
      "COALESCE((SELECT created_at FROM contacts WHERE public_key = ?), ?), ?)");

  query.addBindValue(contact.publicKey());
  query.addBindValue(contact.name());
  query.addBindValue(contact.type());
  query.addBindValue(contact.flags());
  query.addBindValue(contact.pathLength());
  query.addBindValue(contact.path());
  query.addBindValue(contact.lastAdvertTimestamp());
  query.addBindValue(contact.lastModified());
  query.addBindValue(contact.latitude());
  query.addBindValue(contact.longitude());
  query.addBindValue(contact.publicKey()); // For COALESCE
  query.addBindValue(QDateTime::currentSecsSinceEpoch()); // created_at if new
  query.addBindValue(QDateTime::currentSecsSinceEpoch()); // updated_at

  if (!query.exec()) {
    m_lastError = QString("Failed to save contact: %1").arg(query.lastError().text());
    qWarning() << m_lastError;
    return false;
  }

  return true;
}

bool DatabaseManager::saveContacts(const QVector<Contact> &contacts) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  if (!m_db.transaction()) {
    m_lastError = "Failed to start transaction";
    return false;
  }

  for (const Contact &contact : contacts) {
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT OR REPLACE INTO contacts "
        "(public_key, name, type, flags, path_length, path, last_advert_timestamp, "
        "last_modified, latitude, longitude, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "COALESCE((SELECT created_at FROM contacts WHERE public_key = ?), ?), ?)");

    query.addBindValue(contact.publicKey());
    query.addBindValue(contact.name());
    query.addBindValue(contact.type());
    query.addBindValue(contact.flags());
    query.addBindValue(contact.pathLength());
    query.addBindValue(contact.path());
    query.addBindValue(contact.lastAdvertTimestamp());
    query.addBindValue(contact.lastModified());
    query.addBindValue(contact.latitude());
    query.addBindValue(contact.longitude());
    query.addBindValue(contact.publicKey());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
      m_lastError = QString("Failed to save contact batch: %1").arg(query.lastError().text());
      m_db.rollback();
      return false;
    }
  }

  if (!m_db.commit()) {
    m_lastError = "Failed to commit contacts transaction";
    m_db.rollback();
    return false;
  }

  return true;
}

bool DatabaseManager::deleteContact(const QByteArray &publicKey) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("DELETE FROM contacts WHERE public_key = ?");
  query.addBindValue(publicKey);

  if (!query.exec()) {
    m_lastError = QString("Failed to delete contact: %1").arg(query.lastError().text());
    return false;
  }

  return true;
}

QVector<Contact> DatabaseManager::loadAllContacts() {
  QMutexLocker locker(&m_mutex);
  QVector<Contact> contacts;

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return contacts;
  }

  QSqlQuery query(m_db);
  if (!query.exec("SELECT public_key, name, type, flags, path_length, path, "
                  "last_advert_timestamp, last_modified, latitude, longitude "
                  "FROM contacts ORDER BY name")) {
    m_lastError = QString("Failed to load contacts: %1").arg(query.lastError().text());
    return contacts;
  }

  while (query.next()) {
    Contact contact(query.value(0).toByteArray(), // public_key
                    query.value(1).toString(),    // name
                    query.value(2).toUInt());     // type

    contact.setFlags(query.value(3).toUInt());
    contact.setPath(query.value(5).toByteArray(), query.value(4).toInt());
    contact.setLastAdvertTimestamp(query.value(6).toUInt());
    contact.setLastModified(query.value(7).toUInt());
    contact.setLocation(query.value(8).toInt(), query.value(9).toInt());

    contacts.append(contact);
  }

  return contacts;
}

Contact DatabaseManager::loadContact(const QByteArray &publicKey) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return Contact();
  }

  QSqlQuery query(m_db);
  query.prepare("SELECT public_key, name, type, flags, path_length, path, "
                "last_advert_timestamp, last_modified, latitude, longitude "
                "FROM contacts WHERE public_key = ?");
  query.addBindValue(publicKey);

  if (!query.exec() || !query.next()) {
    return Contact();
  }

  Contact contact(query.value(0).toByteArray(), query.value(1).toString(),
                  query.value(2).toUInt());

  contact.setFlags(query.value(3).toUInt());
  contact.setPath(query.value(5).toByteArray(), query.value(4).toInt());
  contact.setLastAdvertTimestamp(query.value(6).toUInt());
  contact.setLastModified(query.value(7).toUInt());
  contact.setLocation(query.value(8).toInt(), query.value(9).toInt());

  return contact;
}

// Channel operations

bool DatabaseManager::saveChannel(const Channel &channel) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("INSERT OR REPLACE INTO channels "
                "(idx, name, secret, created_at, updated_at) "
                "VALUES (?, ?, ?, "
                "COALESCE((SELECT created_at FROM channels WHERE idx = ?), ?), ?)");

  query.addBindValue(channel.index);
  query.addBindValue(channel.name);
  query.addBindValue(channel.secret);
  query.addBindValue(channel.index); // For COALESCE
  query.addBindValue(QDateTime::currentSecsSinceEpoch()); // created_at if new
  query.addBindValue(QDateTime::currentSecsSinceEpoch()); // updated_at

  if (!query.exec()) {
    m_lastError = QString("Failed to save channel: %1").arg(query.lastError().text());
    qWarning() << m_lastError;
    return false;
  }

  return true;
}

bool DatabaseManager::saveChannels(const QVector<Channel> &channels) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  if (!m_db.transaction()) {
    m_lastError = "Failed to start transaction";
    return false;
  }

  for (const Channel &channel : channels) {
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO channels "
                  "(idx, name, secret, created_at, updated_at) "
                  "VALUES (?, ?, ?, "
                  "COALESCE((SELECT created_at FROM channels WHERE idx = ?), ?), ?)");

    query.addBindValue(channel.index);
    query.addBindValue(channel.name);
    query.addBindValue(channel.secret);
    query.addBindValue(channel.index);
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
      m_lastError = QString("Failed to save channel batch: %1").arg(query.lastError().text());
      m_db.rollback();
      return false;
    }
  }

  if (!m_db.commit()) {
    m_lastError = "Failed to commit channels transaction";
    m_db.rollback();
    return false;
  }

  return true;
}

bool DatabaseManager::deleteChannel(uint8_t channelIdx) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("DELETE FROM channels WHERE idx = ?");
  query.addBindValue(channelIdx);

  if (!query.exec()) {
    m_lastError = QString("Failed to delete channel: %1").arg(query.lastError().text());
    return false;
  }

  return true;
}

QVector<Channel> DatabaseManager::loadAllChannels() {
  QMutexLocker locker(&m_mutex);
  QVector<Channel> channels;

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return channels;
  }

  QSqlQuery query(m_db);
  if (!query.exec("SELECT idx, name, secret FROM channels ORDER BY idx")) {
    m_lastError = QString("Failed to load channels: %1").arg(query.lastError().text());
    return channels;
  }

  while (query.next()) {
    Channel channel(query.value(0).toUInt(), // idx
                    query.value(1).toString(), // name
                    query.value(2).toByteArray()); // secret
    channels.append(channel);
  }

  return channels;
}

Channel DatabaseManager::loadChannel(uint8_t channelIdx) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return Channel();
  }

  QSqlQuery query(m_db);
  query.prepare("SELECT idx, name, secret FROM channels WHERE idx = ?");
  query.addBindValue(channelIdx);

  if (!query.exec() || !query.next()) {
    return Channel();
  }

  return Channel(query.value(0).toUInt(), query.value(1).toString(),
                 query.value(2).toByteArray());
}

// Message operations

QString DatabaseManager::generateMessageHash(const Message &message) const {
  QByteArray data;

  if (message.type == Message::CHANNEL_MESSAGE) {
    data.append(message.senderName.toUtf8());
  } else {
    data.append(message.senderPubKeyPrefix);
  }

  data.append(message.text.toUtf8());
  data.append(reinterpret_cast<const char *>(&message.timestamp), sizeof(message.timestamp));

  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool DatabaseManager::isMessageDuplicate(const Message &message) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    return false;
  }

  QString hash = generateMessageHash(message);

  QSqlQuery query(m_db);
  query.prepare("SELECT 1 FROM message_hashes WHERE hash = ?");
  query.addBindValue(hash);

  return query.exec() && query.next();
}

bool DatabaseManager::saveMessage(const Message &message, bool isSentByMe) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  // Check for duplicate
  QString hash = generateMessageHash(message);
  QSqlQuery checkQuery(m_db);
  checkQuery.prepare("SELECT 1 FROM message_hashes WHERE hash = ?");
  checkQuery.addBindValue(hash);

  if (checkQuery.exec() && checkQuery.next()) {
    // Duplicate, silently skip
    return true;
  }

  if (!m_db.transaction()) {
    m_lastError = "Failed to start transaction";
    return false;
  }

  // Insert message
  QSqlQuery query(m_db);
  query.prepare("INSERT INTO messages "
                "(message_type, channel_idx, sender_pubkey_prefix, sender_name, text, "
                "timestamp, received_at, path_length, txt_type, snr, is_sent_by_me) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

  query.addBindValue(static_cast<int>(message.type));
  query.addBindValue(message.type == Message::CHANNEL_MESSAGE
                         ? QVariant(message.channelIdx)
                         : QVariant());
  query.addBindValue(message.type == Message::CONTACT_MESSAGE ? message.senderPubKeyPrefix
                                                               : QVariant());
  query.addBindValue(message.senderName);
  query.addBindValue(message.text);
  query.addBindValue(message.timestamp);
  query.addBindValue(message.receivedAt.toSecsSinceEpoch());
  query.addBindValue(message.pathLength);
  query.addBindValue(message.txtType);
  query.addBindValue(message.snr);
  query.addBindValue(isSentByMe ? 1 : 0);

  if (!query.exec()) {
    m_lastError = QString("Failed to save message: %1").arg(query.lastError().text());
    qWarning() << m_lastError;
    m_db.rollback();
    return false;
  }

  // Get message ID
  qint64 messageId = query.lastInsertId().toLongLong();

  // Insert hash
  QSqlQuery hashQuery(m_db);
  hashQuery.prepare(
      "INSERT INTO message_hashes (hash, message_id, created_at) VALUES (?, ?, ?)");
  hashQuery.addBindValue(hash);
  hashQuery.addBindValue(messageId);
  hashQuery.addBindValue(QDateTime::currentSecsSinceEpoch());

  if (!hashQuery.exec()) {
    m_lastError = QString("Failed to save message hash: %1").arg(hashQuery.lastError().text());
    qWarning() << m_lastError;
    m_db.rollback();
    return false;
  }

  if (!m_db.commit()) {
    m_lastError = "Failed to commit message transaction";
    m_db.rollback();
    return false;
  }

  return true;
}

QVector<Message> DatabaseManager::loadMessages(int limit, int offset) {
  QMutexLocker locker(&m_mutex);
  QVector<Message> messages;

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return messages;
  }

  QSqlQuery query(m_db);
  query.prepare("SELECT message_type, channel_idx, sender_pubkey_prefix, sender_name, text, "
                "timestamp, received_at, path_length, txt_type, snr, is_sent_by_me "
                "FROM messages ORDER BY received_at DESC LIMIT ? OFFSET ?");
  query.addBindValue(limit);
  query.addBindValue(offset);

  if (!query.exec()) {
    m_lastError = QString("Failed to load messages: %1").arg(query.lastError().text());
    return messages;
  }

  while (query.next()) {
    Message msg;
    msg.type = static_cast<Message::Type>(query.value(0).toInt());
    msg.channelIdx = query.value(1).toUInt();
    msg.senderPubKeyPrefix = query.value(2).toByteArray();
    msg.senderName = query.value(3).toString();
    msg.text = query.value(4).toString();
    msg.timestamp = query.value(5).toUInt();
    msg.receivedAt = QDateTime::fromSecsSinceEpoch(query.value(6).toLongLong());
    msg.pathLength = query.value(7).toUInt();
    msg.pathLen = msg.pathLength; // Alias
    msg.txtType = query.value(8).toUInt();
    msg.snr = query.value(9).toFloat();
    // is_sent_by_me is at index 10, but not stored in Message struct

    messages.append(msg);
  }

  return messages;
}

QVector<Message> DatabaseManager::loadChannelMessages(uint8_t channelIdx, int limit) {
  QMutexLocker locker(&m_mutex);
  QVector<Message> messages;

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return messages;
  }

  QSqlQuery query(m_db);
  query.prepare("SELECT message_type, channel_idx, sender_pubkey_prefix, sender_name, text, "
                "timestamp, received_at, path_length, txt_type, snr, is_sent_by_me "
                "FROM messages WHERE channel_idx = ? ORDER BY timestamp DESC LIMIT ?");
  query.addBindValue(channelIdx);
  query.addBindValue(limit);

  if (!query.exec()) {
    m_lastError =
        QString("Failed to load channel messages: %1").arg(query.lastError().text());
    return messages;
  }

  while (query.next()) {
    Message msg;
    msg.type = static_cast<Message::Type>(query.value(0).toInt());
    msg.channelIdx = query.value(1).toUInt();
    msg.senderPubKeyPrefix = query.value(2).toByteArray();
    msg.senderName = query.value(3).toString();
    msg.text = query.value(4).toString();
    msg.timestamp = query.value(5).toUInt();
    msg.receivedAt = QDateTime::fromSecsSinceEpoch(query.value(6).toLongLong());
    msg.pathLength = query.value(7).toUInt();
    msg.pathLen = msg.pathLength;
    msg.txtType = query.value(8).toUInt();
    msg.snr = query.value(9).toFloat();

    messages.append(msg);
  }

  return messages;
}

QVector<Message> DatabaseManager::loadDirectMessages(const QByteArray &contactPubKeyPrefix,
                                                     int limit) {
  QMutexLocker locker(&m_mutex);
  QVector<Message> messages;

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return messages;
  }

  QSqlQuery query(m_db);
  query.prepare("SELECT message_type, channel_idx, sender_pubkey_prefix, sender_name, text, "
                "timestamp, received_at, path_length, txt_type, snr, is_sent_by_me "
                "FROM messages WHERE sender_pubkey_prefix = ? ORDER BY timestamp DESC LIMIT ?");
  query.addBindValue(contactPubKeyPrefix);
  query.addBindValue(limit);

  if (!query.exec()) {
    m_lastError =
        QString("Failed to load direct messages: %1").arg(query.lastError().text());
    return messages;
  }

  while (query.next()) {
    Message msg;
    msg.type = static_cast<Message::Type>(query.value(0).toInt());
    msg.channelIdx = query.value(1).toUInt();
    msg.senderPubKeyPrefix = query.value(2).toByteArray();
    msg.senderName = query.value(3).toString();
    msg.text = query.value(4).toString();
    msg.timestamp = query.value(5).toUInt();
    msg.receivedAt = QDateTime::fromSecsSinceEpoch(query.value(6).toLongLong());
    msg.pathLength = query.value(7).toUInt();
    msg.pathLen = msg.pathLength;
    msg.txtType = query.value(8).toUInt();
    msg.snr = query.value(9).toFloat();

    messages.append(msg);
  }

  return messages;
}

int DatabaseManager::getMessageCount() {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    return 0;
  }

  QSqlQuery query(m_db);
  if (!query.exec("SELECT COUNT(*) FROM messages")) {
    return 0;
  }

  if (query.next()) {
    return query.value(0).toInt();
  }

  return 0;
}

int DatabaseManager::getChannelMessageCount(uint8_t channelIdx) {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    return 0;
  }

  QSqlQuery query(m_db);
  query.prepare("SELECT COUNT(*) FROM messages WHERE channel_idx = ?");
  query.addBindValue(channelIdx);

  if (!query.exec() || !query.next()) {
    return 0;
  }

  return query.value(0).toInt();
}

bool DatabaseManager::clearAllData() {
  QMutexLocker locker(&m_mutex);

  if (!m_db.isOpen()) {
    m_lastError = "Database not open";
    return false;
  }

  QSqlQuery query(m_db);

  if (!m_db.transaction()) {
    m_lastError = "Failed to start transaction";
    return false;
  }

  query.exec("DELETE FROM message_hashes");
  query.exec("DELETE FROM messages");
  query.exec("DELETE FROM channels");
  query.exec("DELETE FROM contacts");
  query.exec("DELETE FROM device_info");

  if (!m_db.commit()) {
    m_lastError = "Failed to commit clear data transaction";
    m_db.rollback();
    return false;
  }

  return true;
}

} // namespace MeshCore
