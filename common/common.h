#ifndef __COMMON_H__
#define __COMMON_H__

/* count of char devices */
#define MAX_MINORS			2U

#define KMOD_NAME			"stt_cdev"

#define DEV0_NAME			"Device_0"
#define DEV1_NAME			"Device_1"

#define PATH_DEV0			"/dev/" DEV0_NAME
#define PATH_DEV1			"/dev/" DEV1_NAME

#define PATH_DBGFS0			"/sys/kernel/debug/" KMOD_NAME "/0"
#define PATH_DBGFS1			"/sys/kernel/debug/" KMOD_NAME "/1"

/**
 * struct entry - represents string
 * @str: string
 * @db: string size
 */
struct entry {
	char *str;
	size_t size;
};

#define IOW_RESET _IOW('a', 'a', struct entry)

#endif /* __COMMON_H__ */
