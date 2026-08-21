/*
 * iotc_fs.h - shared LittleFS (on OSPI flash) bring-up for the credential
 * and model stores.
 */
#ifndef IOTC_FS_H
#define IOTC_FS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct lfs;

/* Mount (formatting a virgin volume if needed). Idempotent, thread-safe. */
int iotc_fs_init(void);
bool iotc_fs_is_ready(void);

/* Serialize littlefs access between tasks (PKCS#11 vs model store). */
void iotc_fs_lock(void);
void iotc_fs_unlock(void);

struct lfs *iotc_fs_lfs(void);

#ifdef __cplusplus
}
#endif

#endif /* IOTC_FS_H */
