#ifndef _C_BLOB_H
#define _C_BLOB_H

#include "cv.h"

typedef void c_blob;

#ifdef __cplusplus
extern "C" {
#endif

c_blob *c_blob_get_biggest_ink_blob(IplImage *in);
void    c_blob_fill(c_blob *blob, IplImage *in);
void    c_blob_destroy(c_blob *blob);
int     c_blob_minx(c_blob *blob);
int     c_blob_miny(c_blob *blob);
int     c_blob_maxx(c_blob *blob);
int     c_blob_maxy(c_blob *blob);

#ifdef __cplusplus
}
#endif

#endif /* _C_BLOB_H */
