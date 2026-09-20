#ifndef __COMPILER_HELPER_H__
#define __COMPILER_HELPER_H__

#define __unused			__attribute__((unused))

#define likely(x)			__builtin_expect(!!(x), 1)
#define unlikely(x)			__builtin_expect(!!(x), 0)

#endif /* __COMPILER_HELPER_H__ */
