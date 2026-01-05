#pragma once

#include <cstdint>

namespace MeshCore {

// Protocol version
constexpr uint8_t PROTOCOL_VERSION = 3;

// Frame delimiters
constexpr uint8_t FRAME_OUTBOUND = 0x3e; // '>' - radio to app
constexpr uint8_t FRAME_INBOUND = 0x3c;  // '<' - app to radio

// Frame size limits
constexpr int MAX_FRAME_SIZE = 172;
constexpr int PUB_KEY_SIZE = 32;
constexpr int MAX_PATH_SIZE = 64;
constexpr int MAX_NAME_SIZE = 32;

// Public channel PSK (base64)
constexpr const char *PUBLIC_GROUP_PSK = "izOH6cXN6mrJ5e26oRXNcg==";

// Command codes (app -> radio)
enum class CommandCode : uint8_t {
  APP_START = 1,
  SEND_TXT_MSG = 2,
  SEND_CHANNEL_TXT_MSG = 3,
  GET_CONTACTS = 4,
  GET_DEVICE_TIME = 5,
  SET_DEVICE_TIME = 6,
  SEND_SELF_ADVERT = 7,
  SET_ADVERT_NAME = 8,
  ADD_UPDATE_CONTACT = 9,
  SYNC_NEXT_MESSAGE = 10,
  SET_RADIO_PARAMS = 11,
  SET_RADIO_TX_POWER = 12,
  RESET_PATH = 13,
  SET_ADVERT_LATLON = 14,
  REMOVE_CONTACT = 15,
  SHARE_CONTACT = 16,
  EXPORT_CONTACT = 17,
  IMPORT_CONTACT = 18,
  REBOOT = 19,
  GET_BATT_AND_STORAGE = 20,
  SET_TUNING_PARAMS = 21,
  DEVICE_QUERY = 22,
  EXPORT_PRIVATE_KEY = 23,
  IMPORT_PRIVATE_KEY = 24,
  SEND_RAW_DATA = 25,
  SEND_LOGIN = 26,
  SEND_STATUS_REQ = 27,
  HAS_CONNECTION = 28,
  LOGOUT = 29,
  GET_CONTACT_BY_KEY = 30,
  GET_CHANNEL = 31,
  SET_CHANNEL = 32,
  SIGN_START = 33,
  SIGN_DATA = 34,
  SIGN_FINISH = 35,
  SEND_TRACE_PATH = 36,
  SET_DEVICE_PIN = 37,
  SET_OTHER_PARAMS = 38,
  SEND_TELEMETRY_REQ = 39,
  GET_CUSTOM_VARS = 40,
  SET_CUSTOM_VAR = 41,
  GET_ADVERT_PATH = 42,
  GET_TUNING_PARAMS = 43,
  SEND_BINARY_REQ = 50,
  FACTORY_RESET = 51,
  SEND_PATH_DISCOVERY_REQ = 52,
  SET_FLOOD_SCOPE = 54,
  SEND_CONTROL_DATA = 55,
  GET_STATS = 56,
};

// Response codes (radio -> app)
enum class ResponseCode : uint8_t {
  OK = 0,
  ERR = 1,
  CONTACTS_START = 2,
  CONTACT = 3,
  END_OF_CONTACTS = 4,
  SELF_INFO = 5,
  SENT = 6,
  CONTACT_MSG_RECV = 7,
  CHANNEL_MSG_RECV = 8,
  CURR_TIME = 9,
  NO_MORE_MESSAGES = 10,
  EXPORT_CONTACT = 11,
  BATT_AND_STORAGE = 12,
  DEVICE_INFO = 13,
  PRIVATE_KEY = 14,
  DISABLED = 15,
  CONTACT_MSG_RECV_V3 = 16,
  CHANNEL_MSG_RECV_V3 = 17,
  CHANNEL_INFO = 18,
  SIGN_START = 19,
  SIGNATURE = 20,
  CUSTOM_VARS = 21,
  ADVERT_PATH = 22,
  TUNING_PARAMS = 23,
  STATS = 24,
};

// Push notification codes (radio -> app, async, value >= 0x80)
enum class PushCode : uint8_t {
  ADVERT = 0x80,
  PATH_UPDATED = 0x81,
  SEND_CONFIRMED = 0x82,
  MSG_WAITING = 0x83,
  RAW_DATA = 0x84,
  LOGIN_SUCCESS = 0x85,
  LOGIN_FAIL = 0x86,
  STATUS_RESPONSE = 0x87,
  LOG_RX_DATA = 0x88,
  TRACE_DATA = 0x89,
  NEW_ADVERT = 0x8A,
  TELEMETRY_RESPONSE = 0x8B,
  BINARY_RESPONSE = 0x8C,
  PATH_DISCOVERY_RESPONSE = 0x8D,
  CONTROL_DATA = 0x8E,
};

// Error codes (second byte of RESP_CODE_ERR)
enum class ErrorCode : uint8_t {
  UNSUPPORTED_CMD = 1,
  NOT_FOUND = 2,
  TABLE_FULL = 3,
  BAD_STATE = 4,
  FILE_IO_ERROR = 5,
  ILLEGAL_ARG = 6,
};

// Text message types
enum class TextType : uint8_t {
  PLAIN = 0,
  CLI_DATA = 1,
  SIGNED_PLAIN = 2,
};

// Contact types
enum class ContactType : uint8_t {
  NONE = 0,
  CHAT = 1,
  REPEATER = 2,
  ROOM = 3,
};

// Stats sub-types for CMD_GET_STATS
enum class StatsType : uint8_t {
  CORE = 0,
  RADIO = 1,
  PACKETS = 2,
};

// Path length special values
constexpr int8_t PATH_LEN_FLOOD = -1;
constexpr uint8_t PATH_LEN_DIRECT = 0xFF;

} // namespace MeshCore
