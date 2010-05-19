#include <stdarg.h>

#include "cv.h"
#include "highgui.h"
#include "kenken.h"
#include "yaml.h"

#define LOCATION_FUZZ 22

typedef struct test_case_s {
    char           *image;
    unsigned short  puzzle_location_fail;
    CvPoint         puzzle_location[4];
    unsigned short  size_fail;
    unsigned short  size;
    unsigned short  cages_fail;
    char           *cages;
} test_case_t;

static unsigned int test_n = 1;
static unsigned int fail_n = 0;
static unsigned short ok (unsigned short condition, char *description, ...) {
    va_list args;
    char description_buffer[1000];

    va_start(args, description);
    vsnprintf(description_buffer, 1000, description, args);
    va_end(args);

    printf("%sok %d - %s\n", condition ? "" : "not ", test_n++, description_buffer);

    fail_n += (! condition);

    return condition;
}

static void show_with_cages(IplImage *in, int actual_size, char *cages, char *window_name) {
    IplImage *squared_puzzle = cvCreateImage(cvGetSize(in), 8, in->nChannels);
    cvCopy(in, squared_puzzle, NULL);
    IplImage *img = cvCreateImage(cvGetSize(squared_puzzle), 8, 1);
    cvCvtColor(squared_puzzle, img, CV_BGR2GRAY);

    int width_interval  = img->width / actual_size;
    int height_interval = img->height / actual_size;
    CvPoint top_left;
    CvPoint bottom_right;
    for (int x = 0; x < actual_size; ++x) {
        top_left.x     = x * width_interval;
        bottom_right.x = top_left.x + width_interval - 1;
        for (int y = 0; y < actual_size; ++y) {
            top_left.y     = y * height_interval;
            bottom_right.y = top_left.y + height_interval - 1;

            char c = cages[(y * actual_size) + x];

            int red   = ((c % 3) == 0) ? (((c - 65) * 7) + 10) : (((c % 7) == 2) ? 50 : 0);
            int green = ((c % 3) == 1) ? (((c - 65) * 7) + 10) : (((c % 7) == 3) ? 50 : 0);
            int blue  = ((c % 3) == 2) ? (((c - 65) * 7) + 10) : (((c % 7) == 4) ? 50 : 0);

            for (int px = top_left.x; px < bottom_right.x; ++px) {
                for (int py = top_left.y; py < bottom_right.y; ++py) {
                    CvScalar s = cvGet2D(img, py, px);
                    if (s.val[0] > 20) {
                        cvSet2D(squared_puzzle, py, px, CV_RGB(red, green, blue));
                    }
                }
            }
        }
    }

    showSmaller(squared_puzzle, window_name);
}

static unsigned short is_fail_node(yaml_node_t *node) {
    if (node->type == YAML_SCALAR_NODE) {
        if (strncmp((char *)node->data.scalar.value, "fail", 4) == 0) {
            return 1;
        }
    }
    return 0;
}

char *DEFAULT_CAGES = "";
int main (int argc, char** argv) {
    unsigned short show_annotations = 1;

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
        test_case.puzzle_location_fail = 0;
        test_case.puzzle_location[0] = cvPoint(0, 0);
        test_case.puzzle_location[1] = cvPoint(0, 0);
        test_case.puzzle_location[2] = cvPoint(0, 0);
        test_case.puzzle_location[3] = cvPoint(0, 0);
        test_case.size_fail = 0;
        test_case.size = 0;
        test_case.cages_fail = 0;
        test_case.cages = NULL;

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
            if (strcmp((const char *)key->data.scalar.value, "cages") == 0) {
                if (! (test_case.cages_fail = is_fail_node(value))) {
                    test_case.cages = (char *)value->data.scalar.value;
                }
            }
            if (strcmp((const char *)key->data.scalar.value, "size") == 0) {
                if (! (test_case.size_fail = is_fail_node(value))) {
                    test_case.size  = atoi((char *)value->data.scalar.value);
                }
            }
            if (strcmp((const char *)key->data.scalar.value, "puzzle_location") == 0) {
                if (! (test_case.puzzle_location_fail = is_fail_node(value))) {
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
            }
        }

        if (test_case.image == NULL) {
            printf("incomplete test case (missing 'image')\n");
            exit(255);
        }
        if (test_case.cages == NULL) {
            test_case.cages = DEFAULT_CAGES;
        }

        IplImage *color_image = cvLoadImage(test_case.image, 1);

        if (test_case.puzzle_location_fail) {
            continue;
        }

        IplImage *locate_puzzle_annotated;
        const CvPoint2D32f *actual_location = locate_puzzle(color_image, &locate_puzzle_annotated);

        if (ok(actual_location != NULL, "%s: puzzle found", test_case.image)) {
            for (int i = 0; i < 4; ++i) {
                ok(abs(actual_location[i].x - test_case.puzzle_location[i].x) < LOCATION_FUZZ, "%s: point %d: x=%.0f, expecting %d", test_case.image, i, actual_location[i].x, test_case.puzzle_location[i].x);
                ok(abs(actual_location[i].y - test_case.puzzle_location[i].y) < LOCATION_FUZZ, "%s: point %d: y=%.0f, expecting %d", test_case.image, i, actual_location[i].y, test_case.puzzle_location[i].y);
            }
        }

        if (show_annotations) {
            char *window_name = malloc(strlen("locate_puzzle ") + strlen(test_case.image) + 1);
            sprintf(window_name, "locate_puzzle %s", test_case.image);
            cvNamedWindow(window_name, 1);
            showSmaller(locate_puzzle_annotated, window_name);
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

        if (test_case.size_fail) {
            continue;
        }

        IplImage *squared_puzzle = square_puzzle(color_image, actual_location);

        puzzle_size actual_size = compute_puzzle_size(squared_puzzle);
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

        if (test_case.cages_fail) {
            continue;
        }

        char *actual_cages = compute_puzzle_cages(squared_puzzle, actual_size);
        ok(strcmp(actual_cages, test_case.cages) == 0, "%s: cages=%s, expecting %s", test_case.image, actual_cages, test_case.cages);

        if (fail_n) {
            cvNamedWindow("expected", 1);
            cvNamedWindow("actual", 1);

            show_with_cages(squared_puzzle, actual_size, test_case.cages, "expected");
            show_with_cages(squared_puzzle, actual_size, actual_cages, "actual");

            cvWaitKey(0);
            cvDestroyWindow("actual");
            cvDestroyWindow("expected");
            exit(fail_n);
        }
    }

    yaml_document_delete(&document);

    if (show_annotations) {
        cvWaitKey(0);
    }

    exit(fail_n);
}
