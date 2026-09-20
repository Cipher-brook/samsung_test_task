#ifndef __TEST_APP_PARAMS_H__
#define __TEST_APP_PARAMS_H__

/**
 * struct ta_params - represents the application input parameters
 * @path_to_kmod: path to kernel module
 */
struct ta_params {
	const char *path_to_kmod;
};

/**
 * ta_input_params_get- get input parameters
 * @argc: the number of command-line arguments passed by the user
 *        including the name of the program
 * @argv: an array of character pointers listing all the arguments
 *
 * Returns pointer to input parameters, NULL if error occurred.
 */
const struct ta_params *ta_input_params_get(int argc, char **argv);

/**
 * ta_input_params_free - free input parameters
 */
void ta_input_params_free(void);

#endif /* __TEST_APP_PARAMS_H__ */
