#ifndef __LOG_H__
#define __LOG_H__

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define LOG(fmt, ...)						\
	printf(fmt "\n", ##__VA_ARGS__)

#define ERR(fmt, ...)						\
	printf("[%s:%d] " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

#define ERR_NO(fmt, ...)					\
	printf("[%s:%d] " fmt ", errno = %s\n",			\
		__func__, __LINE__, ##__VA_ARGS__, strerror(errno))

#endif /* __LOG_H__ */
