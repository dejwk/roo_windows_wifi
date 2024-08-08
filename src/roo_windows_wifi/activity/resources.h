#pragma once

#include "roo_wifi.h"

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

inline const char* StatusAsString(roo_wifi::ConnectionStatus status,
                                  bool connecting) {
  return (connecting && (status == roo_wifi::WL_DISCONNECTED ||
                         status == roo_wifi::WL_NO_SSID_AVAIL))
             ? kStrStatusConnecting
         : (status == roo_wifi::WL_IDLE_STATUS) ? kStrStatusConnectedNoInternet
         : (status == roo_wifi::WL_NO_SSID_AVAIL)   ? kStrStatusOutOfRange
         : (status == roo_wifi::WL_CONNECTED)       ? kStrStatusConnected
         : (status == roo_wifi::WL_CONNECT_FAILED)  ? kStrStatusBadPassword
         : (status == roo_wifi::WL_CONNECTION_LOST) ? kStrStatusConnectionLost
         : (status == roo_wifi::WL_DISCONNECTED)    ? kStrStatusDisconnected
                                                    : kStrStatusUnknown;
}

}  // namespace roo_windows_wifi
