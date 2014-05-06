#ifndef NETDATASET_H
#define NETDATASET_H
#include <stdlib.h>

#ifdef __cplusplus
# define EXPORT extern "C"
#else
# define EXPORT /* no extern "C" needed. */
#endif

EXPORT void netds_brick_request(const size_t lod, const size_t bidx);
EXPORT void netds_open(const char*);
EXPORT void netds_close(const char*);
EXPORT char** netds_list_files(size_t* count);
EXPORT void netds_shutdown();

#endif
