#include "cv.h"
#include "highgui.h"

#include "kenken.h"

int main( int argc, char** argv ) {
    if (argc == 2) {
        IplImage *color_image = cvLoadImage(argv[1], 1);

        const CvPoint2D32f *location = locate_puzzle(color_image);
        if (location) {
            IplImage *puzzle = square_puzzle(color_image, location);

            cvNamedWindow( "Image view", 1 );
            showSmaller(puzzle, "Image view");
            cvWaitKey(0);

            cvDestroyWindow( "Image view" );
            cvReleaseImage( &puzzle );
        }
        cvReleaseImage( &color_image );
    }
    exit(0);
}
