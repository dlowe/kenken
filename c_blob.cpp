#include "Blob.h"
#include "Blobresult.h"
#include "c_blob.h"

c_blob *c_blob_get_biggest_ink_blob(IplImage *in) {
    CBlob *currentBlob = new CBlob;

    CBlobResult blobs;
    blobs = CBlobResult(in, NULL, 0, true);

    blobs.Filter(blobs, B_INCLUDE, CBlobGetMean(), B_EQUAL, 0);
    blobs.GetNthBlob(CBlobGetArea(), 0, *currentBlob);

    return (void *)currentBlob;
}

void c_blob_fill(void *blob, IplImage *in) {
    ((CBlob *)blob)->FillBlob(in, CV_RGB(255, 255, 255));
    return;
}

void    c_blob_destroy(c_blob *blob) {
    delete (CBlob *)blob;
}

int     c_blob_minx(c_blob *blob) {
    return ((CBlob *)blob)->MinX();
}

int     c_blob_miny(c_blob *blob) {
    return ((CBlob *)blob)->MinY();
}

int     c_blob_maxx(c_blob *blob) {
    return ((CBlob *)blob)->MaxX();
}

int     c_blob_maxy(c_blob *blob) {
    return ((CBlob *)blob)->MaxY();
}
