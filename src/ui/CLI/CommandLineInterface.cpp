#include "CommandLineInterface.h"
#include "../../protocol/ProtocolConstants.h"
#include <QBluetoothLocalDevice>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
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
  connect(m_client, &MeshClient::contactMessageReceived, this,
          &CommandLineInterface::onContactMessageReceived);
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
  connect(m_client, &MeshClient::bleDeviceFound, this,
          &CommandLineInterface::onBLEDeviceFound);
  connect(m_client, &MeshClient::bleDiscoveryFinished, this,
          &CommandLineInterface::onBLEDiscoveryFinished);

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

#ifdef Q_OS_MACOS
  // BLE doesn't work on macOS with Qt CLI apps due to permission plugin limitation
  m_output << "  scan serial              - Scan for USB serial devices\n";
  m_output << "  connect <port>           - Connect to serial device\n";
  m_output << "                             Example: /dev/cu.usbserial-0001\n";
#else
  m_output << "  scan [type]              - Scan for devices\n";
  m_output << "                             Types: all (default), serial, ble\n";
  m_output << "                             Examples:\n";
  m_output << "                               scan          (scan everything)\n";
  m_output << "                               scan serial   (USB ports only)\n";
  m_output << "                               scan ble      (Bluetooth only)\n";
  m_output << "  connect <target>         - Connect to device\n";
  m_output << "                             Serial: /dev/ttyUSB0, COM3\n";
  m_output << "                             BLE: ble:DeviceName or ble:MAC\n";
