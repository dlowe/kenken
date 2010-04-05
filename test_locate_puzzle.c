#include <stdarg.h>

#include "c_blob.h"
#include "cv.h"
#include "highgui.h"
#include "kenken.h"
#include "yaml.h"

#define BLOB_FUZZ 33
#define LOCATION_FUZZ 22

typedef struct test_case_s {
    char           *image;
    CvPoint         puzzle_location[4];
    CvPoint         blob_area[2];
    unsigned short  size;
    unsigned short  failing;
} test_case_t;

static unsigned int test_n = 1;
static unsigned int fail_n = 0;
unsigned short ok (unsigned short condition, char *description, ...) {
    va_list args;
    char description_buffer[1000];

    va_start(args, description);
    vsnprintf(description_buffer, 1000, description, args);
    va_end(args);

    printf("%sok %d - %s\n", condition ? "" : "not ", test_n++, description_buffer);

    fail_n += (! condition);

    return condition;
}

extern void showSmaller (IplImage *in, char *window_name);

int main (int argc, char** argv) {
    yaml_parser_t parser;
    yaml_document_t document;

    yaml_parser_initialize(&parser);
    FILE *input = fopen("test/test.yaml", "r");
    yaml_parser_set_input_file(&parser, input);
    yaml_parser_load(&parser, &document);
    yaml_parser_delete(&parser);
    fclose(input);

    yaml_node_t *n = yaml_document_get_root_node(&document);
    if (n == NULL) {
        exit(255);
    }
    if (n->type != YAML_SEQUENCE_NODE) {
        exit(255);
    }
    for (yaml_node_item_t *test_case_id = n->data.sequence.items.start; test_case_id < n->data.sequence.items.top; ++test_case_id) {
        test_case_t  test_case;
        test_case.image = NULL;
        test_case.failing = 0;
        test_case.puzzle_location[0] = cvPoint(0, 0);
        test_case.puzzle_location[1] = cvPoint(0, 0);
        test_case.puzzle_location[2] = cvPoint(0, 0);
        test_case.puzzle_location[3] = cvPoint(0, 0);
        test_case.blob_area[0] = cvPoint(0, 0);
        test_case.blob_area[1] = cvPoint(0, 0);
        test_case.size = 0;

        yaml_node_t *test_case_node = yaml_document_get_node(&document, *test_case_id);
        if (test_case_node->type != YAML_MAPPING_NODE) {
            exit(255);
        }
        for (yaml_node_pair_t *pair = test_case_node->data.mapping.pairs.start; pair < test_case_node->data.mapping.pairs.top; ++pair) {
            yaml_node_t *key   = yaml_document_get_node(&document, pair->key);
            yaml_node_t *value = yaml_document_get_node(&document, pair->value);

            if (key->type != YAML_SCALAR_NODE) {
                exit(255);
            }
            if (strcmp((const char *)key->data.scalar.value, "image") == 0) {
                test_case.image = (char *)value->data.scalar.value;
            }
            if (strcmp((const char *)key->data.scalar.value, "size") == 0) {
                test_case.size  = atoi((char *)value->data.scalar.value);
            }
            if (strcmp((const char *)key->data.scalar.value, "failing") == 0) {
                test_case.failing = 1;
            }
            if (strcmp((const char *)key->data.scalar.value, "puzzle_location") == 0) {
                if (value->type != YAML_SEQUENCE_NODE) {
                    printf("puzzle_location value should be a sequence of points\n");
                    exit(255);
                }
                int i = 0;
                for (yaml_node_item_t *point_id = value->data.sequence.items.start; point_id < value->data.sequence.items.top; ++point_id) {
                    if (i > 3) {
                        printf("puzzle_location should specify exactly 4 points (got more)\n");
                        exit(255);
                    }

                    yaml_node_t *point_node = yaml_document_get_node(&document, *point_id);

                    if (point_node->type != YAML_SEQUENCE_NODE) {
                        printf("puzzle_location point[%d] should be a sequence\n", i);
                        exit(255);
                    }
                    yaml_node_item_t *x_id = point_node->data.sequence.items.start;
                    yaml_node_t *x_node = yaml_document_get_node(&document, *x_id);
                    if (x_node->type != YAML_SCALAR_NODE) {
                        printf("puzzle_location point[%d].x should be a scalar\n", i);
                        exit(255);
                    }
                    yaml_node_item_t *y_id = x_id + 1;
                    yaml_node_t *y_node = yaml_document_get_node(&document, *y_id);
                    if (y_node->type != YAML_SCALAR_NODE) {
                        printf("puzzle_location point[%d].y should be a scalar\n", i);
                        exit(255);
                    }

                    test_case.puzzle_location[i].x = atoi((const char *)x_node->data.scalar.value);
                    test_case.puzzle_location[i].y = atoi((const char *)y_node->data.scalar.value);
                    ++i;
                }
                if (i != 4) {
                    printf("puzzle_location should specify exactly 4 points (got %d)\n", (i+1));
                    exit(255);
                }
            }
            if (strcmp((const char *)key->data.scalar.value, "blob_area") == 0) {
                if (value->type != YAML_SEQUENCE_NODE) {
                    printf("blob_area value should be a sequence of points\n");
                    exit(255);
                }
                int i = 0;
                for (yaml_node_item_t *point_id = value->data.sequence.items.start; point_id < value->data.sequence.items.top; ++point_id) {
                    if (i > 1) {
                        printf("blob_area should specify exactly 2 points (got more)\n");
                        exit(255);
                    }

                    yaml_node_t *point_node = yaml_document_get_node(&document, *point_id);

                    if (point_node->type != YAML_SEQUENCE_NODE) {
                        printf("blob_area point[%d] should be a sequence\n", i);
                        exit(255);
                    }
                    yaml_node_item_t *x_id = point_node->data.sequence.items.start;
                    yaml_node_t *x_node = yaml_document_get_node(&document, *x_id);
                    if (x_node->type != YAML_SCALAR_NODE) {
                        printf("blob_area point[%d].x should be a scalar\n", i);
                        exit(255);
                    }
                    yaml_node_item_t *y_id = x_id + 1;
                    yaml_node_t *y_node = yaml_document_get_node(&document, *y_id);
                    if (y_node->type != YAML_SCALAR_NODE) {
                        printf("blob_area point[%d].y should be a scalar\n", i);
                        exit(255);
                    }

                    test_case.blob_area[i].x = atoi((const char *)x_node->data.scalar.value);
                    test_case.blob_area[i].y = atoi((const char *)y_node->data.scalar.value);
                    ++i;
                }
                if (i != 2) {
                    printf("blob_area should specify exactly 2 points (got %d)\n", (i+1));
                    exit(255);
                }
            }
        }

        if (test_case.image == NULL) {
            printf("incomplete test case (missing 'image')\n");
            exit(255);
        }

        if (test_case.failing) {
            continue;
        }

        IplImage *color_image = cvLoadImage(test_case.image, 1);
        c_blob *blob = _locate_puzzle_blob(color_image);

        ok(abs(c_blob_minx(blob) - test_case.blob_area[0].x) < BLOB_FUZZ, "%s: blob top/left: x=%.0f, expecting %d", test_case.image, c_blob_minx(blob), test_case.blob_area[0].x);
        ok(abs(c_blob_miny(blob) - test_case.blob_area[0].y) < BLOB_FUZZ, "%s: blob top/left: y=%.0f, expecting %d", test_case.image, c_blob_miny(blob), test_case.blob_area[0].y);
        ok(abs(c_blob_maxx(blob) - test_case.blob_area[1].x) < BLOB_FUZZ, "%s: blob bottom/right: x=%.0f, expecting %d", test_case.image, c_blob_maxx(blob), test_case.blob_area[1].x);
        ok(abs(c_blob_maxy(blob) - test_case.blob_area[1].y) < BLOB_FUZZ, "%s: blob bottom/right: y=%.0f, expecting %d", test_case.image, c_blob_maxy(blob), test_case.blob_area[1].y);

        if (fail_n) {
            cvNamedWindow("result", 1);

            // expected
            cvRectangle(color_image, test_case.blob_area[0], test_case.blob_area[1], CV_RGB(0,255,0), 1, 8, 0);

            // actual
            c_blob_fill(blob, color_image);

            showSmaller(color_image, "result");
            cvWaitKey(0);
            cvDestroyWindow("result");
            exit(fail_n);
        }

        const CvPoint2D32f *actual_location = locate_puzzle(color_image);

        if (ok(actual_location != NULL, "%s: puzzle found", test_case.image)) {
            for (int i = 0; i < 4; ++i) {
                ok(abs(actual_location[i].x - test_case.puzzle_location[i].x) < LOCATION_FUZZ, "%s: point %d: x=%.0f, expecting %d", test_case.image, i, actual_location[i].x, test_case.puzzle_location[i].x);
                ok(abs(actual_location[i].y - test_case.puzzle_location[i].y) < LOCATION_FUZZ, "%s: point %d: y=%.0f, expecting %d", test_case.image, i, actual_location[i].y, test_case.puzzle_location[i].y);
            }
        }

        if (fail_n) {
            cvNamedWindow("result", 1);

            // expected
            cvLine(color_image, test_case.puzzle_location[0], test_case.puzzle_location[1], CV_RGB(0,255,0), 1, 8, 0);
            cvLine(color_image, test_case.puzzle_location[1], test_case.puzzle_location[2], CV_RGB(0,255,0), 1, 8, 0);
            cvLine(color_image, test_case.puzzle_location[2], test_case.puzzle_location[3], CV_RGB(0,255,0), 1, 8, 0);
            cvLine(color_image, test_case.puzzle_location[3], test_case.puzzle_location[0], CV_RGB(0,255,0), 1, 8, 0);

            if (actual_location != NULL) {
                // actual
                cvLine(color_image, cvPointFrom32f(actual_location[0]), cvPointFrom32f(actual_location[1]), CV_RGB(255,0,0), 1, 8, 0);
                cvLine(color_image, cvPointFrom32f(actual_location[1]), cvPointFrom32f(actual_location[2]), CV_RGB(255,0,0), 1, 8, 0);
                cvLine(color_image, cvPointFrom32f(actual_location[2]), cvPointFrom32f(actual_location[3]), CV_RGB(255,0,0), 1, 8, 0);
                cvLine(color_image, cvPointFrom32f(actual_location[3]), cvPointFrom32f(actual_location[0]), CV_RGB(255,0,0), 1, 8, 0);
            }

            showSmaller(color_image, "result");

            cvWaitKey(0);
            cvDestroyWindow("result");
            exit(fail_n);
        }

        IplImage *squared_puzzle = square_puzzle(color_image, actual_location);

        unsigned int actual_size = compute_puzzle_size(squared_puzzle);
        ok(actual_size == test_case.size, "%s: size=%d, expecting %d", test_case.image, actual_size, test_case.size);

        if (fail_n) {
            cvNamedWindow("result", 1);

            // expected
            for (int i = 1; i < test_case.size; ++i) {
                int q = i * (squared_puzzle->width / test_case.size);
                CvPoint horizontal[2];
                horizontal[0].x = 0;
                horizontal[0].y = q;
                horizontal[1].x = squared_puzzle->width;
                horizontal[1].y = q;
                cvLine(squared_puzzle, horizontal[0], horizontal[1], CV_RGB(0,255,0), 4, 8, 0);

                CvPoint vertical[2];
                vertical[0].x = q;
                vertical[0].y = 0;
                vertical[1].x = q;
                vertical[1].y = squared_puzzle->height;
                cvLine(squared_puzzle, vertical[0], vertical[1], CV_RGB(0,255,0), 4, 8, 0);
            }

            // actual
            for (int i = 1; i < actual_size; ++i) {
                int q = i * (squared_puzzle->width / actual_size);
                CvPoint horizontal[2];
                horizontal[0].x = 0;
                horizontal[0].y = q;
                horizontal[1].x = squared_puzzle->width;
                horizontal[1].y = q;
                cvLine(squared_puzzle, horizontal[0], horizontal[1], CV_RGB(255,0,0), 4, 8, 0);

                CvPoint vertical[2];
                vertical[0].x = q;
                vertical[0].y = 0;
                vertical[1].x = q;
                vertical[1].y = squared_puzzle->height;
                cvLine(squared_puzzle, vertical[0], vertical[1], CV_RGB(255,0,0), 4, 8, 0);
            }

            showSmaller(squared_puzzle, "result");

            cvWaitKey(0);
            cvDestroyWindow("result");
            exit(fail_n);
        }
    }

    yaml_document_delete(&document);

    exit(fail_n);
}
