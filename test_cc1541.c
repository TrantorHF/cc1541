/*******************************************************************************
* Copyright (c) 20018-2019 Claus, Bjoern Esser
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*******************************************************************************/

#define _CRT_SECURE_NO_WARNINGS /* avoid security warnings for MSVC */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

enum {
    NO_ERROR = 0,
    ERROR_ALLOCATION,
    ERROR_RETURN_VALUE,
    ERROR_NO_OUTPUT
};

const unsigned int track_offset[] = { /* taken from http://unusedino.de/ec64/technical/formats/d64.html */
    0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400, 0x06900, 0x07E00, 0x09300,
    0x0A800, 0x0BD00, 0x0D200, 0x0E700, 0x0FC00, 0x11100, 0x12600, 0x13B00,
    0x15000, 0x16500, 0x17800, 0x18B00, 0x19E00, 0x1B100, 0x1C400, 0x1D700,
    0x1EA00, 0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400, 0x25600, 0x26700,
    0x27800, 0x28900, 0x29A00, 0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00
};

const unsigned int track_offset_b[] = {
    /* second side of D71 */
    0x2AB00 + 0x00000, 0x2AB00 + 0x01500, 0x2AB00 + 0x02A00, 0x2AB00 + 0x03F00, 0x2AB00 + 0x05400, 0x2AB00 + 0x06900, 0x2AB00 + 0x07E00, 0x2AB00 + 0x09300,
    0x2AB00 + 0x0A800, 0x2AB00 + 0x0BD00, 0x2AB00 + 0x0D200, 0x2AB00 + 0x0E700, 0x2AB00 + 0x0FC00, 0x2AB00 + 0x11100, 0x2AB00 + 0x12600, 0x2AB00 + 0x13B00,
    0x2AB00 + 0x15000, 0x2AB00 + 0x16500, 0x2AB00 + 0x17800, 0x2AB00 + 0x18B00, 0x2AB00 + 0x19E00, 0x2AB00 + 0x1B100, 0x2AB00 + 0x1C400, 0x2AB00 + 0x1D700,
    0x2AB00 + 0x1EA00, 0x2AB00 + 0x1FC00, 0x2AB00 + 0x20E00, 0x2AB00 + 0x22000, 0x2AB00 + 0x23200, 0x2AB00 + 0x24400, 0x2AB00 + 0x25600, 0x2AB00 + 0x26700,
    0x2AB00 + 0x27800, 0x2AB00 + 0x28900, 0x2AB00 + 0x29A00, 0x2AB00 + 0x2AB00, 0x2AB00 + 0x2BC00, 0x2AB00 + 0x2CD00, 0x2AB00 + 0x2DE00, 0x2AB00 + 0x2EF00
};

/* Runs the binary with the provided commandline and returns the content of the output image file in a buffer */
int
run_binary(const char* binary, const char* options, const char* image_name, char **image, size_t *size)
{
    struct stat st;
    char *command_line;

    if (*image != NULL) {
        free(*image);
        *image = NULL;
    }

    /* build command line */
    size_t command_line_len = strlen(binary) + strlen(options) + strlen(image_name) + 3 + 12; /* 3 additional for spaces and terminator */
    command_line = (char*)calloc(command_line_len, sizeof(char));
    if (command_line == NULL) {
        return ERROR_ALLOCATION;
    }
    strcat(command_line, binary);
    strcat(command_line, " ");
    strcat(command_line, options);
    strcat(command_line, " ");
    strcat(command_line, image_name);
#ifdef _WIN32
    strcat(command_line, " > nul      ");
#else
    strcat(command_line, " > /dev/null");
#endif

    if (system(command_line) != 0) {
        return ERROR_RETURN_VALUE;
    }

    if (stat(image_name, &st)) {
        return ERROR_NO_OUTPUT;
    }

    *size = st.st_size;
    *image = calloc(st.st_size, sizeof(unsigned char));

    FILE* f = fopen(image_name, "rb");
    if (f == NULL) {
        return ERROR_NO_OUTPUT;
    }
    if (fread(*image, *size, 1, f) != 1) {
        fprintf(stderr, "ERROR: Unexpected filesize when reading %s\n", image_name);
        return ERROR_NO_OUTPUT;
    }
    fclose(f);

    return NO_ERROR;
}

