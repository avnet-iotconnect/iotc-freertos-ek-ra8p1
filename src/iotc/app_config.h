/*
 * app_config.h - IoTConnect device identity & connection settings.
 *
 * Do NOT put credentials in this file. Copy app_secrets.h.example to
 * app_secrets.h (gitignored) and fill in your device identity there.
 */
#ifndef IOTC_APP_CONFIG_H
#define IOTC_APP_CONFIG_H

#if defined(__has_include)
 #if __has_include("app_secrets.h")
  #include "app_secrets.h"
 #endif
#endif

/* ---- Defaults for anything app_secrets.h does not define ---- */

#ifndef IOTC_CFG_ENV
#define IOTC_CFG_ENV  ""   /* IoTConnect environment, e.g. "poc" */
#endif

#ifndef IOTC_CFG_CPID
#define IOTC_CFG_CPID ""   /* Company ID from Settings -> Key Vault */
#endif

#ifndef IOTC_CFG_DUID
#define IOTC_CFG_DUID ""   /* Device unique ID */
#endif

/* AWS-backend discovery host. */
#ifndef IOTC_CFG_DISCOVERY_HOST
#define IOTC_CFG_DISCOVERY_HOST "awsdiscovery.iotconnect.io"
#endif

#ifndef IOTC_CFG_SNTP_SERVER
#define IOTC_CFG_SNTP_SERVER "pool.ntp.org"
#endif

/* Device X.509 identity, PEM. Provisioned once into PKCS#11 (LittleFS on
 * the 64 MB OSPI flash) at boot when non-empty; may be blanked afterwards. */
#ifndef IOTC_CFG_DEVICE_CERT_PEM
#define IOTC_CFG_DEVICE_CERT_PEM ""
#endif

#ifndef IOTC_CFG_DEVICE_KEY_PEM
#define IOTC_CFG_DEVICE_KEY_PEM ""
#endif

/* Set to 1 when identity above is populated (enables the IoTConnect task). */
#ifndef IOTC_CFG_ENABLED
#define IOTC_CFG_ENABLED 0
#endif

/* Build without the 441 KB built-in model array in MRAM. Default 1: the
 * KVS WebRTC video stack and the built-in model do not fit MRAM together.
 * The device boots from the OSPI model store (any previously pushed model
 * persists) and idles with a console hint if the store is empty; push a
 * model from /IOTCONNECT AI Models to start inference. Set to 0 only if
 * you also exclude src/kvs* from the build. */
#ifndef IOTC_CFG_NO_BUILTIN_MODEL
#define IOTC_CFG_NO_BUILTIN_MODEL 1
#endif

/* One-shot on-target H.264 software-encode benchmark (minih264) printed to
 * the console at boot. Requires IOTC_CFG_NO_BUILTIN_MODEL=1 for MRAM space. */
#ifndef H264_BENCH
#define H264_BENCH 0
#endif

#endif /* IOTC_APP_CONFIG_H */
