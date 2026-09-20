#include "test_app_data.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "common.h"
#include "compiler_helper.h"

static struct ta_data data[MAX_MINORS];

/**
 * rand_string_get - generate and get pseudo random string
 * @size: string size
 *
 * Returns pointer to pseudo random string on success, NULL if error occurred.
 */
static char *rand_string_get(size_t size)
{
	char *str = NULL;
	static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK...";

	if (size) {
		str = malloc(size + 1U);
		for (size_t n = 0; n < size; n++) {
			int key = rand() % (int)(sizeof(chars) - 1U);

			str[n] = chars[key];
		}
		str[size] = '\0';
	}

	return str;
}

/**
 * ta_fds_close - close Device_0, Device_1 in dev file system
 *                and debug file system
 */
static void ta_fds_close(void)
{
	for (size_t i = 0U; i < MAX_MINORS; ++i) {
		const char *dev_name = data[i].dev_name;

		if (data[i].fd_dev > 0 && close(data[i].fd_dev))
			ERR_NO("Couldn't close dev %s", dev_name);

		if (data[i].fd_dbgfs > 0 && close(data[i].fd_dbgfs))
			ERR_NO("Couldn't close debugfs %s", dev_name);
	}
}

/**
 * ta_fds_open - open Device_0, Device_1 in dev file system
 *               and debug file system
 * @path_to_kmod: path to kernel module
 *
 * Returns 0 on success, -1 if error occurred.
 */
static int ta_fds_open(void)
{
	for (size_t i = 0U; i < MAX_MINORS; ++i) {
		const char *path_dev = i == 0U ? PATH_DEV0 : PATH_DEV1;
		const char *path_dbgfs = i == 0U ? PATH_DBGFS0 : PATH_DBGFS1;

		data[i].fd_dev = open(path_dev, O_RDWR);
		if (unlikely(data[i].fd_dev == -1)) {
			ERR_NO("Couldn't open %s", path_dev);
			ta_fds_close();
			return -1;
		}

		data[i].fd_dbgfs = open(path_dbgfs, O_RDONLY);
		if (unlikely(data[i].fd_dbgfs == -1)) {
			ERR_NO("Couldn't open %s", path_dbgfs);
			ta_fds_close();
			return -1;
		}
	}

	return 0;
}

/**
 * ta_strs_free - free writing string to Device_0, Device_1
 */
static void ta_strs_free(void)
{
	for (size_t i = 0U; i < MAX_MINORS; ++i)
		for (size_t j = 0U; j < ARR_STRS_SIZE; ++j)
			free((void *)data[i].strs[j]);
}

/**
 * ta_strs_init - initialize writing string to Device_0, Device_1
 *
 * Returns 0 on success, -1 if error occurred.
 */
static int ta_strs_init(void)
{
	for (size_t i = 0U; i < MAX_MINORS; ++i) {
		data[i].ref_str = "It's a reference string";
		for (size_t j = 0U; j < ARR_STRS_SIZE; ++j) {
			data[i].strs[j] = rand_string_get(j + 1U);
			if (unlikely(!data[i].strs[j])) {
				ERR("Couldn't allocate memory");
				ta_strs_free();

				return -1;
			}
		}
	}

	return 0;
}

/**
 * ta_devs_name_free - free devices name
 */
static void ta_devs_name_free(void)
{
	for (size_t i = 0; i < MAX_MINORS; ++i)
		data[i].dev_name = NULL;
}

/**
 * ta_devs_name_free - initialize devices name
 */
static void ta_devs_name_init(void)
{
	for (size_t i = 0; i < MAX_MINORS; ++i)
		data[i].dev_name = i == 0U ? DEV0_NAME : DEV1_NAME;
}

const struct ta_data *ta_data_get(const size_t minor)
{
	return minor < MAX_MINORS ? &data[minor] : NULL;
}

int ta_data_init(void)
{
	int ret = -1;

	ta_devs_name_init();

	ret = ta_fds_open();
	if (unlikely(ret == -1)) {
		ERR("Couldn't open files");
		goto err_devs_name_free;
	}

	ret = ta_strs_init();
	if (unlikely(ret == -1)) {
		ERR("Couldn't init strings");
		goto err_fds_close;
	}

	return 0;

err_fds_close:
	ta_fds_close();
err_devs_name_free:
	ta_devs_name_free();
	return -1;
}

void ta_data_free(void)
{
	ta_fds_close();
	ta_strs_free();
	ta_devs_name_free();
}
