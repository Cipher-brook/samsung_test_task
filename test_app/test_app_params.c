#include "test_app_params.h"

#include <unistd.h>

#include "log.h"
#include "compiler_helper.h"

#define DEF_KMOD_PATH			"ksrc/stt_cdev.ko"

static struct ta_params params;

static void help_msg(void)
{
	LOG("Usage:\nsudo ./test_app -f path_to_kernel_module");
	LOG("-f is optional, without -f uses %s", DEF_KMOD_PATH);
}

const struct ta_params *ta_input_params_get(int argc, char **argv)
{
	char c = '\0';

	while ((c = getopt(argc, argv, "f:h")) != -1) {
		switch (c) {
		case 'f':
			params.path_to_kmod = optarg;
			break;
		case 'h':
		default:
			help_msg();
			return NULL;
		}
	}

	if (!params.path_to_kmod)
		params.path_to_kmod = DEF_KMOD_PATH;

	return &params;
}

void ta_input_params_free(void)
{
	params.path_to_kmod = NULL;
}
