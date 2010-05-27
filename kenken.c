#include <stdio.h>

#include "cv.h"
#include "kenken.h"

static IplImage *_threshold(IplImage *in) {
    IplImage *img = cvCreateImage(cvGetSize(in), 8, 1);

    // convert to grayscale
    cvCvtColor(in, img, CV_BGR2GRAY);

    // compute the mean intensity. This is used to adjust constant_reduction value below.
    long total = 0;
    for (int x = 0; x < img->width; ++x) {
        for (int y = 0; y < img->height; ++y) {
            CvScalar s = cvGet2D(img, y, x);
            total += s.val[0];
        }
    }
    int mean_intensity = (int)(total / (img->width * img->height));

    // apply thresholding (converts it to a binary image)
    // block_size observations: higher value does better for images with variable lighting (e.g.
    //   shadows).
    // may eventually need to paramaterize this, to some extent, because the different callers
    //   seem to do better with different values (e.g. contour location is better with smaller numbers,
    //   but cage location is better with larger...) but for now, have been able to settle on value
    //   which works pretty well for most cases.
    int block_size = (int)(img->width / 9);
    if ((block_size % 2) == 0) {
        // must be odd
        block_size += 1;
    }
    // constant_reduction observations: magic, but adapting this value to the mean intensity of the
    //   image as a whole seems to help.
    int constant_reduction = (int)(mean_intensity / 3.6 + 0.5);

    IplImage *threshold_image = cvCreateImage(cvGetSize(img), 8, 1);
    cvAdaptiveThreshold(img, threshold_image, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY_INV,
        block_size, constant_reduction);
    cvReleaseImage(&img);

    // try to get rid of "noise" spots.
    int min_blob_size = 2;
    for (int x = 0; x < threshold_image->width; ++x) {
        for (int y = 0; y < threshold_image->height; ++y) {
            CvScalar s = cvGet2D(threshold_image, y, x);
            int ink_neighbors = 0;
            if (s.val[0] == 255) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if ((x + dx >= 0) && (x + dx < threshold_image->width)) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            if ((y + dy >= 0) && (y + dy < threshold_image->height)) {
                                if (! ((dy == 0) && (dx == 0))) {
                                    CvScalar m = cvGet2D(threshold_image, y + dy, x + dx);
                                    if (m.val[0] == 255) {
                                        ++ink_neighbors;
                                        if (ink_neighbors > min_blob_size) {
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        if (ink_neighbors > min_blob_size) {
                            break;
                        }
                    }
                }
                if (ink_neighbors <= min_blob_size) {
                    s.val[0] = 0;
                    cvSet2D(threshold_image, y, x, s);
                }
            }
        }
    }

    return threshold_image;
}

static CvSeq *_locate_puzzle_contour(IplImage *in) {
    IplImage *threshold_image = _threshold(in);

    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* contour = 0;

    cvFindContours(threshold_image, storage, &contour, sizeof(CvContour), CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));

    cvReleaseImage(&threshold_image);

    double max_area    = fabs(cvContourArea(contour, CV_WHOLE_SEQ));
    CvSeq *max_contour = contour;
    for( CvSeq *p = contour; p != 0; p = p->h_next )
    {
        double area = fabs(cvContourArea(p, CV_WHOLE_SEQ));
        if (area > max_area) {
            max_area    = area;
            max_contour = p;
        }
    }

    return max_contour;
}

static IplImage *_grid(IplImage *in) {
    CvSeq *contour = _locate_puzzle_contour(in);

    // draw the contour onto an otherwise blank image
    IplImage *grid_image = cvCreateImage(cvGetSize(in), 8, 1);
    CvScalar color = CV_RGB(255, 255, 255);
    cvDrawContours(grid_image, contour, color, color, -1, CV_FILLED, 8, cvPoint(0, 0) );

    return grid_image;
}

