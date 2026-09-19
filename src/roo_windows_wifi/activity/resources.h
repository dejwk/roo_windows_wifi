#pragma once

#include "roo_windows_wifi/model.h"

namespace roo_windows_wifi {

extern const char* kStrConnect;
extern const char* kStrDisconnect;
extern const char* kStrForget;
extern const char* kStrConnectingEllipsis;

extern const char* kStrWiFi;
extern const char* kStrEnableWiFi;
extern const char* kStrNetworkDetails;

extern const char* kStrStatusConnecting;
extern const char* kStrStatusConnectedNoInternet;
extern const char* kStrStatusOutOfRange;
extern const char* kStrStatusConnected;
extern const char* kStrStatusBadPassword;
extern const char* kStrStatusConnectionLost;
extern const char* kStrStatusDisconnected;
extern const char* kStrStatusUnknown;

extern const char* kStrEnterPassword;
extern const char* kStrPasswordUnchanged;

inline const char* StatusAsString(ConnectionStatus status, bool connecting) {
  return (connecting &&
          (status == WL_DISCONNECTED || status == WL_NO_SSID_AVAIL))
             ? kStrStatusConnecting
         : (status == WL_IDLE_STATUS)     ? kStrStatusConnectedNoInternet
         : (status == WL_NO_SSID_AVAIL)   ? kStrStatusOutOfRange
         : (status == WL_CONNECTED)       ? kStrStatusConnected
         : (status == WL_CONNECT_FAILED)  ? kStrStatusBadPassword
         : (status == WL_CONNECTION_LOST) ? kStrStatusConnectionLost
         : (status == WL_DISCONNECTED)    ? kStrStatusDisconnected
                                          : kStrStatusUnknown;
}

}  // namespace roo_windows_wifi
