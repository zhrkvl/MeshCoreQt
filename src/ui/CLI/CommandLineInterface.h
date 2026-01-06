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

  MeshClient *m_client;
  QTextStream m_input;
  QTextStream m_output;
  QSocketNotifier *m_notifier;
  bool m_running;
};

} // namespace MeshCore
