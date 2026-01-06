#include "CommandLineInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <iostream>

namespace MeshCore {

CommandLineInterface::CommandLineInterface(MeshClient *client, QObject *parent)
    : QObject(parent), m_client(client), m_input(stdin), m_output(stdout),
      m_notifier(
          new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this)),
      m_running(true) {
  // Connect MeshClient signals
  connect(m_client, &MeshClient::channelMessageReceived, this,
          &CommandLineInterface::onChannelMessageReceived);
  connect(m_client, &MeshClient::initializationComplete, this,
          &CommandLineInterface::onInitComplete);
  connect(m_client, &MeshClient::connected, this,
          &CommandLineInterface::onConnected);
  connect(m_client, &MeshClient::disconnected, this,
          &CommandLineInterface::onDisconnected);
  connect(m_client, &MeshClient::errorOccurred, this,
          &CommandLineInterface::onError);
  connect(m_client, &MeshClient::channelDiscovered, this,
          &CommandLineInterface::onChannelDiscovered);
  connect(m_client, &MeshClient::newMessageWaiting, this,
          &CommandLineInterface::onNewMessageWaiting);
  connect(m_client, &MeshClient::noMoreMessages, this,
          &CommandLineInterface::onNoMoreMessages);

  // Connect input notifier
  connect(m_notifier, &QSocketNotifier::activated, this,
          &CommandLineInterface::processInput);
}

void CommandLineInterface::start() {
  printWelcome();
  printHelp();
  printPrompt();
}

void CommandLineInterface::printWelcome() {
  m_output << "\n";
  m_output << "╔════════════════════════════════════════╗\n";
  m_output << "║     MeshCore Qt Client v1.0.0          ║\n";
  m_output << "║  LoRa Mesh Network Communication       ║\n";
  m_output << "╚════════════════════════════════════════╝\n";
  m_output << "\n";
  m_output.flush();
}

void CommandLineInterface::printHelp() {
  m_output << "Commands:\n";
  m_output << "  connect <target>         - Connect to device\n";
  m_output << "                             Serial: /dev/ttyUSB0, COM3\n";
  m_output << "                             BLE: ble:DeviceName or ble:MAC\n";
  m_output << "  disconnect               - Disconnect from device\n";
  m_output << "  init                     - Run initialization sequence\n";
  m_output << "  configure <preset>       - Set radio preset (eu_uk_narrow, "
              "usa_canada_narrow, etc.)\n";
  m_output << "  channels                 - List available channels\n";
  m_output << "  discover                 - Discover custom channels\n";
  m_output
      << "  join <name> <psk>        - Join channel with name and PSK (hex)\n";
  m_output << "  send <channel> <message> - Send message to channel\n";
  m_output << "  msg <pubkey> <message>   - Send direct message to contact (pubkey is hex)\n";
  m_output << "  sync                     - Pull next message from queue\n";
  m_output << "  status                   - Show connection status\n";
  m_output << "  help                     - Show this help\n";
  m_output << "  quit                     - Exit application\n";
  m_output << "\n";
  m_output << "Available radio presets:\n";
  m_output << "  eu_uk_narrow             - EU/UK Narrow (869.618 MHz, BW62.5, "
              "SF8, CR8)\n";
  m_output << "  eu_uk_wide               - EU/UK Wide (868.0 MHz, BW125, "
              "SF11, CR8)\n";
  m_output << "  usa_canada_narrow        - USA/Canada Narrow (910.525 MHz, "
              "BW62.5, SF7, CR8)\n";
  m_output << "  usa_canada_wide          - USA/Canada Wide (915.0 MHz, BW125, "
              "SF11, CR8)\n";
  m_output << "  australia_nz_narrow      - Australia/NZ (915.8 MHz, BW62.5, "
              "SF8, CR8)\n";
  m_output << "  asia_433                 - Asia 433MHz (433.0 MHz, BW62.5, "
              "SF9, CR8)\n";
  m_output << "\n";
  m_output.flush();
}

