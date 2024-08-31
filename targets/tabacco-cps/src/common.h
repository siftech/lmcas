#ifndef LMCAS_CHALS_COMMON_H
#define LMCAS_CHALS_COMMON_H 1

#include <stddef.h>

/**
 * Returns errors as non-zero return values compatible with strerror(). If zero
 * is returned, a malloc()-allocated buffer (which should be freed by the
 * caller) is stored in *out_buf, and its length is stored in *out_len.
 */
int read_file(const char *path, unsigned char **out_buf, size_t *out_len);

#ifdef MUST_AUTO_NECK_ID

#define neck()                                                                 \
  do {                                                                         \
  } while (0)

#else

__attribute__((noinline, weak)) extern void _lmcas_neck(void);
#define neck() _lmcas_neck()

#endif // MUST_AUTO_NECK_ID

#endif // LMCAS_CHALS_COMMON_H
