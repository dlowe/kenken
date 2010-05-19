#ifndef _KENKEN_H
#define _KENKEN_H

#include "cv.h"
#include "highgui.h"

const CvPoint2D32f* locate_puzzle(IplImage *in, IplImage **annotated);

IplImage *square_puzzle(IplImage *in, const CvPoint2D32f *location);

typedef unsigned short puzzle_size;

puzzle_size compute_puzzle_size(IplImage *puzzle);

char *compute_puzzle_cages(IplImage *puzzle, puzzle_size size);

void showSmaller (IplImage *in, char *window_name);

#endif /* _KENKEN_H */
