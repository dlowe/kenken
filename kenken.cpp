#include "cv.h"
#include "Blob.h"
#include "BlobResult.h"
#include "kenken.h"

void _locate_puzzle_blob(IplImage *in, CBlob *currentBlob) {
    IplImage *img = cvCreateImage(cvGetSize(in), 8, 1);

    // convert to grayscale
    cvCvtColor(in, img, CV_BGR2GRAY);

    // apply thresholding (converts it to a binary image)
    cvAdaptiveThreshold(img, img, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 59, 12);
    // cvNamedWindow("thresh", 1);
    // showSmaller(img, "thresh");

    // find all blobs in the image
    CBlobResult blobs;
    blobs = CBlobResult(img, NULL, 0, true);
    cvReleaseImage(&img);

    // only interested in blobs of ink (not paper)
    // (recall that this is a binary image)
    blobs.Filter(blobs, B_INCLUDE, CBlobGetMean(), B_EQUAL, 0);
    blobs.GetNthBlob(CBlobGetArea(), 0, *currentBlob);

    return;
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
    CBlob currentBlob;
    _locate_puzzle_blob(in, &currentBlob);

    // draw the blob onto an otherwise blank image
    IplImage *hough_image = cvCreateImage(cvGetSize(in), 8, 1);
    currentBlob.FillBlob(hough_image, CV_RGB(255, 255, 255));

    //cvNamedWindow("hough", 1);
    //showSmaller(hough_image, "hough");

    // find lines using Hough transform
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* lines = 0;

    int minimum_line_length = in->width / 2;
    lines = cvHoughLines2(hough_image, storage, CV_HOUGH_PROBABILISTIC, 1, CV_PI/90, 60, minimum_line_length, 40);
    //cvReleaseImage( &hough_image );

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
        double length = sqrt(pow(dx, 2) + pow(dy, 2));
        double slope  = INFINITY;
        if (dx) {
            slope = dy / dx;
        }

        //cvLine(in, line[0], line[1], CV_RGB(255, 255, 255), 1);
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
    coordinates = new CvPoint2D32f[4];

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

void showSmaller (IplImage *in, char *window_name) {
    IplImage *smaller = cvCreateImage(cvSize(in->width / 2, in->height / 2), 8, in->nChannels);
    cvResize(in, smaller, CV_INTER_LINEAR);
    cvShowImage(window_name, smaller);
    cvReleaseImage(&smaller);
}