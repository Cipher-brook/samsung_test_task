#ifndef __TEST_APP_H__
#define __TEST_APP_H__

/**
 * test_app - load kernel module, initialize internal application data,
 *            run tests, demonstrates output test messages to console,
 *            unload kernel module if it was not existed while loading
 * @argc: the number of command-line arguments passed by the user
 *        including the name of the program
 * @argv: an array of character pointers listing all the arguments
 *
 * Returns 0 on success, -1 if tests didn't pass or error occurred.
 */
int test_app(int argc, char **argv);

#endif /* __TEST_APP_H__ */
