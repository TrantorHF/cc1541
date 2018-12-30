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

/* Runs the binary with the provided commandline and returns the content of the output image file in a buffer */
int 
run_binary(const char* binary, const char* options, const char* image_name, unsigned char **image, unsigned int *size) {
    struct stat st;
    char *command_line;

    if (*image != NULL) {
        free(*image);
        *image = NULL;
    }

    /* build command line */
    int command_line_len = strlen(binary) + strlen(options) + strlen(image_name) + 3 + 12; /* 3 additional for spaces and terminator */
    command_line = (char*)calloc(command_line_len, sizeof(char));
    if (command_line == NULL) {
        return ERROR_ALLOCATION;
    }
    strcat(command_line, binary);
    strcat(command_line, " ");
    strcat(command_line, options);
    strcat(command_line, " ");
    strcat(command_line, image_name);
#ifdef WIN32    
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
    fread(*image, *size, 1, f);
    fclose(f);

    return NO_ERROR;
}

/* runs the binary with a given command line and image output file, reads the output into a buffer and deletes the file then */
int 
run_binary_cleanup(const char* binary, const char* options, const char* image_name, unsigned char **image, size_t *size) {
    int status = run_binary(binary, options, image_name, image, size);
    remove(image_name);
    return status;
}

/* creates a file with given name, size and filled with given value */
int
create_value_file(const char* name, int size, char value) {
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
block_is_filled(unsigned char* image, int block, int value) {
    for (int i = 0; i < 254; i++) {
        if (image[block * 256 + 2 + i] != value) {
            return 0;
        }
    }
    return 1;
}

int
main(int argc, char* argv[]) {
    struct stat st;
    const char* binary;
    unsigned char *image = NULL;
    int size;
    int test = 0;
    int passed = 0;
    char *description;

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
    test++;
    if (run_binary_cleanup(binary, "", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (size == 174848) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    
    description = "Size of empty D71 image should be 2*174848";
    test++;
    if (run_binary_cleanup(binary, "", "image.d71", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (size == 2*174848) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }

    description = "Size of empty Speed DOS D64 image should be 174848+5*17*256";
    test++;
    if (run_binary_cleanup(binary, "-4", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (size == 174848 + 5 * 17 * 256) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    
    description = "Size of empty Dolphin DOS D64 image should be 174848+5*17*256";
    test++;
    if (run_binary_cleanup(binary, "-5", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (size == 174848 + 5 * 17 * 256) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }

    description = "Writing file with one block should fill track 1 sector 3";
    test++;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (block_is_filled(image, 3, 37) && image[3*256] == 0 && image[3 * 256+1] == 255) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");

    description = "Diskname should be found in track 18 sector 0 offset $90";
    test++;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-n 0123456789ABCDEF -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (strncmp(&image[track_offset[17]+0x90], "0123456789ABCDEF", 16) == 0) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");

    description = "Diskname should be truncated to 16 characters";
    test++;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-n 0123456789ABCDEFGHI -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (strncmp(&image[track_offset[17] + 0x90], "0123456789ABCDEF", 16) == 0 && image[track_offset[17] + 0xa0] == 0xa0) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");

    description = "Disk ID should be found in track 18 sector 0 offset $a2";
    test++;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-i 01234 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (strncmp(&image[track_offset[17] + 0xa2], "01234", 5) == 0) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");

    description = "Disk ID should be truncated to 5 characters";
    test++;
    create_value_file("1.prg", 254, 37);
    if (run_binary_cleanup(binary, "-i 0123456789ABCDEFGHI -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (strncmp(&image[track_offset[17] + 0xa2], "01234", 5) == 0 && image[track_offset[17] + 0xa7] == 0xa0) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");

    description = "Setting minimum sector to 7 should fill track 1 sector 7";
    test++;
    create_value_file("1.prg", 254*21, 1);
    if (run_binary_cleanup(binary, "-F 7 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (block_is_filled(image, 7, 1)) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");

    description = "Setting minimum sector to 7 for second track should fill track 2 sector 7";
    test++;
    create_value_file("1.prg", 254 * 21, 1);
    create_value_file("2.prg", 254, 2);
    if (run_binary_cleanup(binary, "-w 1.prg -F 7 -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (block_is_filled(image, 21 + 7, 2)) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");
    remove("2.prg");

    description = "Minimum sector should fall back to 3 after track change";
    test++;
    create_value_file("1.prg", 254 * 21, 1);
    create_value_file("2.prg", 254, 2);
    if (run_binary_cleanup(binary, "-F 7 -w 1.prg -w 2.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (block_is_filled(image, 21 + 3, 2)) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");
    remove("2.prg");

    description = "File with default sector interleave 7 should fill sector 3 and 10 on track 1";
    test++;
    create_value_file("1.prg", 254*2, 37);
    if (run_binary_cleanup(binary, "-S 7 -w 1.prg", "image.d64", &image, &size) != NO_ERROR) {
        printf("UNRESOLVED: %s\n", description);
    } else if (block_is_filled(image, 3, 37) && block_is_filled(image, 10, 37)) {
        passed++;
    } else {
        printf("FAIL: %s\n", description);
    }
    remove("1.prg");

    /* clean up */
    if (image != NULL) {
        free(image);
    }

    /* print summary */
    printf("\nPassed %d of %d tests\n", passed, test);
    if (passed == test) {
        return 0;
    }
    return 1;
}
