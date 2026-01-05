#pragma once

#include <QMap>
#include <QObject>
#include <QVector>

#include "../models/Channel.h"

namespace MeshCore {

class ChannelManager : public QObject {
  Q_OBJECT

public:
  explicit ChannelManager(QObject *parent = nullptr);

  // Initialization - adds public channel by default
  void initialize();

  // Channel list operations
  QVector<Channel> getChannels() const;
  Channel getChannel(uint8_t index) const;
  bool hasChannel(uint8_t index) const;
  void addOrUpdateChannel(const Channel &channel);
  void removeChannel(uint8_t index);
  void clear();
  uint8_t getNextAvailableIndex() const;

  // Discovery state
  bool isDiscovering() const { return m_isDiscovering; }
  void setDiscovering(bool discovering) { m_isDiscovering = discovering; }

signals:
  void channelAdded(const Channel &channel);
  void channelUpdated(const Channel &channel);
  void channelRemoved(uint8_t index);
  void discoveryComplete();

private:
  QMap<uint8_t, Channel> m_channels; // index -> Channel
  bool m_isDiscovering;
};

} // namespace MeshCore