static void intersect(CvPoint *a, CvPoint *b, CvPoint2D32f *i) {
   int x[5] = { 0, a[0].x, a[1].x, b[0].x, b[1].x };
   int y[5] = { 0, a[0].y, a[1].y, b[0].y, b[1].y };

   // http://en.wikipedia.org/wiki/Line-line_intersection
   i->x = (((( x[1] * y[2] ) - ( y[1] * x[2] )) * (x[3] - x[4])) - ((x[1] - x[2]) * ((x[3] * y[4]) - (y[3] * x[4])))) / (((x[1] - x[2]) * (y[3] - y[4])) - ((y[1] - y[2]) * (x[3] - x[4])));
   i->y = (((( x[1] * y[2] ) - ( y[1] * x[2] )) * (y[3] - y[4])) - ((y[1] - y[2]) * ((x[3] * y[4]) - (y[3] * x[4])))) / (((x[1] - x[2]) * (y[3] - y[4])) - ((y[1] - y[2]) * (x[3] - x[4])));

   return;
}

const CvPoint2D32f* locate_puzzle(IplImage *in, IplImage **annotated) {
    IplImage *grid_image = _grid(in);

    *annotated = cvCloneImage(in);

    // find lines using Hough transform
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* lines = 0;

    double distance_resolution = 1;
    double angle_resolution    = CV_PI / 60;
    int threshold              = 60;
    int minimum_line_length    = in->width / 2;
    int maximum_join_gap       = in->width / 10;
    lines = cvHoughLines2(grid_image, storage, CV_HOUGH_PROBABILISTIC,  distance_resolution, angle_resolution, threshold, minimum_line_length, maximum_join_gap);

    cvCvtColor(grid_image, *annotated, CV_GRAY2RGB);

    cvReleaseImage(&grid_image);

    double most_horizontal = INFINITY;
    for (int i = 0; i < lines->total; ++i) {
        CvPoint *line = (CvPoint*)cvGetSeqElem(lines,i);
        double dx     = abs(line[1].x - line[0].x);
        double dy     = abs(line[1].y - line[0].y);

        double slope = INFINITY;
        if (dx != 0) {
            slope = dy / dx;
        }
        if (slope != INFINITY) {
            if (slope < most_horizontal) {
                //printf("most horizontal seen: %0.2f\n", slope);
                most_horizontal = slope;
            }
        }
    }

    int top    = -1;
    int left   = -1;
    int bottom = -1;
    int right  = -1;
    for (int i = 0; i < lines->total; i++) {
        CvPoint* line = (CvPoint*)cvGetSeqElem(lines,i);
        double dx     = abs(line[1].x - line[0].x);
        double dy     = abs(line[1].y - line[0].y);
        double slope  = INFINITY;
        if (dx) {
            slope = dy / dx;
        }

        cvLine(*annotated, line[0], line[1], CV_RGB(255, 0, 0), 1, 8, 0);
        if (abs(slope - most_horizontal) <= 1) {
            if ((top == -1) || (line[1].y < ((CvPoint*)cvGetSeqElem(lines,top))[1].y)) {
                top = i;
            }
            if ((bottom == -1) || (line[1].y > ((CvPoint*)cvGetSeqElem(lines,bottom))[1].y)) {
                bottom = i;
            }
        } else {
            if ((left == -1) || (line[1].x < ((CvPoint*)cvGetSeqElem(lines,left))[1].x)) {
                left = i;
            }
            if ((right == -1) || (line[1].x > ((CvPoint*)cvGetSeqElem(lines,right))[1].x)) {
                right = i;
            }
        }
    }
    //printf("number of lines: %d\n", lines->total);
    if ((top == -1) || (left == -1) || (bottom == -1) || (right == -1)) {
        return NULL;
    }

    CvPoint *top_line    = (CvPoint*)cvGetSeqElem(lines,top);
    cvLine(*annotated, top_line[0], top_line[1], CV_RGB(0, 0, 255), 6, 8, 0);

    CvPoint *bottom_line = (CvPoint*)cvGetSeqElem(lines,bottom);
    cvLine(*annotated, bottom_line[0], bottom_line[1], CV_RGB(0, 255, 255), 6, 8, 0);

    CvPoint *left_line   = (CvPoint*)cvGetSeqElem(lines,left);
    cvLine(*annotated, left_line[0], left_line[1], CV_RGB(0, 255, 0), 6, 8, 0);

    CvPoint *right_line  = (CvPoint*)cvGetSeqElem(lines,right);
    cvLine(*annotated, right_line[0], right_line[1], CV_RGB(255, 255, 0), 6, 8, 0);

    CvPoint2D32f *coordinates;
    coordinates = malloc(sizeof(CvPoint2D32f) * 4);

    // top left
    intersect(top_line, left_line, &(coordinates[0]));
    cvLine(*annotated, cvPointFrom32f(coordinates[0]), cvPointFrom32f(coordinates[0]), CV_RGB(255, 255, 0), 10, 8, 0);

    //printf("top_left: %.0f, %.0f\n", coordinates[0].x, coordinates[0].y);

    // top right
    intersect(top_line, right_line, &(coordinates[1]));
    cvLine(*annotated, cvPointFrom32f(coordinates[1]), cvPointFrom32f(coordinates[1]), CV_RGB(255, 255, 0), 10, 8, 0);

    //printf("top_right: %.0f, %.0f\n", coordinates[1].x, coordinates[1].y);

    // bottom right
    intersect(bottom_line, right_line, &(coordinates[2]));
    cvLine(*annotated, cvPointFrom32f(coordinates[2]), cvPointFrom32f(coordinates[2]), CV_RGB(255, 255, 0), 10, 8, 0);

    //printf("bottom_right: %.0f, %.0f\n", coordinates[2].x, coordinates[2].y);

    // bottom left
    intersect(bottom_line, left_line, &(coordinates[3]));
    cvLine(*annotated, cvPointFrom32f(coordinates[3]), cvPointFrom32f(coordinates[3]), CV_RGB(255, 255, 0), 10, 8, 0);

    //printf("bottom_left: %.0f, %.0f\n", coordinates[3].x, coordinates[3].y);

    return coordinates;
}

