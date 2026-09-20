#include "test_app.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include<sys/ioctl.h>

#include "log.h"
#include "common.h"
#include "test_app_data.h"
#include "test_app_kmod.h"
#include "test_app_params.h"
#include "compiler_helper.h"

#define BUF_SIZE				1024U
#define TEST_BEGIN				"TEST_%zu_BEGIN "
#define TEST_END				"TEST_%zu_END "

static bool fasync_flag;

/**
 * signal_fasync_handler - handler SIGIO signal
 * @signal: signal
 */
static void signal_fasync_handler(int signal)
{
	if (likely(signal == SIGIO)) {
		LOG("SIGIO handler called");
		fasync_flag = true;
	}
}

/**
 * ta_ref_strs_set - test to set reference string to devices
 * @test_num: test number
 *
 * Returns 0 on success, -1 if test failed.
 */
static int ta_ref_strs_set(size_t test_num)
{
	LOG("\n" TEST_BEGIN "Set reference strings", test_num);

	for (size_t i = 0U; i < MAX_MINORS; ++i) {
		long ret = -1;
		struct entry entry;
		const struct ta_data *const data = ta_data_get(i);

		if (unlikely(!data)) {
			ERR("Invalid data");
			return -1;
		}

		entry.str = data->ref_str;
		entry.size = strlen(data->ref_str);

		LOG("Setting reference string for %s", data->dev_name);

		ret = ioctl(data->fd_dev, IOW_RESET, (struct entry *)&entry);
		if (unlikely(ret)) {
			ERR("Couldn't set reference string for %s",
			    data->dev_name);
			return -1;
		}

		LOG("Reference string was set successfully for %s",
		    data->dev_name);
	}

	LOG(TEST_END "Reference strings were set successfully", test_num);

	return 0;
}

/**
 * ta_strs_dev_write - write strings to device
 * @data: test application data
 *
 * Returns 0 on success, -1 if error occurred.
 */
static int ta_strs_dev_write(const struct ta_data *const data)
{
	ssize_t ret = -1;

	for (size_t i = 0U; i < ARR_STRS_SIZE; ++i) {
		const char *str = data->strs[i];

		LOG("Writting %s to %s", str, data->dev_name);

		ret = write(data->fd_dev, str, strlen(str));
		if (unlikely(ret == -1)) {
			ERR_NO("couldn't write string to %s", data->dev_name);
			return -1;
		}
	}

	return 0;
}

/**
 * ta_strs_write - test to write strings to devices
 * @test_num: test number
 *
 * Returns 0 on success, -1 if test failed.
 */
static int ta_strs_write(size_t test_num)
{
	LOG("\n" TEST_BEGIN "Write strings", test_num);

	for (size_t i = 0U; i < MAX_MINORS; ++i) {
		int ret = -1;
		const struct ta_data *const data = ta_data_get(i);

		if (unlikely(!data)) {
			ERR("Invalid data");
			return -1;
		}

		ret = ta_strs_dev_write(data);
		if (unlikely(ret == -1)) {
			ERR("Couldn't write strings to %s", data->dev_name);
			return -1;
		}
	}

	LOG(TEST_END "Strings were written successfully", test_num);

	return 0;
}

/**
 * ta_strs_dbgfs_read - read strings from device debug file system
 * @data: test application data
 *
 * Returns 0 on success, -1 if error occurred.
 */
static int ta_strs_dbgfs_read(const struct ta_data *const data)
{
	ssize_t ret = -1;
	size_t offset = 0U;
	char *strs = NULL;

	strs = malloc(BUF_SIZE);
	if (unlikely(!strs)) {
		ERR("Couldn't allocate memory for all strings");
		return -1;
	}

	ret = read(data->fd_dbgfs, strs, BUF_SIZE);
	if (unlikely(ret == -1)) {
		ERR_NO("Couldn't read all strings from %s", data->dev_name);
		free(strs);
		return -1;
	}

	for (size_t i = 0U; i < ARR_STRS_SIZE; ++i) {
		const char *str = data->strs[i];
		const char *read_str = (char *)(strs + offset);
		const size_t read_str_size = strlen(read_str);

		if (unlikely(strncmp(str, read_str, read_str_size))) {
			ERR("Read str %s != written str %s", str, read_str);
			free(strs);
			return -1;
		}

		LOG("Read str %s == written str %s", str, read_str);

		offset += read_str_size + 1U;
	}

	if (unlikely(offset != (size_t)ret)) {
		ERR("Didn't divide strings, offset=%zu, ret=%zu", offset, ret);
		free(strs);
		return -1;
	}

	free(strs);

	return 0;
}

/**
 * ta_strs_read - test to read strings from devices debug file system
 * @test_num: test number
 *
 * Returns 0 on success, -1 if test failed.
 */
static int ta_strs_read(size_t test_num)
{
	LOG("\n" TEST_BEGIN "Equal read and written strings", test_num);

	for (size_t i = 0U; i < MAX_MINORS; ++i) {
		int ret = -1;
		const struct ta_data *const data = ta_data_get(i);

		if (unlikely(!data)) {
			ERR("Invalid data");
			return -1;
		}

		LOG("Reading strings from debugfs %s", data->dev_name);

		ret = ta_strs_dbgfs_read(data);
		if (unlikely(ret == -1)) {
			ERR("Couldn't read strings %s", data->dev_name);
			return -1;
		}

		LOG("Written strings equal read strings for %s",
		    data->dev_name);
	}

	LOG(TEST_END "Written strings equal read strings", test_num);

	return 0;
}

