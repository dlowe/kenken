#include "cv.h"
#include "highgui.h"
#include "Blob.h"
#include "BlobResult.h"

void _locate_puzzle_blob(IplImage *in, CBlob *currentBlob);

const CvPoint2D32f* locate_puzzle(IplImage *in);

IplImage *square_puzzle(IplImage *in, const CvPoint2D32f *location);

void showSmaller (IplImage *in, char *window_name);
