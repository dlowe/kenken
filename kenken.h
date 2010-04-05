#ifndef _KENKEN_H
#define _KENKEN_H

#include "cv.h"
#include "highgui.h"
#include "c_blob.h"

c_blob *_locate_puzzle_blob(IplImage *in);

const CvPoint2D32f* locate_puzzle(IplImage *in);

IplImage *square_puzzle(IplImage *in, const CvPoint2D32f *location);

unsigned short compute_puzzle_size(IplImage *puzzle);

char *compute_puzzle_cages(IplImage *puzzle, unsigned short puzzle_size);

void showSmaller (IplImage *in, char *window_name);

#endif /* _KENKEN_H */