void CommandLineInterface::printPrompt() {
  m_output << "> ";
  m_output.flush();
}

void CommandLineInterface::processInput() {
  QString line = m_input.readLine().trimmed();

  if (line.isEmpty()) {
    printPrompt();
    return;
  }

  handleCommand(line);
  printPrompt();
}

void CommandLineInterface::handleCommand(const QString &line) {
  QStringList parts = line.split(' ', Qt::SkipEmptyParts);
  if (parts.isEmpty())
    return;

  QString cmd = parts[0].toLower();
  QStringList args = parts.mid(1);

  if (cmd == "connect") {
    cmdConnect(args);
  } else if (cmd == "disconnect") {
    cmdDisconnect();
  } else if (cmd == "init") {
    cmdInit();
  } else if (cmd == "configure" || cmd == "config") {
    cmdConfigure(args);
  } else if (cmd == "channels") {
    cmdChannels();
  } else if (cmd == "discover") {
    cmdDiscover();
  } else if (cmd == "join") {
    cmdJoin(args);
  } else if (cmd == "send") {
    cmdSend(args);
  } else if (cmd == "msg" || cmd == "message") {
    cmdMsg(args);
  } else if (cmd == "sync") {
    cmdSync();
  } else if (cmd == "status") {
    cmdStatus();
  } else if (cmd == "help") {
    cmdHelp();
  } else if (cmd == "quit" || cmd == "exit") {
    cmdQuit();
  } else {
    m_output << "Unknown command: " << cmd << "\n";
    m_output << "Type 'help' for available commands.\n";
    m_output.flush();
  }
}

void CommandLineInterface::cmdConnect(const QStringList &args) {
  if (args.isEmpty()) {
    m_output << "Usage: connect <target>\n";
    m_output << "Serial examples:\n";
    m_output << "  Linux:   connect /dev/ttyUSB0\n";
    m_output << "  macOS:   connect /dev/cu.usbserial-*\n";
    m_output << "  Windows: connect COM3\n";
    m_output << "\nBLE examples:\n";
    m_output << "  connect ble:MyMeshDevice      (by device name)\n";
    m_output << "  connect ble:AA:BB:CC:DD:EE:FF (by MAC address)\n";
    m_output.flush();
    return;
  }

  QString target = args[0];

  // Check if BLE connection
  if (target.startsWith("ble:", Qt::CaseInsensitive)) {
    QString deviceIdentifier = target.mid(4); // Remove "ble:" prefix
    m_output << "Connecting to BLE device: " << deviceIdentifier << "...\n";
    m_output << "Discovery may take a few seconds...\n";
    m_output.flush();

    if (m_client->connectToBLEDevice(deviceIdentifier)) {
      // Wait for connected signal
      m_output << "BLE discovery started. Waiting for device...\n";
      m_output.flush();
    } else {
      m_output << "Failed to start BLE connection to " << deviceIdentifier << "\n";
      m_output.flush();
    }
  } else {
    // Serial connection
    m_output << "Connecting to serial port: " << target << "...\n";
    m_output.flush();

    if (m_client->connectToSerialDevice(target, 115200)) {
      // Wait for connected signal
    } else {
      m_output << "Failed to connect to " << target << "\n";
      m_output.flush();
    }
  }
}

void CommandLineInterface::cmdDisconnect() {
  m_client->disconnect();
  m_output << "Disconnected.\n";
  m_output.flush();
}

void CommandLineInterface::cmdInit() {
  if (!m_client->isConnected()) {
    m_output << "Error: Not connected. Use 'connect <port>' first.\n";
    m_output.flush();
    return;
  }

  m_output << "Starting initialization sequence...\n";
  m_output.flush();
  m_client->startInitSequence();
}

