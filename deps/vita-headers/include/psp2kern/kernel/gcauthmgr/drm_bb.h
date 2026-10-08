/**
 * \kernelgroup{SceSblGcAuthMgr}
 * \usage{psp2kern/kernel/gcauthmgr/drm_bb.h,SceSblGcAuthMgrDrmBBForDriver_stub}
 */

#ifndef __PSP2KERN_GCAUTHMGR_DRM_BB_H__
#define __PSP2KERN_GCAUTHMGR_DRM_BB_H__

#include <psp2kern/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Removes derived cart secret from memory.
 *
 * @return 0 on success, < 0 on error.
 */
int ksceSblGcAuthMgrDrmBBClearCartSecret(void);

/**
 * @brief Gets derived cart secret from memory.
 * should be the result of sha256 of the cartridges 2 unique per-game keys
 *
 * @param[out]  secret - Pointer to 0x20 bytes.
 *
 * @return 0 on success, < 0 on error.
 */
int ksceSblGcAuthMgrDrmBBGetCartSecret(void* secret);

/**
 * @brief Checks a given value and compares to the hash derived from authentication step
 *
 * @param[in]  hash - Pointer to 0x14 byte hash value to compare against
 *
 * @return 0 if matches, < 0 if does not match.
 */
int ksceSblGcAuthMgrDrmBBCheckCartHash(const void* hash);

#ifdef __cplusplus
}
#endif

#endif /* __PSP2KERN_GCAUTHMGR_DRM_BB_H__ */