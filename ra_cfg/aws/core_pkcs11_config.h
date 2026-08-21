/* generated configuration header file - do not edit */
#ifndef CORE_PKCS11_CONFIG_H_
#define CORE_PKCS11_CONFIG_H_
#ifdef __cplusplus
extern "C" {
#endif



#include "logging_levels.h"

#include "logging_stack.h"

/* FreeRTOS include. */
#include "FreeRTOS.h"

#ifndef pkcs11configPKCS11_MALLOC
#define pkcs11configPKCS11_MALLOC       pvPortMalloc
#endif

#ifndef pkcs11configPKCS11_FREE
#define pkcs11configPKCS11_FREE         vPortFree
#endif


#ifndef configPKCS11_DEFAULT_USER_PIN
#define configPKCS11_DEFAULT_USER_PIN            "0000"
#endif

#ifndef pkcs11configMAX_LABEL_LENGTH
#define pkcs11configMAX_LABEL_LENGTH                    32
#endif

#ifndef pkcs11configMAX_NUM_OBJECTS
#define pkcs11configMAX_NUM_OBJECTS                     6
#endif

#ifndef pkcs11configMAX_SESSIONS
#define pkcs11configMAX_SESSIONS                        10
#endif

#ifndef pkcs11configPAL_DESTROY_SUPPORTED
#define pkcs11configPAL_DESTROY_SUPPORTED               0
#endif

#ifndef pkcs11configOTA_SUPPORTED
#define pkcs11configOTA_SUPPORTED                       0
#endif

#ifndef pkcs11configJITP_CODEVERIFY_ROOT_CERT_SUPPORTED
#define pkcs11configJITP_CODEVERIFY_ROOT_CERT_SUPPORTED 0
#endif

#ifndef pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS
#define pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS    "Device Priv TLS Key"
#endif

#ifndef pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS
#define pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS     "Device Pub TLS Key"
#endif

#ifndef pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS
#define pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS    "Device Cert"
#endif

#ifndef pkcs11configLABEL_ROOT_CERTIFICATE
#define pkcs11configLABEL_ROOT_CERTIFICATE              "Root Cert"
#endif

#ifndef pkcs11configLABEL_CODE_VERIFICATION_KEY
#define pkcs11configLABEL_CODE_VERIFICATION_KEY         "Code Verify Key"
#endif

#ifndef pkcs11configLABEL_JITP_CERTIFICATE
#define pkcs11configLABEL_JITP_CERTIFICATE              "JITP Cert"
#endif

#ifndef pkcs11configLABEL_HMAC_KEY
#define pkcs11configLABEL_HMAC_KEY                       "HMAC Key"
#endif

#ifndef pkcs11configLABEL_CMAC_KEY
#define pkcs11configLABEL_CMAC_KEY                       "CMAC Key"
#endif

#ifdef __cplusplus
}
#endif
#endif /* CORE_PKCS11_CONFIG_H_ */
