#pragma once

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QMutex>
#include <QByteArray>
#include <QVector>

#include "../models/Contact.h"
#include "../models/Channel.h"
#include "../models/Message.h"
#include "../core/DeviceInfo.h"

namespace MeshCore {

class DatabaseManager : public QObject {
  Q_OBJECT

public:
  explicit DatabaseManager(QObject *parent = nullptr);
  ~DatabaseManager();

  // Database lifecycle
  bool openDatabase(const QByteArray &devicePublicKey);
  void closeDatabase();
  bool isOpen() const;
  QString getDatabasePath(const QByteArray &devicePublicKey) const;

  // Device info operations
  bool saveDeviceInfo(const DeviceInfo &deviceInfo, const SelfInfo &selfInfo);
  bool loadDeviceInfo(DeviceInfo &deviceInfo, SelfInfo &selfInfo);
  bool updateLastConnectedTime();

  // Contact operations
  bool saveContact(const Contact &contact);
  bool saveContacts(const QVector<Contact> &contacts);
  bool deleteContact(const QByteArray &publicKey);
  QVector<Contact> loadAllContacts();
  Contact loadContact(const QByteArray &publicKey);

  // Channel operations
  bool saveChannel(const Channel &channel);
  bool saveChannels(const QVector<Channel> &channels);
  bool deleteChannel(uint8_t channelIdx);
  QVector<Channel> loadAllChannels();
  Channel loadChannel(uint8_t channelIdx);

  // Message operations
  bool saveMessage(const Message &message, bool isSentByMe = false);
  QVector<Message> loadMessages(int limit = 100, int offset = 0);
  QVector<Message> loadChannelMessages(uint8_t channelIdx, int limit = 100);
  QVector<Message> loadDirectMessages(const QByteArray &contactPubKeyPrefix,
                                      int limit = 100);
  bool isMessageDuplicate(const Message &message);
  int getMessageCount();
  int getChannelMessageCount(uint8_t channelIdx);

  // Schema management
  int getCurrentSchemaVersion();
  bool migrateSchema(int fromVersion, int toVersion);

  // Utility
  bool clearAllData();
  QString getLastError() const { return m_lastError; }

signals:
  void errorOccurred(const QString &error);
  void databaseOpened(const QString &path);
  void databaseClosed();

private:
  // Schema initialization and migration
  bool initializeSchema();
  bool createTables();
  bool insertSchemaVersion(int version);

  // Helper methods
  QString generateMessageHash(const Message &message) const;
  QSqlDatabase getDatabase();
  bool executeQuery(const QString &query);

  // Member variables
  QSqlDatabase m_db;
  QString m_currentDbPath;
  QByteArray m_currentDeviceKey;
  mutable QMutex m_mutex;
  QString m_lastError;

  static const int CURRENT_SCHEMA_VERSION = 1;
};

} // namespace MeshCore