void CommandLineInterface::cmdChannels() {
  QVector<Channel> channels = m_client->getChannels();

  if (channels.isEmpty()) {
    m_output << "No channels available.\n";
    m_output.flush();
    return;
  }

  m_output << "Available channels:\n";
  for (const Channel &ch : channels) {
    m_output << "  [" << ch.index << "] " << ch.name << "\n";
  }
  m_output.flush();
}

void CommandLineInterface::cmdDiscover() {
  if (!m_client->isInitialized()) {
    m_output << "Error: Not initialized. Use 'init' first.\n";
    m_output.flush();
    return;
  }

  m_output << "Discovering channels...\n";
  m_output.flush();
  m_client->discoverChannels();
}

void CommandLineInterface::cmdJoin(const QStringList &args) {
  if (args.size() < 2) {
    m_output << "Usage: join <name> <psk>\n";
    m_output << "Example: join KKFamily f88f2184e0d7b7cc88f471cf61bd5b0a\n";
    m_output << "\n";
    m_output << "The PSK should be a 32 or 64 character hex string (16 or 32 "
                "bytes).\n";
    m_output.flush();
    return;
  }

  if (!m_client->isInitialized()) {
    m_output << "Error: Not initialized. Use 'init' first.\n";
    m_output.flush();
    return;
  }

  QString name = args[0];
  QString pskHex = args[1];

  m_output << "Joining channel '" << name << "'...\n";
  m_output.flush();

  m_client->joinChannel(name, pskHex);
  m_output << "Channel configured. Use 'channels' to see all channels.\n";
  m_output.flush();
}

void CommandLineInterface::cmdSend(const QStringList &args) {
  if (args.size() < 2) {
    m_output << "Usage: send <channel_idx> <message>\n";
    m_output << "Example: send 0 Hello from MeshCoreQt!\n";
    m_output.flush();
    return;
  }

  bool ok;
  uint8_t channelIdx = args[0].toUInt(&ok);
  if (!ok) {
    m_output << "Error: Invalid channel index: " << args[0] << "\n";
    m_output.flush();
    return;
  }

  QString message = args.mid(1).join(' ');
  m_output << "Sending to channel " << channelIdx << ": " << message << "\n";
  m_output.flush();

  m_client->sendChannelMessage(channelIdx, message);
}

void CommandLineInterface::cmdMsg(const QStringList &args) {
  if (args.size() < 2) {
    m_output << "Usage: msg <pubkey_hex> <message>\n";
    m_output << "Example: msg abc123def456 Hello from MeshCoreQt!\n";
    m_output << "Note: pubkey_hex is the first 6+ bytes of recipient's public key in hex\n";
    m_output.flush();
    return;
  }

  if (!m_client->isInitialized()) {
    m_output << "Error: Not initialized. Use 'init' first.\n";
    m_output.flush();
    return;
  }

  QString pubKeyHex = args[0];
  QByteArray pubKey = QByteArray::fromHex(pubKeyHex.toUtf8());

  if (pubKey.size() < 6) {
    m_output << "Error: Public key too short (need at least 6 bytes / 12 hex chars)\n";
    m_output.flush();
    return;
  }

  QString message = args.mid(1).join(' ');
  m_output << "Sending direct message to " << pubKey.left(6).toHex() << ": " << message << "\n";
  m_output.flush();

  m_client->sendDirectMessage(pubKey, message);
}

void CommandLineInterface::cmdSync() {
  if (!m_client->isInitialized()) {
    m_output << "Error: Not initialized. Use 'init' first.\n";
    m_output.flush();
    return;
  }

  m_output << "Checking for messages...\n";
  m_output.flush();
  m_client->syncNextMessage();
}

