/* jconfig.h --- source file edited by configure script */
/* see jconfig.doc for explanations */

#ifdef __cplusplus
extern "C" {
#endif // cplusplus

#undef HAVE_PROTOTYPES
#undef HAVE_UNSIGNED_CHAR
#undef HAVE_UNSIGNED_SHORT
#undef void
#undef const
#undef CHAR_IS_UNSIGNED
#undef HAVE_STDDEF_H
#undef HAVE_STDLIB_H
#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_FAR_POINTERS
#undef NEED_SHORT_EXTERNAL_NAMES
/* Define this if you get warnings about undefined structures. */
#undef INCOMPLETE_TYPES_BROKEN


#if defined(_WIN32) && !(defined(__CYGWIN__) || defined(__MINGW32__))
/* Define "boolean" as unsigned char, not int, per Windows custom */
/* don't conflict if rpcndr.h already read; Note that the w32api headers
   used by Cygwin and Mingw do not define "boolean", so jmorecfg.h
   handles it later. */
#ifndef __RPCNDR_H__
typedef unsigned char boolean;
#endif
#define HAVE_BOOLEAN            /* prevent jmorecfg.h from redefining it */
#endif

#ifdef JPEG_INTERNALS

#undef RIGHT_SHIFT_IS_UNSIGNED
#undef INLINE
/* These are for configuring the JPEG memory manager. */
#undef DEFAULT_MAX_MEM
#undef NO_MKTEMP

#endif /* JPEG_INTERNALS */

#ifdef JPEG_CJPEG_DJPEG

#undef BMP_SUPPORTED		/* BMP image file format */
#define GIF_SUPPORTED		/* GIF image file format */
#undef PPM_SUPPORTED		/* PBMPLUS PPM/PGM image file format */
#undef RLE_SUPPORTED		/* Utah RLE image file format */
#undef TARGA_SUPPORTED		/* Targa image file format */

#undef TWO_FILE_COMMANDLINE
#undef NEED_SIGNAL_CATCHER
#undef DONT_USE_B_MODE

/* Define this if you want percent-done progress reports from cjpeg/djpeg. */
#undef PROGRESS_REPORT

#endif /* JPEG_CJPEG_DJPEG */

#ifdef __cplusplus
}
#endif // cplusplus

#define BITS_IN_JSAMPLE 8
#define HAVE_PROTOTYPES
#include "mangle_jpeg.h"
