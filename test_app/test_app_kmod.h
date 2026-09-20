#ifndef __TEST_APP_KMOD_H__
#define __TEST_APP_KMOD_H__

/**
 * ta_kmod_load - load kernel module to kernel
 * @path_to_kmod: path to kernel module
 *
 * Returns 0 on success, -1 if error occurred.
 */
int ta_kmod_load(const char *path_to_kmod);

/**
 * ta_kmod_unload - unload kernel module from kernel
 *                  if it was not existed while loading
 * @path_to_kmod: path to kernel module
 */
void ta_kmod_unload(const char *path_to_kmod);

#endif /* __TEST_APP_KMOD_H__ */