void CommandLineInterface::cmdConfigure(const QStringList &args) {
  if (args.isEmpty()) {
    m_output << "Usage: configure <preset>\n";
    m_output << "Available presets:\n";
    m_output << "  eu_uk_narrow        - 869.618 MHz, BW62.5, SF8, CR8\n";
    m_output << "  eu_uk_wide          - 868.0 MHz, BW125, SF11, CR8\n";
    m_output << "  usa_canada_narrow   - 910.525 MHz, BW62.5, SF7, CR8\n";
    m_output << "  usa_canada_wide     - 915.0 MHz, BW125, SF11, CR8\n";
    m_output << "  australia_nz_narrow - 915.8 MHz, BW62.5, SF8, CR8\n";
    m_output << "  asia_433            - 433.0 MHz, BW62.5, SF9, CR8\n";
    m_output << "\nRun 'help' to see more information.\n";
    m_output.flush();
    return;
  }

  if (!m_client->isConnected()) {
    m_output << "Error: Not connected. Use 'connect <port>' first.\n";
    m_output.flush();
    return;
  }

  QString preset = args[0].toLower();
  m_output << "Setting radio preset: " << preset << "...\n";
  m_output.flush();

  m_client->setRadioPreset(preset);
}

void CommandLineInterface::cmdStatus() {
  m_output << "Status:\n";
  m_output << "  Connected: " << (m_client->isConnected() ? "Yes" : "No")
           << "\n";
  m_output << "  Initialized: " << (m_client->isInitialized() ? "Yes" : "No")
           << "\n";

  if (m_client->isInitialized()) {
    DeviceInfo info = m_client->getDeviceInfo();
    m_output << "  Device: " << info.firmwareName << "\n";
    m_output << "  Channels: " << m_client->getChannels().size() << "\n";
  }

  m_output.flush();
}

void CommandLineInterface::cmdHelp() { printHelp(); }

void CommandLineInterface::cmdQuit() {
  m_output << "Goodbye!\n";
  m_output.flush();
  m_client->disconnect();
  QCoreApplication::quit();
}

// Signal handlers
void CommandLineInterface::onChannelMessageReceived(const Message &msg) {
  m_output << "\n";
  m_output
      << "╔══════════════════════════════════════════════════════════════\n";
  m_output << "║ Message from: " << msg.senderName << "\n";
  m_output << "║ Channel: " << msg.channelIdx << "\n";
  m_output << "║ Time: " << msg.receivedAt.toString("yyyy-MM-dd HH:mm:ss")
           << "\n";
  m_output << "║ Signal: SNR " << QString::number(msg.snr, 'f', 1) << " dB, ";
  m_output << "Hops "
           << (msg.pathLen == 0xFF ? "direct" : QString::number(msg.pathLen))
           << "\n";
  m_output
      << "╠══════════════════════════════════════════════════════════════\n";
  m_output << "║ " << msg.text << "\n";
  m_output
      << "╚══════════════════════════════════════════════════════════════\n";
  m_output.flush();
  printPrompt();
}

void CommandLineInterface::onInitComplete() {
  m_output << "Initialization complete!\n";
  DeviceInfo info = m_client->getDeviceInfo();
  m_output << "Device: " << info.firmwareName << "\n";
  m_output.flush();
}

void CommandLineInterface::onConnected() {
  m_output << "Connected successfully!\n";
  m_output << "Run 'init' to initialize the device.\n";
  m_output.flush();
}

void CommandLineInterface::onDisconnected() {
  // Already handled in cmdDisconnect
}

void CommandLineInterface::onError(const QString &error) {
  m_output << "Error: " << error << "\n";
  m_output.flush();
}

void CommandLineInterface::onChannelDiscovered(const Channel &channel) {
  m_output << "Channel discovered: [" << channel.index << "] " << channel.name
           << "\n";
  m_output.flush();
}

void CommandLineInterface::onNewMessageWaiting() {
  // Automatically sync the waiting message
  m_client->syncNextMessage();
}

void CommandLineInterface::onNoMoreMessages() {
  m_output << "No messages in queue.\n";
  m_output.flush();
}

void CommandLineInterface::printChannels() { cmdChannels(); }

} // namespace MeshCore
