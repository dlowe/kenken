#include <stdio.h>

#include "cv.h"
#include "kenken.h"
#include "c_blob.h"

IplImage *_threshold(IplImage *in) {
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
    int block_size = 139;
    // constant_reduction observations: magic, but adapting this value to the mean intensity of the
    //   image as a whole seems to help.
    int constant_reduction = (int)(mean_intensity / 3.6 + 0.5);

    IplImage *threshold_image = cvCreateImage(cvGetSize(img), 8, 1);
    cvAdaptiveThreshold(img, threshold_image, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY,
        block_size, constant_reduction);
    cvReleaseImage(&img);

    // before blobbing, let's try to get rid of "noise" spots. The blob algorithm is very slow unless
    // these are cleaned up...
    int min_blob_size = 2;
    for (int x = 0; x < threshold_image->width; ++x) {
        for (int y = 0; y < threshold_image->height; ++y) {
            CvScalar s = cvGet2D(threshold_image, y, x);
            int ink_neighbors = 0;
            if (s.val[0] == 0) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if ((x + dx >= 0) && (x + dx < threshold_image->width)) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            if ((y + dy >= 0) && (y + dy < threshold_image->height)) {
                                if (! ((dy == 0) && (dx == 0))) {
                                    CvScalar m = cvGet2D(threshold_image, y + dy, x + dx);
                                    if (m.val[0] == 0) {
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
                    s.val[0] = 255;
                    cvSet2D(threshold_image, y, x, s);
                }
            }
        }
    }

    return threshold_image;
}

c_blob *_locate_puzzle_blob(IplImage *in) {
    IplImage *threshold_image = _threshold(in);

    //cvNamedWindow("thresh", 1);
    //showSmaller(threshold_image, "thresh");

    c_blob *blob = c_blob_get_biggest_ink_blob(threshold_image);
    cvReleaseImage(&threshold_image);
    return blob;
}

void intersect(CvPoint *a, CvPoint *b, CvPoint2D32f *i) {
   int x[5] = { 0, a[0].x, a[1].x, b[0].x, b[1].x };
   int y[5] = { 0, a[0].y, a[1].y, b[0].y, b[1].y };

   // http://en.wikipedia.org/wiki/Line-line_intersection
   i->x = (((( x[1] * y[2] ) - ( y[1] * x[2] )) * (x[3] - x[4])) - ((x[1] - x[2]) * ((x[3] * y[4]) - (y[3] * x[4])))) / (((x[1] - x[2]) * (y[3] - y[4])) - ((y[1] - y[2]) * (x[3] - x[4])));
   i->y = (((( x[1] * y[2] ) - ( y[1] * x[2] )) * (y[3] - y[4])) - ((y[1] - y[2]) * ((x[3] * y[4]) - (y[3] * x[4])))) / (((x[1] - x[2]) * (y[3] - y[4])) - ((y[1] - y[2]) * (x[3] - x[4])));

   return;
}

const CvPoint2D32f* locate_puzzle(IplImage *in) {
    c_blob *currentBlob = _locate_puzzle_blob(in);

    // draw the blob onto an otherwise blank image
    IplImage *hough_image = cvCreateImage(cvGetSize(in), 8, 1);
    c_blob_fill(currentBlob, hough_image);
    c_blob_destroy(currentBlob);

    //cvNamedWindow("hough", 1);
    //showSmaller(hough_image, "hough");

    // find lines using Hough transform
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* lines = 0;

    int minimum_line_length = in->width / 2;
    lines = cvHoughLines2(hough_image, storage, CV_HOUGH_PROBABILISTIC, 1, CV_PI/90, 60, minimum_line_length, 40);
    cvReleaseImage(&hough_image);

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

        //cvLine(in, line[0], line[1], CV_RGB(255, 255, 255), 1, 8, 0);
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
    if ((top == -1) || (left == -1) || (bottom == -1) || (right == -1)) {
        return NULL;
    }

    CvPoint *top_line    = (CvPoint*)cvGetSeqElem(lines,top);
    //cvLine(in, top_line[0], top_line[1], CV_RGB(0, 0, 255), 1);

    CvPoint *bottom_line = (CvPoint*)cvGetSeqElem(lines,bottom);
    //cvLine(in, bottom_line[0], bottom_line[1], CV_RGB(0, 255, 255), 1);

    CvPoint *left_line   = (CvPoint*)cvGetSeqElem(lines,left);
    //cvLine(in, left_line[0], left_line[1], CV_RGB(0, 255, 0), 1);

    CvPoint *right_line  = (CvPoint*)cvGetSeqElem(lines,right);
    //cvLine(in, right_line[0], right_line[1], CV_RGB(255, 0, 0), 1);

    CvPoint2D32f *coordinates;
    coordinates = malloc(sizeof(CvPoint2D32f) * 4);

    // top left
    intersect(top_line, left_line, &(coordinates[0]));
    //cvLine(in, cvPointFrom32f(coordinates[0]), cvPointFrom32f(coordinates[0]), CV_RGB(255, 255, 255), 3);

    //printf("top_left: %.0f, %.0f\n", coordinates[0].x, coordinates[0].y);

    // top right
    intersect(top_line, right_line, &(coordinates[1]));
    //cvLine(in, cvPointFrom32f(coordinates[1]), cvPointFrom32f(coordinates[1]), CV_RGB(255, 255, 255), 3);

    //printf("top_right: %.0f, %.0f\n", coordinates[1].x, coordinates[1].y);

    // bottom right
    intersect(bottom_line, right_line, &(coordinates[2]));
    //cvLine(in, cvPointFrom32f(coordinates[2]), cvPointFrom32f(coordinates[2]), CV_RGB(255, 255, 255), 3);

    //printf("bottom_right: %.0f, %.0f\n", coordinates[2].x, coordinates[2].y);

    // bottom left
    intersect(bottom_line, left_line, &(coordinates[3]));
    //cvLine(in, cvPointFrom32f(coordinates[3]), cvPointFrom32f(coordinates[3]), CV_RGB(255, 255, 255), 3);

    //printf("bottom_left: %.0f, %.0f\n", coordinates[3].x, coordinates[3].y);
    //cvNamedWindow("hough", 1);
    //showSmaller(in, "hough");

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

int _compare_means(void *means, const void *guess_a, const void *guess_b) {
    return ((unsigned long *)means)[*((unsigned short *)guess_a)] - ((unsigned long *)means)[*((unsigned short *)guess_b)];
}

unsigned short compute_puzzle_size(IplImage *puzzle) {
    IplImage *threshold_image = _threshold(puzzle);

    // the logic here is to "rank" the possible sizes, by computing the average pixel intensity
    // in the vicinity of where the lines should be.
    unsigned long means[10];
    const int fuzz = 8;
    for (unsigned short guess_size = 3; guess_size <= 9; ++guess_size) {
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
        //cvNamedWindow("thresh", 1);
        //showSmaller(threshold_image, "thresh");
    }
    cvReleaseImage(&threshold_image);

    unsigned short guesses[] = { 3, 4, 5, 6, 7, 8, 9 };
    qsort_r(guesses, 7, sizeof(unsigned short), (void *)means, _compare_means);

    //for (int i = 0; i < 7; ++i) {
        //printf("%d: %lu\n", guesses[i], means[guesses[i]]);
    //}

    // evenly divisible sizes are easily confused. Err on the side of the larger size puzzle.
    if ((guesses[0] == 4) && (guesses[1] == 8) && (means[guesses[1]] - means[guesses[0]] < means[guesses[2]] - means[guesses[1]])) {
        return 8;
    }
    if ((guesses[0] == 3) && (guesses[1] == 9) && (means[guesses[1]] - means[guesses[0]] < means[guesses[2]] - means[guesses[1]])) {
        return 9;
    }
    if ((guesses[0] == 3) && (guesses[1] == 6) && (means[guesses[1]] - means[guesses[0]] < means[guesses[2]] - means[guesses[1]])) {
        return 6;
    }

    return guesses[0];
}

static char puzzle_cages[9 * 9 + 1];
static char cage_names[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
char *compute_puzzle_cages(IplImage *puzzle, unsigned short puzzle_size) {
    IplImage *threshold_image = _threshold(puzzle);

    // first figure out, for this puzzle, the difference between a cage edge and
    // a regular edge. We'll do this via the mean intensity of the rough location
    // where we expect the edges to be.
    int fuzz = threshold_image->height / 40;


    // look at the right edge of each box
    int right_mean_max = -1;
    int right_mean_min = -1;
    int right_means[puzzle_size - 1][puzzle_size];
    for (int box_y = 0; box_y < puzzle_size; ++box_y) {
        int y_center = (2 * box_y + 1) * (threshold_image->height / puzzle_size / 2);
        for (int box_x = 0; box_x < (puzzle_size - 1); ++box_x) {
            int x_center = (box_x + 1) * (threshold_image->width / puzzle_size);

            long total = 0;
            for (int x = x_center - fuzz; x <= x_center + fuzz; ++x) {
                for (int y = y_center - fuzz; y <= y_center + fuzz; ++y) {
                    CvScalar s = cvGet2D(threshold_image, y, x);
                    total += s.val[0];
                    //cvSet2D(threshold_image, y, x, CV_RGB(255, 255, 255));
                }
            }

            int mean = total / ((2 * fuzz +1) * (2 * fuzz + 1));
            right_means[box_x][box_y] = mean;
            if ((right_mean_max == -1) || (mean > right_mean_max)) {
                right_mean_max = mean;
            }
            if ((right_mean_min == -1) || (mean < right_mean_min)) {
                right_mean_min = mean;
            }
        }
    }

    short right_is_cage[puzzle_size][puzzle_size];
    for (int box_y = 0; box_y < puzzle_size; ++box_y) {
        for (int box_x = 0; box_x < (puzzle_size - 1); ++box_x) {
            int delta_min = abs(right_means[box_x][box_y] - right_mean_min);
            int delta_max = abs(right_means[box_x][box_y] - right_mean_max);
            if (delta_min < delta_max) {
                right_is_cage[box_x][box_y] = 1;
            } else {
                right_is_cage[box_x][box_y] = 0;
            }
            //printf("(%d, %d) right edge mean = %d, is_cage = %d\n", box_x, box_y, right_means[box_x][box_y], right_is_cage[box_x][box_y]);
        }
        right_is_cage[puzzle_size - 1][box_y] = 1;
        //printf("(%d, %d) right edge is_cage = 1\n", puzzle_size - 1, box_y);
    }

    // look at the bottom edge of each box
    int bottom_means[puzzle_size][puzzle_size - 1];
    int bottom_mean_min = -1;
    int bottom_mean_max = -1;
    for (int box_x = 0; box_x < puzzle_size; ++box_x) {
        int x_center = (2 * box_x + 1) * (threshold_image->width / puzzle_size / 2);
        for (int box_y = 0; box_y < (puzzle_size - 1); ++box_y) {
            int y_center = (box_y + 1) * (threshold_image->height / puzzle_size);

            long total = 0;
            for (int x = x_center - fuzz; x <= x_center + fuzz; ++x) {
                for (int y = y_center - fuzz; y <= y_center + fuzz; ++y) {
                    CvScalar s = cvGet2D(threshold_image, y, x);
                    total += s.val[0];
                    //cvSet2D(threshold_image, y, x, CV_RGB(255, 255, 255));
                }
            }

            int mean = total / ((2 * fuzz +1) * (2 * fuzz + 1));
            bottom_means[box_x][box_y] = mean;
            if ((bottom_mean_max == -1) || (mean > bottom_mean_max)) {
                bottom_mean_max = mean;
            }
            if ((bottom_mean_min == -1) || (mean < bottom_mean_min)) {
                bottom_mean_min = mean;
            }
        }
    }

    short bottom_is_cage[puzzle_size][puzzle_size];
    for (int box_x = 0; box_x < puzzle_size; ++box_x) {
        for (int box_y = 0; box_y < (puzzle_size - 1); ++box_y) {
            int delta_min = abs(bottom_means[box_x][box_y] - bottom_mean_min);
            int delta_max = abs(bottom_means[box_x][box_y] - bottom_mean_max);
            if (delta_min < delta_max) {
                bottom_is_cage[box_x][box_y] = 1;       
            } else {
                bottom_is_cage[box_x][box_y] = 0;
            }
            //printf("(%d, %d) bottom edge mean = %d, is_cage = %d\n", box_x, box_y, bottom_means[box_x][box_y], bottom_is_cage[box_x][box_y]);
        }
        bottom_is_cage[box_x][puzzle_size - 1] = 1;
        //printf("(%d, %d) bottom edge is_cage = 1\n", box_x, puzzle_size - 1);
    }

    //cvNamedWindow("thresh", 1);
    //showSmaller(threshold_image, "thresh");

    int i = 0;
    int next_cage_id = 0;
    int cage_ids[puzzle_size][puzzle_size];
    for (int box_x = 0; box_x < puzzle_size; ++box_x) {
        for (int box_y = 0; box_y < puzzle_size; ++box_y) {
            cage_ids[box_x][box_y] = -1;
        }
    }

    // pass through the puzzle from top to bottom, left to right, propagating cage_id values to the
    // bottom and the right through edges which are not cage edges.
    // at the same time, serialize into the string representation (puzzle_cages)
    for (int box_y = 0; box_y < puzzle_size; ++box_y) {
        for (int box_x = 0; box_x < puzzle_size; ++box_x) {
            int cage_id = cage_ids[box_x][box_y];
            if (cage_id == -1) {
                cage_id = next_cage_id++;
            }
            if (right_is_cage[box_x][box_y] == 0) {
                cage_ids[box_x + 1][box_y] = cage_id;
            }
            if (bottom_is_cage[box_x][box_y] == 0) {
                cage_ids[box_x][box_y + 1] = cage_id;
            }
            cage_ids[box_x][box_y] = cage_id;
            puzzle_cages[i++] = cage_names[cage_ids[box_x][box_y]];
        }
    }
    puzzle_cages[i] = 0;

    cvReleaseImage(&threshold_image);
    return puzzle_cages;
}

void showSmaller (IplImage *in, char *window_name) {
    IplImage *smaller = cvCreateImage(cvSize(in->width / 2, in->height / 2), 8, in->nChannels);
    cvResize(in, smaller, CV_INTER_LINEAR);
    cvShowImage(window_name, smaller);
    cvReleaseImage(&smaller);
}
