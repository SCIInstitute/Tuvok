#ifndef IV3D_GZIO_H
#define IV3D_GZIO_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int gz_inflate(FILE *source, FILE *dest);

/** Reads the gzip header; inflate apparently doesn't like to read this. */
void gz_skip_header(FILE *fs);


#ifdef __cplusplus
}
#endif

#endif /* IV3D_GZIO_H */