/**
 * ta_str_dev_match_check - set SIGIO signal handler,
 *                          check that signal received from device
 * @data: test application data
 *
 * Returns 0 on success, -1 if error occurred.
 */
static int ta_str_dev_match_check(const struct ta_data *const data)
{
	struct entry entry;
	size_t attempt = 0U;
	struct sigaction act;
	int ret = -1, flags = -1;
	const int fd = data->fd_dev;

	entry.str = data->ref_str;
	entry.size = strlen(data->ref_str);

	memset(&act, '\0', sizeof(act));
	act.sa_handler = &signal_fasync_handler;
	ret = sigaction(SIGIO, &act, NULL);
	if (unlikely(ret == -1)) {
		ERR_NO("Couldn't set signal action");
		return -1;
	}

	ret = fcntl(fd, F_SETOWN, getpid());
	if (unlikely(ret == -1)) {
		ERR_NO("Couldn't set owner");
		return -1;
	}

	flags = fcntl(fd, F_GETFL);
	if (unlikely(flags == -1)) {
		ERR_NO("Couldn't get flags");
		return -1;
	}

	ret = fcntl(fd, F_SETFL, flags | FASYNC);
	if (unlikely(ret == -1)) {
		ERR_NO("Couldn't set flags");
		return -1;
	}

	LOG("SIGIO handler was set to %s", data->dev_name);
	LOG("writing string equals reference string to %s", data->dev_name);

	ret = write(fd, entry.str, entry.size);
	if (unlikely(ret == -1)) {
		ERR_NO("Couldn't write to %s", data->dev_name);
		return -1;
	}

	while (!fasync_flag && attempt++ < 5U)
		sleep(1000);

	return fasync_flag ? 0 : -1;
}

/**
 * ta_str_match_check - test to check match reference string
 *                      to writing string to devices
 * @test_num: test number
 *
 * Returns 0 on success, -1 if test failed.
 */
static int ta_str_match_check(size_t test_num)
{
	LOG("\n" TEST_BEGIN "Check string match events", test_num);

	for (size_t i = 0U; i < MAX_MINORS; ++i) {
		int ret = -1;
		const char *dev_name = NULL;
		const struct ta_data *const data = ta_data_get(i);

		if (unlikely(!data)) {
			ERR("Invalid data");
			return -1;
		}

		dev_name = data->dev_name;
		fasync_flag = false;

		LOG("Checking string match event %s", dev_name);

		ret = ta_str_dev_match_check(data);
		if (unlikely(ret == -1)) {
			ERR("String match event failed %s", dev_name);
			return -1;
		}

		LOG("String match event finished successfully %s", dev_name);
	}

	LOG(TEST_END "String match events finished successfully\n", test_num);

	return 0;
}

/**
 * ta_run_tests - run tests
 *
 * Returns 0 on success, -1 if tests failed.
 */
static int ta_run_tests(void)
{
	int ret = -1;
	size_t test_num = 1U;

	/* Setting reference string by ioctl call for each of the devices */
	ret = ta_ref_strs_set(test_num++);
	if (unlikely(ret == -1)) {
		ERR("Couldn't set reference strings");
		return -1;
	}

	/* Writing arbitrary strings */
	ret = ta_strs_write(test_num++);
	if (unlikely(ret == -1)) {
		ERR("Couldn't write strings");
		return -1;
	}

	/* Reading all strings that were written to the particular device */
	ret = ta_strs_read(test_num++);
	if (unlikely(ret == -1)) {
		ERR("Couldn't read strings");
		return -1;
	}

	/* Showing that string match event were sent by fasync method */
	ret = ta_str_match_check(test_num);
	if (unlikely(ret == -1)) {
		ERR("Couldn't match string");
		return -1;
	}

	return 0;
}

int test_app(int argc, char **argv)
{
	int ret = -1;
	const struct ta_params *params = NULL;

	params = ta_input_params_get(argc, argv);
	if (unlikely(!params))
		goto exit;

	LOG("Test application started");

	ret = ta_kmod_load(params->path_to_kmod);
	if (unlikely(ret == -1)) {
		ERR("Couldn't load kmod %s", params->path_to_kmod);
		goto exit_input_params_free;
	}

	ret = ta_data_init();
	if (ret == -1) {
		ERR("Couldn't init test application");
		goto exit_kmod_unload;
	}

	LOG("Running tests");

	ret = ta_run_tests();
	if (unlikely(ret == -1)) {
		ERR("Tests failed");
		goto exit_data_free;
	}

	LOG("Tests finished successfully");
	LOG("Test application finished successfully");

exit_data_free:
	ta_data_free();
exit_kmod_unload:
	ta_kmod_unload(params->path_to_kmod);
exit_input_params_free:
	ta_input_params_free();
exit:
	return ret;
}
