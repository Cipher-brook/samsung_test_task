#ifndef __TEST_APP_DATA_H__
#define __TEST_APP_DATA_H__

#include <stdio.h>
#include <stdbool.h>

#define ARR_STRS_SIZE				20U

/**
 * struct ta_data - represents the test application data
 * @fd_dev: file descriptor to open device in /dev(read and write)
 * @fd_dbgfs: file descriptor to open device in /debugfs(only read)
 * @dev_name: character device name
 * @strs: an array of random strings
 * @ref_str: reference string
 */
struct ta_data {
	int fd_dev;
	int fd_dbgfs;
	const char *dev_name;
	char *strs[ARR_STRS_SIZE];
	char *ref_str;
};

/**
 * ta_data_init - initialize test application data
 *
 * Returns 0 on success, -1 if error occurred.
 */
int ta_data_init(void);

/**
 * ta_data_free - free application data
 */
void ta_data_free(void);

/**
 * ta_data_get- initialize application data
 * @minor: device minor number
 *
 * Returns pointer to application data on success,
 * NULL if device minor number is not valid.
 */
const struct ta_data *ta_data_get(const size_t minor);

#endif /* __TEST_APP_DATA_H__ */
