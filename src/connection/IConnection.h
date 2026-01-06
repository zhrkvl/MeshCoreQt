#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include "ConnectionState.h"

namespace MeshCore {

class IConnection : public QObject {
  Q_OBJECT

public:
  explicit IConnection(QObject *parent = nullptr) : QObject(parent) {}
  virtual ~IConnection() = default;

  // Core connection operations
  virtual bool open(const QString &target) = 0;
  virtual void close() = 0;
  virtual bool isOpen() const = 0;

  // Data transmission
  virtual bool sendFrame(const QByteArray &data) = 0;

  // Connection state
  virtual ConnectionState state() const = 0;

  // Connection type identification
  virtual QString connectionType() const = 0;

signals:
  // Frame received from the device
  void frameReceived(const QByteArray &frame);

  // Connection state changed
  void stateChanged(ConnectionState state);

  // Error occurred
  void errorOccurred(const QString &error);
};

} // namespace MeshCore