IplImage *square_puzzle(IplImage *in, const CvPoint2D32f *location) {
    int xsize = location[1].x - location[0].x;
    int ysize = xsize;

    CvPoint2D32f warped_coordinates[4];
    warped_coordinates[0] = cvPointTo32f(cvPoint(0,       0));
    warped_coordinates[1] = cvPointTo32f(cvPoint(xsize-1, 0));
    warped_coordinates[2] = cvPointTo32f(cvPoint(xsize-1, ysize-1));
    warped_coordinates[3] = cvPointTo32f(cvPoint(0,       ysize-1));

    CvMat *map_matrix = cvCreateMat(3, 3, CV_64FC1);
    cvGetPerspectiveTransform(location, warped_coordinates, map_matrix);

    IplImage *warped_image = cvCreateImage(cvSize(xsize, ysize), 8, in->nChannels);
    CvScalar fillval=cvScalarAll(0);
    cvWarpPerspective(in, warped_image, map_matrix, CV_WARP_FILL_OUTLIERS, fillval);

    return warped_image;
}

static int _compare_means(void *means, const void *guess_a, const void *guess_b) {
    return ((unsigned long *)means)[*((unsigned short *)guess_b)] - ((unsigned long *)means)[*((unsigned short *)guess_a)];
}

enum { PUZZLE_SIZE_MIN = 3 };
enum { PUZZLE_SIZE_MAX = 9 };

