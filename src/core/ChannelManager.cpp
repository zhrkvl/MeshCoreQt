#include <QDebug>

#include "ChannelManager.h"

namespace MeshCore {

ChannelManager::ChannelManager(QObject *parent)
    : QObject(parent), m_isDiscovering(false) {}

void ChannelManager::initialize() {
  // Add the default public channel
  Channel publicChannel = Channel::createPublicChannel();
  addOrUpdateChannel(publicChannel);
  qDebug() << "ChannelManager initialized with public channel";
}

QVector<Channel> ChannelManager::getChannels() const {
  QVector<Channel> channels;
  for (const Channel &ch : m_channels) {
    channels.append(ch);
  }
  return channels;
}

Channel ChannelManager::getChannel(uint8_t index) const {
  return m_channels.value(index, Channel());
}

bool ChannelManager::hasChannel(uint8_t index) const {
  return m_channels.contains(index);
}

void ChannelManager::addOrUpdateChannel(const Channel &channel) {
  bool isNew = !m_channels.contains(channel.index);

  m_channels[channel.index] = channel;

  if (isNew) {
    qDebug() << "Channel added:" << channel.index << channel.name;
    emit channelAdded(channel);
  } else {
    qDebug() << "Channel updated:" << channel.index << channel.name;
    emit channelUpdated(channel);
  }
}

void ChannelManager::removeChannel(uint8_t index) {
  if (m_channels.remove(index)) {
    qDebug() << "Channel removed:" << index;
    emit channelRemoved(index);
  }
}

void ChannelManager::clear() {
  m_channels.clear();
  qDebug() << "All channels cleared";
}

uint8_t ChannelManager::getNextAvailableIndex() const {
  // Find the first available channel index (starting from 1, since 0 is public)
  for (uint8_t i = 1; i < 255; i++) {
    if (!m_channels.contains(i)) {
      return i;
    }
  }
  return 1; // Fallback, though this means all slots are taken
}

} // namespace MeshCore
