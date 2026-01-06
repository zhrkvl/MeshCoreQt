#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QTextStream>

#include "../../core/MeshClient.h"

namespace MeshCore {

class CommandLineInterface : public QObject {
  Q_OBJECT

public:
  explicit CommandLineInterface(MeshClient *client, QObject *parent = nullptr);

  void start();
  void printWelcome();

private slots:
  void processInput();
  void onChannelMessageReceived(const Message &msg);
  void onInitComplete();
  void onConnected();
  void onDisconnected();
  void onError(const QString &error);
  void onChannelDiscovered(const Channel &channel);
  void onNewMessageWaiting();
  void onNoMoreMessages();
  void onBLEDeviceFound(const BLEDeviceInfo &device);
  void onBLEDiscoveryFinished();

private:
  void printHelp();
  void printChannels();
  void printPrompt();
  void handleCommand(const QString &line);

  // Command handlers
  void cmdScan(const QStringList &args);
  void cmdConnect(const QStringList &args);
  void cmdDisconnect();
  void cmdInit();
  void cmdChannels();
  void cmdDiscover();
  void cmdJoin(const QStringList &args);
  void cmdSend(const QStringList &args);
  void cmdMsg(const QStringList &args);
  void cmdSync();
  void cmdConfigure(const QStringList &args);
  void cmdStatus();
  void cmdHelp();
  void cmdQuit();
  void cmdContacts(const QStringList &args);
  void cmdAdvert(const QStringList &args);
  void cmdSetName(const QStringList &args);
  void cmdSetLocation(const QStringList &args);

  // Helper methods for contacts command
  QString contactTypeToString(uint8_t type) const;
  QString formatPathLength(int8_t pathLen) const;
  void printContactDetails(const Contact &contact);

  MeshClient *m_client;
  QTextStream m_input;
  QTextStream m_output;
  QSocketNotifier *m_notifier;
  bool m_running;
};

} // namespace MeshCore
