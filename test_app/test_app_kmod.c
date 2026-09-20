#include "test_app_kmod.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/syscall.h>

#include "log.h"
#include "common.h"
#include "compiler_helper.h"

static bool kmod_was_existed;

#define finit_module(fd, param_values, flags)			\
	syscall(__NR_finit_module, fd, param_values, flags)

#define delete_module(name, flags)				\
	syscall(__NR_delete_module, name, flags)

int ta_kmod_load(const char *path_to_kmod)
{
	int ret = -1, fd = -1;

	kmod_was_existed = false;

	fd = open(path_to_kmod, O_RDONLY);
	if (unlikely(fd == -1)) {
		ERR_NO("Couldn't open %s", path_to_kmod);
		return -1;
	}

	ret = finit_module(fd, "", 0);
	if (unlikely(ret)) {
		if (errno == EEXIST) {
			LOG("%s is existed, not needed to load", path_to_kmod);
			kmod_was_existed = true;
		} else {
			ERR_NO("Couldn't load %s", path_to_kmod);
			return -1;
		}
	}

	ret = close(fd);
	if (unlikely(ret))
		ERR_NO("Couldn't close %s", path_to_kmod);

	LOG("%s loaded", path_to_kmod);

	return 0;
}

void ta_kmod_unload(const char *path_to_kmod)
{
	/* unload kernel module if it was not existed while loading */
	if (likely(!kmod_was_existed && strstr(path_to_kmod, KMOD_NAME))) {
		int ret = delete_module(KMOD_NAME, O_NONBLOCK);
		if (unlikely(ret))
			ERR_NO("Couldn't remove %s", KMOD_NAME);
	}

}
