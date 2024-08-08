#include "roo_windows_wifi/activity/resources.h"

#include "roo_windows/config.h"

namespace roo_windows_wifi {

#if (ROO_WINDOWS_LANG == ROO_WINDOWS_LANG_pl)

const char* kStrConnect = "Połącz";
const char* kStrDisconnect = "Rozłącz";
const char* kStrForget = "Zapomnij";

const char* kStrConnectingEllipsis = "Łączę ...";

const char* kStrWiFi = "Wi-Fi";
const char* kStrEnableWiFi = "Włącz Wi-Fi";
const char* kStrDisconnected = "Rozłączono";
const char* kStrNetworkDetails = "Szczegóły sieci";

const char* kStrStatusConnecting = "Łączenie";
const char* kStrStatusConnectedNoInternet = "Połączenie, brak Internetu";
const char* kStrStatusOutOfRange = "Poza zasięgiem";
const char* kStrStatusConnected = "Połączono";
const char* kStrStatusBadPassword = "Sprawdź hasło i spróbuj ponownie";
const char* kStrStatusConnectionLost = "Połączenie przerwane";
const char* kStrStatusDisconnected = "Rozłączono";
const char* kStrStatusUnknown = "Nieznany";

const char* kStrEnterPassword = "podaj hasło";
const char* kStrPasswordUnchanged = "(nie zmieniono)";

#else

const char* kStrConnect = "Connect";
const char* kStrDisconnect = "Disconnect";
const char* kStrForget = "Forget";

const char* kStrConnectingEllipsis = "Connecting ...";

const char* kStrWiFi = "Wi-Fi";
const char* kStrEnableWiFi = "Enable Wi-Fi";
const char* kStrDisconnected = "Disconnected";
const char* kStrNetworkDetails = "Network details";

const char* kStrStatusConnecting = "Connecting";
const char* kStrStatusConnectedNoInternet = "Connected, no Internet";
const char* kStrStatusOutOfRange = "Out of range";
const char* kStrStatusConnected = "Connected";
const char* kStrStatusBadPassword = "Check password and try again";
const char* kStrStatusConnectionLost = "Connection lost";
const char* kStrStatusDisconnected = "Disconnected";
const char* kStrStatusUnknown = "Unknown";

const char* kStrEnterPassword = "enter password";
const char* kStrPasswordUnchanged = "(unchanged)";

#endif

}  // namespace roo_windows_wifi