/* runs the binary with a given command line and image output file, reads the output into a buffer and deletes the file then */
int
run_binary_cleanup(const char* binary, const char* options, const char* image_name, char **image, size_t *size)
{
    int status = run_binary(binary, options, image_name, image, size);
    remove(image_name);
    return status;
}

/* creates a file with given name, size and filled with given value */
int
create_value_file(const char* name, int size, char value)
{
    FILE* f = fopen(name, "wb");
    if (f == NULL) {
        return ERROR_NO_OUTPUT;
    }
    for (int i = 0; i < size; i++) {
        fputc(value, f);
    }
    fclose(f);
    return NO_ERROR;
}

/* checks if a given block in the image is filled with a given value */
int
block_is_filled(char* image, int block, int value)
{
    for (int i = 0; i < 254; i++) {
        if (image[block * 256 + 2 + i] != value) {
            return 0;
        }
    }
    return 1;
}

int
main(int argc, char* argv[])
{
    struct stat st;
    const char* binary;
    char *image = NULL;
    size_t size;
    int test = 0;
    int passed = 0;
    char *description;
    int result = 0;

    enum {
        TEST_PASS = 0,
        TEST_FAIL = 1,
        TEST_UNRESOLVED = 2
    };
    const char *const result_str[] = {
        "PASS",
        "FAIL",
        "UNRESOLVED"
    };
    const int test_pad = 2; /* Decimal digits of the test counter */

    if (argc != 2) {
        printf("Test suite for cc1541\n");
        printf("Usage: test_cc1541 <path to cc1541 binary>\n");
        return(-1);
    }
    if (stat(argv[1], &st)) {
        printf("ERROR: Test binary %s does not exist.\n", argv[1]);
        return(-1);
    }

    binary = argv[1];

    description = "Size of empty D64 image should be 174848";
    ++test;
    if (run_binary_cleanup(binary, "", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (size == 174848) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);

    description = "Size of empty G64 image should be 269862";
    ++test;
    if (run_binary_cleanup(binary, "", "image.g64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (size == 269862) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);

    description = "Size of empty D71 image should be 2*174848";
    ++test;
    if (run_binary_cleanup(binary, "", "image.d71", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (size == 2 * 174848) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);

    description = "Size of empty Speed DOS D64 image should be 174848+5*17*256";
    ++test;
    if (run_binary_cleanup(binary, "-4", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (size == 174848 + 5 * 17 * 256) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);

    description = "Size of empty Dolphin DOS D64 image should be 174848+5*17*256";
    ++test;
    if (run_binary_cleanup(binary, "-5", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (size == 174848 + 5 * 17 * 256) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);

    description = "Writing file with one block should fill track 1 sector 3";
    ++test;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 3, 37) && image[3 * 256] == 0 && image[3 * 256 + 1] == (char)255) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Diskname should be found in track 18 sector 0 offset $90";
    ++test;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-n 0123456789ABCDEF -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (strncmp(&image[track_offset[17] + 0x90], "0123456789ABCDEF", 16) == 0) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Diskname should be truncated to 16 characters";
    ++test;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-n 0123456789ABCDEFGHI -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (strncmp(&image[track_offset[17] + 0x90], "0123456789ABCDEF", 16) == 0 && image[track_offset[17] + 0xa0] == (char)0xa0) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Disk ID should be found in track 18 sector 0 offset $a2";
    ++test;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-i 01234 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (strncmp(&image[track_offset[17] + 0xa2], "01234", 5) == 0) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Disk ID should be truncated to 5 characters";
    ++test;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-i 0123456789ABCDEFGHI -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (strncmp(&image[track_offset[17] + 0xa2], "01234", 5) == 0 && image[track_offset[17] + 0xa7] == (char)0xa0) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Setting minimum sector to 7 should fill track 1 sector 7";
    ++test;
    create_value_file("1.prg", 254 * 21, 1);
    if (run_binary_cleanup(binary, "-F 7 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 7, 1)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Setting minimum sector to 7 for second track should fill track 2 sector 7";
    ++test;
    create_value_file("1.prg", 254 * 21, 1);
    create_value_file("2.prg", 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -F 7 -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 21 + 7, 2)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "Minimum sector should fall back to 3 after track change";
    ++test;
    create_value_file("1.prg", 254 * 21, 1);
    create_value_file("2.prg", 254, 2);
    if (run_binary_cleanup(binary, "-F 7 -w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 21 + 3, 2)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "File with default sector interleave 10 should fill sector 3 and 13 on track 1";
    ++test;
    create_value_file("1.prg", 254 * 2, 37);
    if (run_binary_cleanup(binary, "-S 7 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 3, 37) && block_is_filled(image, 10, 37)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "File with sector interleave 9 should fill sector 3 and 12 on track 1";
    ++test;
    create_value_file("1.prg", 254 * 2, 37);
    if (run_binary_cleanup(binary, "-s 9 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 3, 37) && block_is_filled(image, 12, 37)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "File with sector interleave 20 should fill sector 3 and 1 on track 1";
    ++test;
    create_value_file("1.prg", 254 * 2, 37);
    if (run_binary_cleanup(binary, "-s 20 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 3, 37) && block_is_filled(image, 1, 37)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "File with sector interleave -20 should fill sector 3 and 2 on track 1";
    ++test;
    create_value_file("1.prg", 254 * 2, 37);
    if (run_binary_cleanup(binary, "-s -20 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 3, 37) && block_is_filled(image, 2, 37)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Sector interleave should go back to default for next file";
    ++test;
    create_value_file("1.prg", 254 * 2, 1);
    create_value_file("2.prg", 254 * 2, 2);
    if (run_binary_cleanup(binary, "-S 3 -s 2 -w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 7, 2) && block_is_filled(image, 10, 2)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "Filename should be found at track 18 sector 1 offset 5";
    ++test;
    create_value_file("1.prg", 254 * 2, 1);
    if (run_binary_cleanup(binary, "-f 0123456789ABCDEF -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (strncmp(&image[track_offset[17] + 256 + 5], "0123456789ABCDEF", 16) == 0) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Filename hexvalue should be interpreted correctly";
    ++test;
    create_value_file("1.prg", 254 * 2, 1);
    if (run_binary_cleanup(binary, "-f 0123456789ABCDE#ef -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (image[track_offset[17] + 256 + 5 + 15] == (char)0xef) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Second file should start on track 2 sector 13 for -e";
    ++test;
    create_value_file("1.prg", 254, 1);
    create_value_file("2.prg", 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -e -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 3 + 21 + 10, 2)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "Second file should start on track 2 sector 3 for -e -b 3";
    ++test;
    create_value_file("1.prg", 254, 1);
    create_value_file("2.prg", 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -e -b 3 -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 3 + 21, 2)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "Second file should start on track 1 when it fits for -E";
    ++test;
    create_value_file("1.prg", 20 * 254, 1);
    create_value_file("2.prg", 1 * 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -E -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 19, 2)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "Second file should not start on track 1 when it does not fit for -E";
    ++test;
    create_value_file("1.prg", 20 * 254, 1);
    create_value_file("2.prg", 2 * 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -E -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 19, 0)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "File should start on track 13 for -r";
    ++test;
    create_value_file("1.prg", 254, 1);
    if (run_binary_cleanup(binary, "-r 13 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, track_offset[12] / 256 + 3, 1)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "File should start on sector 14 for -b";
    ++test;
    create_value_file("1.prg", 254, 1);
    create_value_file("2.prg", 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -b 14 -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, 14, 2)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "File should be distributed to both sides for -c";
    ++test;
    create_value_file("1.prg", 22 * 254, 1);
    if (run_binary_cleanup(binary, "-c -w 1.prg", "image.d71", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, track_offset_b[0] / 256 + 3, 1)) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "File should cover 1 sector on track 19 for -x not set";
    ++test;
    create_value_file("1.prg", 356 * 254, 1); /* leaves only one sector free before track 18 */
    create_value_file("2.prg", 2 * 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, track_offset[18] / 256 + 13, 0)) { /* check only second sector */
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "File should cover 2 sectors on track 19 for -x";
    ++test;
    create_value_file("1.prg", 356 * 254, 1); /* leaves only one sector free before track 18 */
    create_value_file("2.prg", 2 * 254, 2);
    if (run_binary_cleanup(binary, "-x -w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, track_offset[18] / 256 + 13, 2)) { /* check only second sector */
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "File should be placed on track 18 for -t";
    ++test;
    create_value_file("1.prg", 357 * 254, 1); /* fills all tracks up to 18 */
    create_value_file("2.prg", 2 * 254, 2);
    if (run_binary_cleanup(binary, "-t -w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, track_offset[17] / 256 + 13, 2)) { /* check only second sector */
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "File with 3 sectors should be placed on track 18 for -u";
    ++test;
    create_value_file("1.prg", 357 * 254, 1); /* fills all tracks up to 18 */
    create_value_file("2.prg", 3 * 254, 2);
    if (run_binary_cleanup(binary, "-t -u 3 -w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (block_is_filled(image, track_offset[17] / 256 + 3 /* (3+10+10)%19-1 */, 2)) { /* check only third sector */
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "Track 23 sector 0 and 1 should be identical to track 18 for -d";
    ++test;
    create_value_file("1.prg", 3 * 254, 1);
    create_value_file("2.prg", 5 * 254, 2);
    if (run_binary_cleanup(binary, "-d 23 -w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (memcmp(&image[track_offset[17] + 1], &image[track_offset[22] + 1], 1) == 0 /* shadow BAM is not valid, only need the sector link */
               && memcmp(&image[track_offset[17] + 1] + 256, &image[track_offset[22] + 1] + 256, 255) == 0) {
        /* leave out next dir block track */
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");
    remove("2.prg");

    description = "File should have DIR block size 0 for -B";
    ++test;
    create_value_file("1.prg", 3 * 254, 1);
    if (run_binary_cleanup(binary, "-B 0 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (image[track_offset[17] + 256 + 30] == 0 && image[track_offset[17] + 256 + 31] == 0) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "File should have DIR block size 65535 for -B";
    ++test;
    create_value_file("1.prg", 3 * 254, 1);
    if (run_binary_cleanup(binary, "-B 65535 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (image[track_offset[17] + 256 + 30] == (char)255 && image[track_offset[17] + 256 + 31] == (char)255) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Loop file should have actual DIR block size per default";
    ++test;
    create_value_file("1.prg", 258 * 254, 1);
    if (run_binary_cleanup(binary, "-w 1.prg -f LOOP.PRG -l 1.PRG", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (image[track_offset[17] + 256 + 32 + 30] == 2 && image[track_offset[17] + 256 + 32 + 31] == 1) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Loop file should have DIR block size 258 for -B";
    ++test;
    create_value_file("1.prg", 39 * 254, 1);
    if (run_binary_cleanup(binary, "-w 1.prg -f LOOP.PRG -B 258 -l 1.PRG", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (image[track_offset[17] + 256 + 32 + 30] == 2 && image[track_offset[17] + 256 + 32 + 31] == 1) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "File should have DIR block size 258 for -B, but actual block size in shadow dir for -d";
    ++test;
    create_value_file("1.prg", 3 * 254, 1);
    if (run_binary_cleanup(binary, "-B 258 -d 23 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (image[track_offset[17] + 256 + 30] == 258%256 && image[track_offset[17] + 256 + 31] == 258/256 && image[track_offset[22] + 256 + 30] == 3 && image[track_offset[22] + 256 + 31] == 0) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    description = "Loop file should have actual DIR block size for -L";
    ++test;
    create_value_file("1.prg", 258 * 254, 1);
    if (run_binary_cleanup(binary, "-w 1.prg -f LOOP.PRG -L 1", "image.d64", &image, &size) != NO_ERROR) {
        result = TEST_UNRESOLVED;
    } else if (image[track_offset[17] + 256 + 32 + 30] == 2 && image[track_offset[17] + 256 + 32 + 31] == 1) {
        result = TEST_PASS;
        ++passed;
    } else {
        result = TEST_FAIL;
    }
    printf("%0*d:  %s:  %s\n", test_pad, test, result_str[result], description);
    remove("1.prg");

    /* clean up */
    if (image != NULL) {
        free(image);
    }

    /* print summary */
    printf("\nPassed %d of %d tests.\n", passed, test);
    if (passed == test) {
        return 0;
    }
    return 1;
}
