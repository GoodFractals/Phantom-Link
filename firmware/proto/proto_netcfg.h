#ifndef PHANTOM_NETCFG_H
#define PHANTOM_NETCFG_H

/* Phantom Link network identity configuration.
 *
 * All three values are overridable from the build command line or by editing
 * this one header. Changing the AP name/password or enrolling an AP BSSID never
 * touches protocol, crypto, or timing code.
 *
 *   -DPHANTOM_WIFI_SSID="MY-AP"
 *   -DPHANTOM_WIFI_PASSWORD="secret"
 *   -DPHANTOM_AP_BSSID='{0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}'
 */

/* Generated per-pair overrides (SSID, password, AP BSSID). */
#include "netcfg_local.h"

#ifndef PHANTOM_WIFI_SSID
#define PHANTOM_WIFI_SSID "TMOBILE-1219"
#endif

#ifndef PHANTOM_WIFI_PASSWORD
#define PHANTOM_WIFI_PASSWORD "12345678"
#endif

/* All-zero means "unbound": the STA falls back to SSID-only joining until a
 * BSSID is enrolled. Enrollment is done by tools/provision_pair.ps1. */
#ifndef PHANTOM_AP_BSSID
#define PHANTOM_AP_BSSID {0, 0, 0, 0, 0, 0}
#endif

#endif
