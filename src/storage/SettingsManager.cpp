#include "SettingsManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

namespace MeshCore {

SettingsManager::SettingsManager()
    : m_settings("MeshCoreQt", "MeshCoreQt") {
  // Initialize default values if not set
  if (!m_settings.contains(KEY_AUTO_CONNECT)) {
    m_settings.setValue(KEY_AUTO_CONNECT, false);
  }
  if (!m_settings.contains(KEY_SHOW_TIMESTAMPS)) {
    m_settings.setValue(KEY_SHOW_TIMESTAMPS, true);
  }
  if (!m_settings.contains(KEY_SHOW_SNR)) {
    m_settings.setValue(KEY_SHOW_SNR, true);
  }
  if (!m_settings.contains(KEY_DATETIME_FORMAT)) {
    m_settings.setValue(KEY_DATETIME_FORMAT, "yyyy-MM-dd HH:mm:ss");
  }
}

SettingsManager::~SettingsManager() { m_settings.sync(); }

SettingsManager &SettingsManager::instance() {
  static SettingsManager instance;
  return instance;
}

// Connection history

QByteArray SettingsManager::getLastDevicePublicKey() {
  return m_settings.value(KEY_LAST_DEVICE).toByteArray();
}

void SettingsManager::setLastDevicePublicKey(const QByteArray &key) {
  m_settings.setValue(KEY_LAST_DEVICE, key);
}

QString SettingsManager::getLastConnectionType() {
  return m_settings.value(KEY_LAST_TYPE).toString();
}

void SettingsManager::setLastConnectionType(const QString &type) {
  m_settings.setValue(KEY_LAST_TYPE, type);
}

QString SettingsManager::getLastConnectionTarget() {
  return m_settings.value(KEY_LAST_TARGET).toString();
}

void SettingsManager::setLastConnectionTarget(const QString &target) {
  m_settings.setValue(KEY_LAST_TARGET, target);
}

bool SettingsManager::getAutoConnect() {
  return m_settings.value(KEY_AUTO_CONNECT, false).toBool();
}

void SettingsManager::setAutoConnect(bool autoConnect) {
  m_settings.setValue(KEY_AUTO_CONNECT, autoConnect);
}

// Window geometry

QRect SettingsManager::getWindowGeometry() {
  return m_settings.value(KEY_WINDOW_GEOMETRY).toRect();
}

void SettingsManager::setWindowGeometry(const QRect &geometry) {
  m_settings.setValue(KEY_WINDOW_GEOMETRY, geometry);
}

// Display preferences

bool SettingsManager::getShowTimestamps() {
  return m_settings.value(KEY_SHOW_TIMESTAMPS, true).toBool();
}

void SettingsManager::setShowTimestamps(bool show) {
  m_settings.setValue(KEY_SHOW_TIMESTAMPS, show);
}

bool SettingsManager::getShowSNR() { return m_settings.value(KEY_SHOW_SNR, true).toBool(); }

void SettingsManager::setShowSNR(bool show) { m_settings.setValue(KEY_SHOW_SNR, show); }

QString SettingsManager::getDateTimeFormat() {
  return m_settings.value(KEY_DATETIME_FORMAT, "yyyy-MM-dd HH:mm:ss").toString();
}

void SettingsManager::setDateTimeFormat(const QString &format) {
  m_settings.setValue(KEY_DATETIME_FORMAT, format);
}

// Recent devices

QStringList SettingsManager::getRecentDevices() {
  return m_settings.value(KEY_RECENT_DEVICES).toStringList();
}

void SettingsManager::addRecentDevice(const QByteArray &publicKey,
                                      const QString &deviceName) {
  // Load existing recent devices as JSON
  QStringList recentDevices = m_settings.value(KEY_RECENT_DEVICES).toStringList();

  // Create new entry in format: "PUBLIC_KEY_HEX|DEVICE_NAME|TIMESTAMP"
  QString entry = QString("%1|%2|%3")
                      .arg(QString::fromLatin1(publicKey.toHex()))
                      .arg(deviceName)
                      .arg(QDateTime::currentSecsSinceEpoch());

  // Remove existing entry for this device if present
  QString publicKeyHex = QString::fromLatin1(publicKey.toHex());
  for (int i = 0; i < recentDevices.size(); ++i) {
    if (recentDevices[i].startsWith(publicKeyHex + "|")) {
      recentDevices.removeAt(i);
      break;
    }
  }

  // Add new entry at the beginning
  recentDevices.prepend(entry);

  // Keep only the 10 most recent
  while (recentDevices.size() > 10) {
    recentDevices.removeLast();
  }

  m_settings.setValue(KEY_RECENT_DEVICES, recentDevices);
}

void SettingsManager::sync() { m_settings.sync(); }

} // namespace MeshCore