puzzle_size compute_puzzle_size(IplImage *puzzle, IplImage **annotated) {
    IplImage *threshold_image = _grid(puzzle);

    *annotated = cvCloneImage(puzzle);
    cvCvtColor(threshold_image, *annotated, CV_GRAY2RGB);

    // the logic here is to "rank" the possible sizes, by computing the average pixel intensity
    // in the vicinity of where the lines should be.
    unsigned short guess_id = 0;
    puzzle_size guesses[PUZZLE_SIZE_MAX - PUZZLE_SIZE_MIN + 1];
    unsigned long means[PUZZLE_SIZE_MAX + 1];

    const int fuzz = threshold_image->width / 50;
    for (puzzle_size guess_size = PUZZLE_SIZE_MIN; guess_size <= PUZZLE_SIZE_MAX; ++guess_size) {
        guesses[guess_id++] = guess_size;
        means[guess_size] = 0;

        for (unsigned short i = 1; i < guess_size; ++i) {
            int center = i * (threshold_image->width / guess_size);
            for (int x = 0; x < threshold_image->width; ++x) {
                for (int y = center - fuzz; y < center + fuzz; ++y) {
                    CvScalar s = cvGet2D(threshold_image, y, x);
                    means[guess_size] += s.val[0];
                }
            }
            for (int x = center - fuzz; x < center + fuzz; ++x) {
                for (int y = 0; y < threshold_image->height; ++y) {
                    CvScalar s = cvGet2D(threshold_image, y, x);
                    means[guess_size] += s.val[0];
                }
            }
        }
        means[guess_size] /= (guess_size - 1);
    }

    qsort_r(guesses, sizeof(guesses) / sizeof(puzzle_size), sizeof(puzzle_size), (void *)means, _compare_means);

    puzzle_size size = guesses[0];

    // evenly divisible sizes are easily confused. Err on the side of the larger size puzzle.
    puzzle_size confusable[][2] = { { 4, 8 }, { 3, 9 }, { 3, 6 } };
    for (int i = 0; i < (sizeof(confusable) / sizeof(puzzle_size) / 2); ++i) {
        if ((guesses[0] == confusable[i][0]) && (guesses[1] == confusable[i][1]) && (means[guesses[0]] - means[guesses[1]] < means[guesses[1]] - means[guesses[2]])) {
            size = confusable[i][1];
            break;
        }
    }

    for (unsigned short i = 1; i < size; ++i) {
        int center = i * (threshold_image->width / size);
        cvRectangle(*annotated, cvPoint(0, center - fuzz), cvPoint(threshold_image->width, center + fuzz), CV_RGB(255, 0, 0), 2, 8, 0);
        cvRectangle(*annotated, cvPoint(center - fuzz, 0), cvPoint(center + fuzz, threshold_image->height), CV_RGB(255, 0, 0), 2, 8, 0);
    }
    cvReleaseImage(&threshold_image);

    return size;
}

typedef enum {
    LEFT,
    RIGHT,
    TOP,
    BOTTOM
} border_direction;

static short _border_dx(border_direction b) {
    switch (b) {
         case LEFT:  return -1;
         case RIGHT: return 1;
         default:    return 0;
    }
}

static short _border_dy(border_direction b) {
    switch (b) {
        case TOP:    return -1;
        case BOTTOM: return 1;
        default:     return 0;
    }
}

static void _explore_cage(int cage_id, int box_x, int box_y, int cage_ids[PUZZLE_SIZE_MAX][PUZZLE_SIZE_MAX], short cage_borders[PUZZLE_SIZE_MAX][PUZZLE_SIZE_MAX][4]) {
    if (cage_ids[box_x][box_y] == cage_id) {
        // been here already
        return;
    }
    cage_ids[box_x][box_y] = cage_id;

    for (border_direction b = LEFT; b <= BOTTOM; ++b) {
        if (! cage_borders[box_x][box_y][b]) {
            _explore_cage(cage_id, box_x + _border_dx(b), box_y + _border_dy(b), cage_ids, cage_borders);
        }
    }
    return;
}

static CvScalar _getpixel(IplImage *threshold_image, IplImage **annotated, border_direction d, int across, int along) {
    CvScalar pixel;
    if (d == BOTTOM) {
        pixel = cvGet2D(threshold_image, across, along);
        if (pixel.val[0] == 0) {
            cvSet2D(*annotated, across, along, CV_RGB(255, 0, 0));
        } else {
            cvSet2D(*annotated, across, along, CV_RGB(0, 0, 255));
        }
    } else {
        pixel = cvGet2D(threshold_image, along, across);
        if (pixel.val[0] == 0) {
            cvSet2D(*annotated, along, across, CV_RGB(255, 0, 0));
        } else {
            cvSet2D(*annotated, along, across, CV_RGB(0, 0, 255));
        }
    }
    return pixel;
}

static short *_getborder(short cage_borders[PUZZLE_SIZE_MAX][PUZZLE_SIZE_MAX][4], border_direction d, int across, int along) {
    if ((d == BOTTOM) || (d == TOP)) {
        return &(cage_borders[along][across][d]);
    }
    return &(cage_borders[across][along][d]);
}

