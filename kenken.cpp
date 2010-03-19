#include "cv.h"
#include "highgui.h"
#include "Blob.h"
#include "BlobResult.h"

void showSmaller (IplImage *in, char *window_name) {
    IplImage *smaller = cvCreateImage(cvSize(in->width / 2, in->height / 2), 8, in->nChannels);
    cvResize(in, smaller, CV_INTER_LINEAR);
    cvShowImage(window_name, smaller);
    cvReleaseImage(&smaller);
}

#define min(x, y) ((x < y) ? x : y)
#define max(x, y) ((x > y) ? x : y)

const CvPoint2D32f* locate_puzzle(const IplImage *in) {
    IplImage *img = cvCreateImage( cvGetSize(in), 8, 1 );

    // convert to grayscale
    cvCvtColor( in, img, CV_BGR2GRAY );

    // apply thresholding (converts it to a binary image)
    cvThreshold(img, img, 30, 255, CV_THRESH_BINARY_INV);

    // find all blobs in the image
    CBlobResult blobs;
    blobs = CBlobResult( img, NULL, 0, false );
    cvReleaseImage( &img );

    // only interested in blobs of ink (not paper)
    // (recall that this is a binary image)
    blobs.Filter( blobs, B_INCLUDE, CBlobGetMean(), B_EQUAL, 255);
    CBlob currentBlob;
    blobs.GetNthBlob(CBlobGetArea(), 0, currentBlob);

    // draw the blob onto an otherwise blank image
    IplImage *hough_image = cvCreateImage(cvGetSize(in), 8, 1);
    currentBlob.FillBlob(hough_image, CV_RGB(255, 255, 255));

    // find lines using Hough transform
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* lines = 0;
    lines = cvHoughLines2(hough_image, storage, CV_HOUGH_PROBABILISTIC, 1, CV_PI/180, 600, 50, 40);
    cvReleaseImage( &hough_image );

    double longest         = 0;
    double most_horizontal = INFINITY;
    for (int i = 0; i < lines->total; ++i) {
        CvPoint *line = (CvPoint*)cvGetSeqElem(lines,i);
        double dx     = abs(line[1].x - line[0].x);
        double dy     = abs(line[1].y - line[0].y);
        double length = sqrt(pow(dx, 2) + pow(dy, 2));
        if (length > longest) {
             //printf("longest seen: %0.2f\n", length);
             longest = length;
        }

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

        if ((longest - length) < 200) {
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
    }
    assert(top != -1);
    assert(left != -1);
    assert(bottom != -1);
    assert(right != -1);

    CvPoint *top_line    = (CvPoint*)cvGetSeqElem(lines,top);
    CvPoint *bottom_line = (CvPoint*)cvGetSeqElem(lines,bottom);
    CvPoint *left_line   = (CvPoint*)cvGetSeqElem(lines,left);
    CvPoint *right_line  = (CvPoint*)cvGetSeqElem(lines,right);

    CvPoint2D32f *coordinates;
    coordinates = new CvPoint2D32f[4];

    // top left
    coordinates[0].x = min(top_line[0].x, left_line[1].x);
    coordinates[0].y = min(top_line[0].y, left_line[1].y);

    // top right
    coordinates[1].x = max(top_line[1].x, right_line[1].x);
    coordinates[1].y = min(top_line[1].y, right_line[1].y);

    // bottom right
    coordinates[2].x = max(bottom_line[1].x, right_line[0].x);
    coordinates[2].y = max(bottom_line[1].y, right_line[0].y);

    // bottom left
    coordinates[3].x = min(bottom_line[0].x, left_line[0].x);
    coordinates[3].y = max(bottom_line[0].y, left_line[1].y);

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

int main( int argc, char** argv ) {
    IplImage *color_image = cvLoadImage("IMG_0642.JPG", 1);

    IplImage *puzzle = square_puzzle(color_image, locate_puzzle(color_image));

    cvNamedWindow( "Image view", 1 );
    showSmaller(puzzle, "Image view");
    cvWaitKey(0);

    cvDestroyWindow( "Image view" );
    cvReleaseImage( &puzzle );
    cvReleaseImage( &color_image );

    exit(0);
}