#endif

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
  m_output << "  contacts [options] [pubkey] - List contacts or show contact details\n";
  m_output << "                             Options: --minimal, --sort=name|time|type, --type=chat|repeater|room\n";
  m_output << "                             Example: contacts --minimal --type=chat\n";
  m_output << "  advert [flood]           - Advertise presence to nearby nodes\n";
  m_output << "                             Example: advert (direct only) or advert flood (multi-hop)\n";
  m_output << "  set_name <name>          - Set advertised node name\n";
  m_output << "  set_location <lat> <lon> - Set GPS location for adverts\n";
  m_output << "                             Example: set_location 51.5074 -0.1278\n";
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

  if (cmd == "scan") {
    cmdScan(args);
  } else if (cmd == "connect") {
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
  } else if (cmd == "contacts") {
    cmdContacts(args);
  } else if (cmd == "advert") {
    cmdAdvert(args);
  } else if (cmd == "set_name") {
    cmdSetName(args);
  } else if (cmd == "set_location") {
    cmdSetLocation(args);
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
#ifndef Q_OS_MACOS
    m_output << "\nBLE examples:\n";
    m_output << "  connect ble:MyMeshDevice      (by device name)\n";
    m_output << "  connect ble:AA:BB:CC:DD:EE:FF (by MAC address)\n";
#endif
    m_output.flush();
    return;
  }

  QString target = args[0];

  // Check if BLE connection
  if (target.startsWith("ble:", Qt::CaseInsensitive)) {
#ifdef Q_OS_MACOS
    m_output << "\n";
    m_output << "╔════════════════════════════════════════╗\n";
    m_output << "║  BLE Not Available on macOS            ║\n";
    m_output << "╚════════════════════════════════════════╝\n";
    m_output << "\n";
    m_output << "Bluetooth LE is not supported in Qt CLI apps on macOS\n";
    m_output << "due to permission plugin requirements.\n";
    m_output << "\n";
    m_output << "Please use serial USB connections instead:\n";
    m_output << "  scan serial     (find devices)\n";
    m_output << "  connect <port>  (connect to device)\n";
    m_output << "\n";
    m_output << "For details, see: BLE_MACOS_NOTES.md\n";
    m_output << "Tracked in issue: MeshCoreQt-wwi\n";
    m_output << "\n";
    m_output.flush();
    return;
#else
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
#endif
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

void CommandLineInterface::cmdContacts(const QStringList &args) {
  if (!m_client->isInitialized()) {
    m_output << "Error: Not initialized. Use 'init' first.\n";
    m_output.flush();
    return;
  }

  // Parse command-line options
  bool minimal = false;
  QString sortField = "name";  // default
  QString typeFilter;          // empty = show all
  QString pubkeyPrefix;

  for (const QString &arg : args) {
    if (arg == "--minimal" || arg == "-m") {
      minimal = true;
    } else if (arg.startsWith("--sort=")) {
      sortField = arg.mid(7).toLower();  // "name", "time", or "type"
      if (sortField != "name" && sortField != "time" && sortField != "type") {
        m_output << "Error: Invalid sort field '" << sortField << "'\n";
        m_output << "Valid options: name, time, type\n";
        m_output.flush();
        return;
      }
    } else if (arg.startsWith("--type=")) {
      typeFilter = arg.mid(7).toLower();  // "chat", "repeater", "room", "none"
      if (typeFilter != "chat" && typeFilter != "repeater" &&
          typeFilter != "room" && typeFilter != "none") {
        m_output << "Error: Invalid type filter '" << typeFilter << "'\n";
        m_output << "Valid options: chat, repeater, room, none\n";
        m_output.flush();
        return;
      }
    } else if (!arg.startsWith("-")) {
      pubkeyPrefix = arg.toLower();  // non-option arg is pubkey prefix
    }
  }

  QVector<Contact> contacts = m_client->getContacts();

  // If pubkey prefix provided, show detailed view
  if (!pubkeyPrefix.isEmpty()) {
    Contact foundContact;
    bool found = false;

    for (const Contact &contact : contacts) {
      QString fullKey = contact.publicKeyHex().toLower();
      if (fullKey.startsWith(pubkeyPrefix)) {
        foundContact = contact;
        found = true;
        break;
      }
    }

    if (!found) {
      m_output << "Contact not found with public key prefix: " << pubkeyPrefix << "\n";
      m_output << "Use 'contacts' to list all contacts.\n";
      m_output.flush();
      return;
    }

    printContactDetails(foundContact);
    return;
  }

  // Filter contacts by type if specified
  QVector<Contact> filtered;
  for (const Contact &c : contacts) {
    if (typeFilter.isEmpty() ||
        (typeFilter == "chat" && c.type() == static_cast<uint8_t>(ContactType::CHAT)) ||
        (typeFilter == "repeater" && c.type() == static_cast<uint8_t>(ContactType::REPEATER)) ||
        (typeFilter == "room" && c.type() == static_cast<uint8_t>(ContactType::ROOM)) ||
        (typeFilter == "none" && c.type() == static_cast<uint8_t>(ContactType::NONE))) {
      filtered.append(c);
    }
  }

  // Check if filtering resulted in empty list
  if (filtered.isEmpty()) {
    if (contacts.isEmpty()) {
      m_output << "No contacts available.\n";
      m_output << "Contacts are retrieved during initialization.\n";
      m_output << "New contacts may appear when you receive messages from them.\n";
    } else {
      m_output << "No contacts match the filter.\n";
      m_output << "Use 'contacts' to list all contacts.\n";
    }
    m_output.flush();
    return;
  }

  // Sort contacts
  std::sort(filtered.begin(), filtered.end(), [&sortField](const Contact &a, const Contact &b) {
    if (sortField == "name") {
      return a.name().toLower() < b.name().toLower();
    } else if (sortField == "time") {
      return a.lastModified() > b.lastModified();  // most recent first
    } else if (sortField == "type") {
      return a.type() < b.type();
    }
    return false;
  });

  // Display contacts
  if (minimal) {
    // Minimal view
    m_output << "Contacts:\n";
    for (int i = 0; i < filtered.size(); ++i) {
      const Contact &contact = filtered[i];
      m_output << "  [" << (i + 1) << "] " << contact.name() << " ("
               << contactTypeToString(contact.type()) << ")\n";
      m_output << "      " << contact.publicKeyHex() << "\n";
    }
    m_output << "\n";
    m_output << "Total: " << filtered.size() << " contact(s)\n";
  } else {
    // Full view
    m_output << "Available contacts:\n";
    m_output << "──────────────────────────────────────────────────────────────────────\n";

    for (int i = 0; i < filtered.size(); ++i) {
      const Contact &contact = filtered[i];

      m_output << "[" << (i + 1) << "] " << contact.name() << " ("
               << contactTypeToString(contact.type()) << ")\n";

      m_output << "    PubKey: " << contact.publicKeyHex() << "\n";

      m_output << "    Path:   " << formatPathLength(contact.pathLength()) << "\n";

      if (contact.lastModified() > 0) {
        QDateTime lastSeen = QDateTime::fromSecsSinceEpoch(contact.lastModified());
        m_output << "    Last seen: " << lastSeen.toString("yyyy-MM-dd HH:mm:ss") << "\n";
      } else {
        m_output << "    Last seen: Never\n";
      }

      m_output << "\n";
    }

    m_output << "──────────────────────────────────────────────────────────────────────\n";
    m_output << "Total: " << filtered.size() << " contact(s)\n";
    m_output << "\n";
    m_output << "For details: contacts <pubkey>\n";
    m_output << "To message:  msg <pubkey> <message>\n";
  }

  m_output.flush();
}

void CommandLineInterface::cmdAdvert(const QStringList &args) {
  if (!m_client->isInitialized()) {
    m_output << "Error: Not initialized. Use 'init' first.\n";
    m_output.flush();
    return;
  }

  bool floodMode = false;
  if (!args.isEmpty() && args[0].toLower() == "flood") {
    floodMode = true;
  }

  m_output << "Sending advertisement"
           << (floodMode ? " (flood mode - multi-hop)..." : " (direct only)...")
           << "\n";
  m_output.flush();

  m_client->sendSelfAdvert(floodMode);

  m_output << "Advertisement sent. Nearby nodes should discover you now.\n";
  m_output << "Tip: Use 'contacts' to see who's discovered you.\n";
  m_output.flush();
}

void CommandLineInterface::cmdSetName(const QStringList &args) {
  if (args.isEmpty()) {
    m_output << "Usage: set_name <name>\n";
    m_output << "Example: set_name MyMeshNode\n";
    m_output << "Note: Set your name before advertising to be recognized.\n";
    m_output.flush();
    return;
  }

  QString name = args.join(' ');

  // Limit to 32 characters (protocol constraint)
  if (name.length() > 32) {
    m_output << "Warning: Name too long (max 32 chars), truncating...\n";
    name = name.left(32);
  }

  m_output << "Setting node name: " << name << "\n";
  m_output.flush();

  m_client->setAdvertName(name);

  m_output << "Name set. Use 'advert' to broadcast your presence.\n";
  m_output.flush();
}

void CommandLineInterface::cmdSetLocation(const QStringList &args) {
  if (args.size() < 2) {
    m_output << "Usage: set_location <latitude> <longitude>\n";
    m_output << "Example: set_location 51.5074 -0.1278\n";
    m_output << "Note: Coordinates in decimal degrees\n";
    m_output << "      Latitude: -90 to 90 (N positive, S negative)\n";
    m_output << "      Longitude: -180 to 180 (E positive, W negative)\n";
    m_output.flush();
    return;
  }

  bool okLat, okLon;
  double lat = args[0].toDouble(&okLat);
  double lon = args[1].toDouble(&okLon);

  if (!okLat || !okLon) {
    m_output << "Error: Invalid coordinates. Must be decimal numbers.\n";
    m_output.flush();
    return;
  }

  // Validate ranges
  if (lat < -90.0 || lat > 90.0) {
    m_output << "Error: Latitude must be between -90 and 90\n";
    m_output.flush();
    return;
  }

  if (lon < -180.0 || lon > 180.0) {
    m_output << "Error: Longitude must be between -180 and 180\n";
    m_output.flush();
    return;
  }

  m_output << "Setting location: " << lat << ", " << lon << "\n";
  m_output.flush();

  m_client->setAdvertLocation(lat, lon);

  m_output << "Location set. Use 'advert' to broadcast your position.\n";
  m_output.flush();
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

void CommandLineInterface::onContactMessageReceived(const Message &msg) {
  // Try to resolve sender name from contacts
  QString senderDisplay = msg.senderPubKeyPrefix.toHex();
  QString senderName;
  QVector<Contact> contacts = m_client->getContacts();

  qDebug() << "Resolving sender" << senderDisplay << "from" << contacts.size() << "contacts";

  for (const Contact &contact : contacts) {
    if (contact.publicKey().startsWith(msg.senderPubKeyPrefix)) {
      senderName = contact.name();
      qDebug() << "Found matching contact:" << senderName;

      if (!senderName.isEmpty()) {
        senderDisplay = QString("%1 (%2)").arg(senderName, msg.senderPubKeyPrefix.toHex());
      } else {
        // Contact exists but has no name, show full pubkey instead
        senderDisplay = QString("%1 (unnamed)").arg(contact.publicKeyHex());
      }
      break;
    }
  }

  if (senderName.isEmpty()) {
    qDebug() << "No matching contact found for" << senderDisplay;
    // Show as "Unknown Contact (prefix)"
    senderDisplay = QString("Unknown Contact (%1)").arg(senderDisplay);
  }

  m_output << "\n";
  m_output
      << "╔══════════════════════════════════════════════════════════════\n";
  m_output << "║ Direct Message from: " << senderDisplay << "\n";
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
  m_output << "\n";
  m_output << "To reply: msg " << msg.senderPubKeyPrefix.toHex()
           << " <your message>\n";
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
  m_output << "Initializing device...\n";
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

void CommandLineInterface::cmdScan(const QStringList &args) {
#ifdef Q_OS_MACOS
  // On macOS, only serial scanning works due to Qt permission plugin limitation
  QString type = "serial";

  if (!args.isEmpty()) {
    type = args[0].toLower();

    if (type == "ble" || type == "all") {
      m_output << "\n";
      m_output << "╔════════════════════════════════════════╗\n";
      m_output << "║  BLE Not Available on macOS            ║\n";
      m_output << "╚════════════════════════════════════════╝\n";
      m_output << "\n";
      m_output << "Bluetooth LE is not supported in Qt CLI apps on macOS\n";
      m_output << "due to permission plugin requirements.\n";
      m_output << "\n";
      m_output << "Please use serial USB connections instead:\n";
      m_output << "  scan serial\n";
      m_output << "\n";
      m_output << "For details, see: BLE_MACOS_NOTES.md\n";
      m_output << "Tracked in issue: MeshCoreQt-wwi\n";
      m_output << "\n";
      m_output.flush();
      return;
    }

    if (type != "serial") {
      m_output << "Invalid scan type. Use: scan serial\n";
      m_output.flush();
      return;
    }
  }
#else
  QString type = "all"; // default to scanning everything

  if (!args.isEmpty()) {
    type = args[0].toLower();

    if (type != "all" && type != "serial" && type != "ble") {
      m_output << "Invalid scan type. Use: scan [all|serial|ble]\n";
      m_output.flush();
      return;
    }
  }
#endif

  // Scan serial ports
  if (type == "serial" || type == "all") {
    m_output << "\n";
    m_output << "╔════════════════════════════════════════╗\n";
    m_output << "║          Serial Ports                  ║\n";
    m_output << "╚════════════════════════════════════════╝\n";
    m_output << "\n";
    m_output.flush();

    m_client->scanSerialPorts();
    QList<SerialPortInfo> ports = m_client->getAvailableSerialPorts();

    if (ports.isEmpty()) {
      m_output << "No serial ports found.\n";
      m_output << "\n";
      m_output << "Troubleshooting:\n";
      m_output << "  • Make sure your device is connected via USB\n";
      m_output << "  • Check that USB-Serial drivers are installed\n";
      m_output
          << "  • Linux: Add user to 'dialout' group (sudo usermod -a -G "
             "dialout $USER)\n";
      m_output
          << "  • macOS: Check System Information > USB for device\n";
      m_output << "  • Windows: Check Device Manager for COM ports\n";
      m_output << "\n";
    } else {
      m_output << "Found " << ports.size() << " port(s):\n\n";

      for (const SerialPortInfo &port : ports) {
        m_output << "─────────────────────────────────────────\n";
        m_output << "Port:         " << port.portName << "\n";

        if (!port.description.isEmpty()) {
          m_output << "Description:  " << port.description << "\n";
        }

        if (!port.manufacturer.isEmpty()) {
          m_output << "Manufacturer: " << port.manufacturer << "\n";
        }

        if (!port.serialNumber.isEmpty()) {
          m_output << "Serial #:     " << port.serialNumber << "\n";
        }

        QString usbId = port.usbIdString();
        if (!usbId.isEmpty()) {
          m_output << "USB VID:PID:  " << usbId << "\n";
        }

        // Highlight likely MeshCore devices
        bool isMeshCore = SerialConnection::isMeshCoreDevice(port);
        if (isMeshCore) {
          m_output << "\n*** Likely MeshCore-compatible device ***\n";
        }

        m_output << "\nTo connect:   connect " << port.portName << "\n";
      }

      m_output << "─────────────────────────────────────────\n\n";
    }

    m_output.flush();
  }

  // Scan BLE devices
  if (type == "ble" || type == "all") {
    m_output << "\n";
    m_output << "╔════════════════════════════════════════╗\n";
    m_output << "║     Bluetooth Low Energy Devices       ║\n";
    m_output << "╚════════════════════════════════════════╝\n";
    m_output << "\n";

    // Check if Bluetooth is available
    if (QBluetoothLocalDevice::allDevices().isEmpty()) {
      m_output << "Error: Bluetooth not available on this system.\n";
      m_output << "\n";
      m_output << "Possible reasons:\n";
      m_output << "  • Bluetooth hardware not present\n";
      m_output << "  • Bluetooth turned off in system settings\n";
      m_output << "  • Missing Bluetooth permissions\n";
      m_output << "\n";
      m_output.flush();
    } else {
      m_output << "Scanning for BLE devices (5 seconds)...\n";
      m_output << "\n";
      m_output.flush();

      // Filter MeshCore devices if scanning "all" (to reduce noise)
      // Show all devices if specifically scanning "ble"
      bool filterMeshCore = (type == "all");
      m_client->scanBLEDevices(filterMeshCore);

      // Results will come via signals (onBLEDeviceFound,
      // onBLEDiscoveryFinished)
    }
  }
}

void CommandLineInterface::onBLEDeviceFound(const BLEDeviceInfo &device) {
  m_output << "─────────────────────────────────────────\n";

  QString displayName = device.displayName();
  m_output << "Device:  " << displayName << "\n";

  if (!device.name.isEmpty()) {
    m_output << "Address: " << device.address << "\n";
  }

  m_output << "RSSI:    " << device.rssiString() << "\n";

  if (device.hasMeshCoreService) {
    m_output << "\n*** MeshCore UART Service detected ***\n";
  } else {
    m_output << "\nNote: Service UUID not advertised (may still be "
                "compatible)\n";
  }

  QString connectTarget =
      device.name.isEmpty() ? device.address : device.name;
  m_output << "\nTo connect: connect ble:" << connectTarget << "\n";

  m_output.flush();
}

void CommandLineInterface::onBLEDiscoveryFinished() {
  QList<BLEDeviceInfo> devices = m_client->getDiscoveredBLEDevices();

  m_output << "─────────────────────────────────────────\n";
  m_output << "\n";
  m_output << "BLE scan complete. Found " << devices.size()
           << " device(s).\n";

  if (devices.isEmpty()) {
    m_output << "\n";
    m_output << "Troubleshooting:\n";
    m_output << "  • Make sure your MeshCore device is powered on\n";
    m_output << "  • Check device is in pairing/advertising mode\n";
    m_output << "  • Move closer to the device\n";
    m_output
        << "  • Try 'scan ble' to see all BLE devices (not just MeshCore)\n";
    m_output << "\n";
  }

  m_output.flush();
  printPrompt();
}

QString CommandLineInterface::contactTypeToString(uint8_t type) const {
  switch (type) {
    case static_cast<uint8_t>(ContactType::NONE):
      return "NONE";
    case static_cast<uint8_t>(ContactType::CHAT):
      return "CHAT";
    case static_cast<uint8_t>(ContactType::REPEATER):
      return "REPEATER";
    case static_cast<uint8_t>(ContactType::ROOM):
      return "ROOM";
    default:
      return QString("UNKNOWN(%1)").arg(type);
  }
}

QString CommandLineInterface::formatPathLength(int8_t pathLen) const {
  if (pathLen == PATH_LEN_FLOOD) {
    return "Flood routing";
  } else if (pathLen == static_cast<int8_t>(PATH_LEN_DIRECT)) {
    return "Direct connection";
  } else if (pathLen >= 0) {
    return QString("%1 hop%2").arg(pathLen).arg(pathLen == 1 ? "" : "s");
  } else {
    return QString("Unknown (%1)").arg(pathLen);
  }
}

void CommandLineInterface::printContactDetails(const Contact &contact) {
  m_output << "Contact Details:\n";
  m_output << "╔════════════════════════════════════════════════════════════════\n";
  m_output << "║ Name:        " << contact.name() << "\n";
  m_output << "║ Type:        " << contactTypeToString(contact.type()) << "\n";
  m_output << "║ Public Key:  " << contact.publicKeyHex() << "\n";
  m_output << "║ Flags:       0x" << QString::number(contact.flags(), 16).rightJustified(2, '0').toUpper() << "\n";
  m_output << "║\n";
  m_output << "║ Routing:\n";
  m_output << "║   Path Length:    " << formatPathLength(contact.pathLength()) << "\n";

  if (!contact.path().isEmpty()) {
    m_output << "║   Path:           " << contact.path().toHex() << "\n";
  }

  m_output << "║\n";
  m_output << "║ Timestamps:\n";

  if (contact.lastAdvertTimestamp() > 0) {
    QDateTime lastAdvert = QDateTime::fromSecsSinceEpoch(contact.lastAdvertTimestamp());
    m_output << "║   Last Advert:    " << contact.lastAdvertTimestamp()
             << " (" << lastAdvert.toString("yyyy-MM-dd HH:mm:ss") << ")\n";
  } else {
    m_output << "║   Last Advert:    Never\n";
  }

  if (contact.lastModified() > 0) {
    QDateTime lastMod = QDateTime::fromSecsSinceEpoch(contact.lastModified());
    m_output << "║   Last Modified:  " << contact.lastModified()
             << " (" << lastMod.toString("yyyy-MM-dd HH:mm:ss") << ")\n";
  } else {
    m_output << "║   Last Modified:  Never\n";
  }

  m_output << "║\n";
  m_output << "║ Location:\n";

  if (contact.latitude() != 0 || contact.longitude() != 0) {
    double lat = contact.latitude() / 1000000.0;
    double lon = contact.longitude() / 1000000.0;
    QString latDir = (lat >= 0) ? "N" : "S";
    QString lonDir = (lon >= 0) ? "E" : "W";

    m_output << "║   Latitude:       " << QString::number(qAbs(lat), 'f', 6) << "° " << latDir << "\n";
    m_output << "║   Longitude:      " << QString::number(qAbs(lon), 'f', 6) << "° " << lonDir << "\n";
  } else {
    m_output << "║   Location:       Not available\n";
  }

  m_output << "╚════════════════════════════════════════════════════════════════\n";
  m_output << "\n";
  m_output << "To send message: msg " << contact.publicKeyHex() << " <your message>\n";
  m_output.flush();
}

} // namespace MeshCore
