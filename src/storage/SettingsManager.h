#pragma once

#include <QSettings>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QRect>

namespace MeshCore {

class SettingsManager {
public:
  static SettingsManager &instance();

  // Connection history
  QByteArray getLastDevicePublicKey();
  void setLastDevicePublicKey(const QByteArray &key);

  QString getLastConnectionType(); // "serial" or "ble"
  void setLastConnectionType(const QString &type);

  QString getLastConnectionTarget(); // port name or BLE device name
  void setLastConnectionTarget(const QString &target);

  bool getAutoConnect();
  void setAutoConnect(bool autoConnect);

  // Window geometry (future GUI)
  QRect getWindowGeometry();
  void setWindowGeometry(const QRect &geometry);

  // Display preferences
  bool getShowTimestamps();
  void setShowTimestamps(bool show);

  bool getShowSNR();
  void setShowSNR(bool show);

  QString getDateTimeFormat();
  void setDateTimeFormat(const QString &format);

  // Recent device list (up to 10 devices)
  QStringList getRecentDevices();
  void addRecentDevice(const QByteArray &publicKey, const QString &deviceName);

  // Sync settings to disk
  void sync();

private:
  SettingsManager();
  ~SettingsManager();
  SettingsManager(const SettingsManager &) = delete;
  SettingsManager &operator=(const SettingsManager &) = delete;

  QSettings m_settings;

  // Setting keys
  static constexpr const char *KEY_LAST_DEVICE = "connection/lastDevicePublicKey";
  static constexpr const char *KEY_LAST_TYPE = "connection/lastType";
  static constexpr const char *KEY_LAST_TARGET = "connection/lastTarget";
  static constexpr const char *KEY_AUTO_CONNECT = "connection/autoConnect";
  static constexpr const char *KEY_WINDOW_GEOMETRY = "ui/windowGeometry";
  static constexpr const char *KEY_SHOW_TIMESTAMPS = "display/showTimestamps";
  static constexpr const char *KEY_SHOW_SNR = "display/showSNR";
  static constexpr const char *KEY_DATETIME_FORMAT = "display/dateTimeFormat";
  static constexpr const char *KEY_RECENT_DEVICES = "connection/recentDevices";
};

} // namespace MeshCore