static void _find_cage_borders(IplImage *threshold_image, IplImage **annotated, puzzle_size size, border_direction direction, border_direction opposite, short cage_borders[PUZZLE_SIZE_MAX][PUZZLE_SIZE_MAX][4]) {
    assert(threshold_image->height == threshold_image->width);

    int px_size = threshold_image->height;

    // first figure out, for this puzzle, the difference between a cage border and
    // a regular edge. We'll do this via the mean intensity of the rough location
    // where we expect the edges to be.
    int fuzz_along  = px_size / (size * 3.9);
    int fuzz_across = px_size / (size * 4.0);

    int means[size][size];
    int mean_max = -1;
    int mean_min = -1;
    for (int box_along = 0; box_along < size; ++box_along) {
        int along_center = (2 * box_along + 1) * (px_size / size / 2);
        for (int box_across = 0; box_across < (size - 1); ++box_across) {
            int across_center = (box_across + 1) * (px_size / size);

            long total = 0;
            for (int across = across_center - fuzz_across; across <= across_center + fuzz_across; ++across) {
                for (int along = along_center - fuzz_along; along <= along_center + fuzz_along; ++along) {
                    CvScalar s = _getpixel(threshold_image, annotated, direction, across, along);
                    total += s.val[0];
                }
            }

            int mean = total / ((2 * fuzz_along + 1) * (2 * fuzz_across + 1));
            means[box_across][box_along] = mean;
            if ((mean_max == -1) || (mean > mean_max)) {
                mean_max = mean;
            }
            if ((mean_min == -1) || (mean < mean_min)) {
                mean_min = mean;
            }
        }
    }

    for (int box_along = 0; box_along < size; ++box_along) {
        int box_across;
        for (box_across = 0; box_across < (size - 1); ++box_across) {
            int delta_min = abs(means[box_across][box_along] - mean_min);
            int delta_max = abs(means[box_across][box_along] - mean_max);
            *(_getborder(cage_borders, direction, box_across, box_along)) = (delta_max < delta_min);
        }
        *(_getborder(cage_borders, direction, box_across, box_along)) = 1;
    }
    for (int box_along = 0; box_along < size; ++box_along) {
        int box_across = 0;
        *(_getborder(cage_borders, opposite, box_across, box_along)) = 1;
        for (box_across = 1; box_across < size; ++box_across) {
            *(_getborder(cage_borders, opposite, box_across, box_along)) = *(_getborder(cage_borders, direction, box_across - 1, box_along));
        }
    }

    return;
}

char *compute_puzzle_cages(IplImage *puzzle, puzzle_size size, IplImage **annotated) {
    IplImage *threshold_image = _grid(puzzle);

    *annotated = cvCloneImage(puzzle);
    cvCvtColor(threshold_image, *annotated, CV_GRAY2RGB);

    short cage_borders[PUZZLE_SIZE_MAX][PUZZLE_SIZE_MAX][4];

    _find_cage_borders(threshold_image, annotated, size, RIGHT, LEFT, cage_borders);
    _find_cage_borders(threshold_image, annotated, size, BOTTOM, TOP, cage_borders);

    int cage_ids[PUZZLE_SIZE_MAX][PUZZLE_SIZE_MAX];
    for (int box_x = 0; box_x < size; ++box_x) {
        for (int box_y = 0; box_y < size; ++box_y) {
            cage_ids[box_x][box_y] = -1;
        }
    }

    // pass through the puzzle from top to bottom, left to right, propagating cage_id values
    // through edges which are not cage borders.
    int next_cage_id = 0;
    for (int box_y = 0; box_y < size; ++box_y) {
        for (int box_x = 0; box_x < size; ++box_x) {
            if (cage_ids[box_x][box_y] == -1) {
                _explore_cage(next_cage_id++, box_x, box_y, cage_ids, cage_borders);
            }
        }
    }

    // serialize the cages into string representation
    static char puzzle_cages[PUZZLE_SIZE_MAX * PUZZLE_SIZE_MAX + 1];
    static char cage_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int i = 0;
    for (int box_y = 0; box_y < size; ++box_y) {
        for (int box_x = 0; box_x < size; ++box_x) {
            puzzle_cages[i++] = cage_names[cage_ids[box_x][box_y]];
        }
    }
    puzzle_cages[i] = 0;

    cvReleaseImage(&threshold_image);
    return puzzle_cages;
}

void showSmaller (IplImage *in, char *window_name) {
    double factor = 1;
    if (in->height > 700.) {
        factor = 700. / in->height;
    }

    int width = (int)(in->width * factor);
    int height = (int)(in->height * factor);

    IplImage *smaller = cvCreateImage(cvSize(width, height), 8, in->nChannels);
    cvResize(in, smaller, CV_INTER_LINEAR);
    cvShowImage(window_name, smaller);
    cvReleaseImage(&smaller);
}
