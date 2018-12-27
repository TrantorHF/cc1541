/*******************************************************************************
* Copyright (c) 2008-2018 JackAsser, Krill, Claus
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

#define DIRTRACK               18
#define DIRENTRIESPERBLOCK     8
#define DIRENTRYSIZE           32
#define BLOCKSIZE              256
#define BLOCKOVERHEAD          2
#define TRACKLINKOFFSET        0
#define SECTORLINKOFFSET       1
#define FILETYPEOFFSET         2
#define FILETYPE_PRG           0x82
#define FILETRACKOFFSET        3
#define FILESECTOROFFSET       4
#define FILENAMEOFFSET         5
#define FILENAMEMAXSIZE        16
#define FILENAMEEMPTYCHAR      (' ' | 0x80)
#define FILEBLOCKSLOOFFSET     30
#define FILEBLOCKSHIOFFSET     31
#define D64NUMBLOCKS           (664 + 19)
#define D64SIZE                (D64NUMBLOCKS * BLOCKSIZE)
#define D64SIZE_EXTENDED       (D64SIZE + 5 * 17 * BLOCKSIZE)
#define D71SIZE                (D64SIZE * 2)
#define D64NUMTRACKS           35
#define D64NUMTRACKS_EXTENDED  (D64NUMTRACKS + 5)
#define D71NUMTRACKS           (D64NUMTRACKS * 2)
#define BAM_OFFSET_SPEED_DOS   0xac
#define BAM_OFFSET_DOLPHIN_DOS 0xc0

typedef struct
{
    char* localname;
    char filename[FILENAMEMAXSIZE];
    int direntryindex;
    int direntrysector;
    int direntryoffset;
    int sectorInterleave;
    int first_sector_new_track;
    int track;
    int sector;
    int nrSectors;
    int mode;
} imagefile;

enum mode
{
    MODE_BEGINNING_SECTOR_MASK   = 0x003f, /* 6 bits */
    MODE_MIN_TRACK_MASK          = 0x0fc0, /* 6 bits */
    MODE_MIN_TRACK_SHIFT         = 6,
    MODE_SAVETOEMPTYTRACKS       = 0x1000,
    MODE_FITONSINGLETRACK        = 0x2000,
    MODE_SAVECLUSTEROPTIMIZED    = 0x4000,
    MODE_LOOPFILE                = 0x8000
};

typedef enum
{
    IMAGE_D64,
    IMAGE_D64_EXTENDED_SPEED_DOS,
    IMAGE_D64_EXTENDED_DOLPHIN_DOS,
    IMAGE_D71
} image_type;

void
usage()
{
    printf("Usage: cc1541 -niSsfeErbcwlxtu45q image.[d64|d71]\n\n");
    printf("-n diskname   Disk name, default='DEFAULT'\n");
    printf("-i id         Disk ID, default='LODIS'\n");
    printf("-F            Next file first sector on a new track, <0 assumes aligned tracks (default=3)\n");
    printf("              after each file the value falls back to the default value 3\n");
    printf("-S value      Default sector interleave, default=10\n");
    printf("              if negative, do not consider large tail gap (unlike standard)\n");
    printf("-s value      Next file sector interleave, after each file\n");
    printf("              if negative, do not consider large tail gap (unlike standard)\n");
    printf("              the interleave value falls back to the default value set by -S\n");
    printf("-f filename   Use filename as name when writing next file, use prefix # to include\n");
    printf("              arbitrary PETSCII characters (e.g. -f \"AUTOSTART#a0,8,1\")\n");
    printf("-e            Start next file on an empty track\n");
    printf("-E            Try to fit file on a single track\n");
    printf("-r track      Restrict next file blocks to the specified track or higher\n");
    printf("-b sector     Set next file beginning sector to the specified value\n");
    printf("-c            Save next file cluster-optimized (d71 only)\n");
    printf("-w localname  Write local file to disk, if filename is not set then the\n");
    printf("              local name is used. After file written filename is unset\n");
    printf("-l filename   Write loop file to existing file to disk, set filename with -f\n");
    printf("-x            Don't split files over dirtrack hole (default split files)\n");
    printf("-t            Use dirtrack to also store files (makes -x useless) (default no)\n");
    printf("-d track      Maintain a shadow directory (copy of the actual directory)\n");
    printf("-u numblocks  When using -t, amount of dir blocks to leave free (default=2)\n");
    printf("-4            Use tracks 35-40 with SPEED DOS BAM formatting\n");
    printf("-5            Use tracks 35-40 with DOLPHIN DOS BAM formatting\n");
    printf("-q            Be quiet\n");
    printf("\n");

    exit(-1);
}

static const int
sectors_per_track[] = {
    /*  1-17 */ 21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    /* 18-24 */ 19,19,19,19,19,19,19,
    /* 25-30 */ 18,18,18,18,18,18,
    /* 31-35 */ 17,17,17,17,17,
                21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
                19,19,19,19,19,19,19,
                18,18,18,18,18,18,
                17,17,17,17,17
};

static const int
sectors_per_track_extended[] = {
    /*  1-17 */ 21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    /* 18-24 */ 19,19,19,19,19,19,19,
    /* 25-30 */ 18,18,18,18,18,18,
    /* 31-35 */ 17,17,17,17,17,
    /* 36-40 */ 17,17,17,17,17,
                21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
                19,19,19,19,19,19,19,
                18,18,18,18,18,18,
                17,17,17,17,17,
                17,17,17,17,17
};

static int quiet = 0;

static int nrFiles = 0;

unsigned int
image_size(image_type type)
{
    switch (type) {
        case IMAGE_D64:
            return D64SIZE;

        case IMAGE_D64_EXTENDED_SPEED_DOS:
            /* fall through */

        case IMAGE_D64_EXTENDED_DOLPHIN_DOS:
            return D64SIZE_EXTENDED;

        case IMAGE_D71:
            return D71SIZE;

        default:
            return 0;
    }
}

unsigned int
image_num_tracks(image_type type)
{
    switch (type) {
        case IMAGE_D64:
            return D64NUMTRACKS;

        case IMAGE_D64_EXTENDED_SPEED_DOS:
            /* fall through */
        case IMAGE_D64_EXTENDED_DOLPHIN_DOS:
            return D64NUMTRACKS_EXTENDED;

        case IMAGE_D71:
            return D71NUMTRACKS;

        default:
            return 0;
    }
}

static const int *
image_num_sectors_table(image_type type)
{
    return ((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) ? sectors_per_track_extended : sectors_per_track;
}


static int
dirstrcmp(char* str1, char* str2)
{
    for (int i = 0; i < FILENAMEMAXSIZE; i++) {
        if ((str1[i] == (char) FILENAMEEMPTYCHAR) || (str1[i] == '\0')) {
            return (str2[i] != (char) FILENAMEEMPTYCHAR) && (str2[i] != '\0');
        }
        if ((str2[i] == (char) FILENAMEEMPTYCHAR) || (str2[i] == '\0')) {
            return (str1[i] != (char) FILENAMEEMPTYCHAR) && (str1[i] != '\0');
        }
        if (str1[i] != str2[i]) {
            return 1;
        }
    }

    return 0;
}

static int
linear_sector(image_type type, int track, int sector)
{
    if ((track < 1) || (track > ((type == IMAGE_D64) ? D64NUMTRACKS : (type == IMAGE_D71 ? D71NUMTRACKS : D64NUMTRACKS_EXTENDED)))) {
        fprintf(stderr, "Illegal track %d\n", track);
        exit(-1);
    }

    const int* num_sectors_table = image_num_sectors_table(type);

    int num_sectors = num_sectors_table[track - 1];
    if ((sector < 0) || (sector >= num_sectors)) {
        fprintf(stderr, "Illegal sector %d for track %d (max. is %d)\n", sector, track, num_sectors - 1);
        exit(-1);
    }

    int linear_sector = 0;
    for (int i = 0; i < track - 1; i++) {
        linear_sector += num_sectors_table[i];
    }
    linear_sector += sector;

    return linear_sector;
}

static int
is_sector_free(image_type type, unsigned char* image, int track, int sector, int numdirblocks, int dir_sector_interleave)
{
    int bam;
    unsigned char* bitmap;

    if (sector < 0) {
        fprintf(stderr, "Illegal sector %d for track %d\n", sector, track);
        exit(-1);
    }

    if ((type == IMAGE_D71) && (track > D64NUMTRACKS)) {
        /* access second side bam */
        bam = linear_sector(type, DIRTRACK + D64NUMTRACKS, 0) * BLOCKSIZE;
        bitmap = image + bam + (track - D64NUMTRACKS - 1) * 3;
    } else {
        if (((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) && (track > D64NUMTRACKS)) {
            track -= D64NUMTRACKS;
            bam = linear_sector(type, DIRTRACK, 0) * BLOCKSIZE + ((type == IMAGE_D64_EXTENDED_SPEED_DOS) ? BAM_OFFSET_SPEED_DOS : BAM_OFFSET_DOLPHIN_DOS);
        } else {
            bam = linear_sector(type, DIRTRACK, 0) * BLOCKSIZE;
        }
        bitmap = image + bam + track * 4 + 1;
    }

    int byte = sector >> 3;
    int bit = sector & 7;

    int is_not_dir_block = 1;
    if ((track == DIRTRACK) && (numdirblocks > 0)) {
        const int* num_sectors_table = image_num_sectors_table(type);

        int dirsector;
        int s = 2;
        for (int i = 0; is_not_dir_block && (i < numdirblocks); i++) {
            switch (i) {
                case 0:
                    dirsector = 0;
                    break;

                case 1:
                    dirsector = 1;
                    break;

                default:
                    dirsector += dir_sector_interleave;
                    if (dirsector >= num_sectors_table[track - 1]) {
                        dirsector = s;
                        s++;
                    }
                    break;
            }
            is_not_dir_block = (sector != dirsector);
        }
    }

    return is_not_dir_block && ((bitmap[byte] & (1 << bit)) != 0);
}

static void
mark_sector(image_type type, unsigned char* image, int track, int sector, int free, int shadowdirtrack)
{
    if (free != is_sector_free(type, image, track, sector, 0, 0)) {
        int bam;
        unsigned char* bitmap;
        int shadowbam;
        unsigned char* shadowbitmap;

        if ((type == IMAGE_D71) && (track > D64NUMTRACKS)) {
            /* access second side bam */
            bam = linear_sector(type, DIRTRACK + D64NUMTRACKS, 0) * BLOCKSIZE;
            bitmap = image + bam + (track - D64NUMTRACKS - 1) * 3;

            /* update number of free sectors on track */
            if (free) {
                image[bam + 0xdd + track - D64NUMTRACKS - 1]++;
            } else {
                image[bam + 0xdd + track - D64NUMTRACKS - 1]--;
            }

            if (shadowdirtrack > 0) {
                shadowbam = linear_sector(type, shadowdirtrack + D64NUMTRACKS, 0) * BLOCKSIZE;
                shadowbitmap = image + bam + (track - D64NUMTRACKS - 1) * 3;

                /* update number of free sectors on track */
                if (free) {
                    image[shadowbam + 0xdd + track - D64NUMTRACKS - 1]++;
                } else {
                    image[shadowbam + 0xdd + track - D64NUMTRACKS - 1]--;
                }
            }
       } else {
            if (((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) && (track > D64NUMTRACKS)) {
                track -= D64NUMTRACKS;
                bam = linear_sector(type, DIRTRACK, 0) * BLOCKSIZE + ((type == IMAGE_D64_EXTENDED_SPEED_DOS) ? BAM_OFFSET_SPEED_DOS : BAM_OFFSET_DOLPHIN_DOS);

                if (shadowdirtrack > 0) {
                    shadowbam = linear_sector(type, shadowdirtrack, 0) * BLOCKSIZE + ((type == IMAGE_D64_EXTENDED_SPEED_DOS) ? BAM_OFFSET_SPEED_DOS : BAM_OFFSET_DOLPHIN_DOS);
                }
            } else {
                bam = linear_sector(type, DIRTRACK, 0) * BLOCKSIZE;

                if (shadowdirtrack > 0) {
                    shadowbam = linear_sector(type, shadowdirtrack, 0) * BLOCKSIZE;
                }
            }
            bitmap = image + bam + track * 4 + 1;
            if (shadowdirtrack > 0) {
                shadowbitmap = image + shadowbam + track * 4 + 1;
            }

            /* update number of free sectors on track */
            if (free) {
                image[bam + track * 4]++;
                if (shadowdirtrack > 0) {
                    image[shadowbam + track * 4]++;
                }
            } else {
                image[bam + track * 4]--;
                if (shadowdirtrack > 0) {
                    image[shadowbam + track * 4]--;
                }
            }
        }

        /* update bitmap */
        int byte = sector >> 3;
        int bit = sector & 7;

        if (free) {
            bitmap[byte] |= 1 << bit;
            if (shadowdirtrack > 0) {
                shadowbitmap[byte] |= 1 << bit;
            }
        } else {
            bitmap[byte] &= ~(1 << bit);
            if (shadowdirtrack > 0) {
                shadowbitmap[byte] &= ~(1 << bit);
            }
        }
    }
}

static char *
ascii2petscii(char* str)
{
    unsigned char* ascii = (unsigned char *) str;

    while (*ascii != '\0') {
        if ((*ascii >= 'a') && (*ascii <= 'z')) {
            *ascii += 'A' - 'a';
        }

        ascii++;
    }

    return str;
}

static unsigned int
hex2int(char hex) {
    if ((hex < '0' || hex > '9') && (hex < 'a' || hex > 'f')) {
        fprintf(stderr, "Invalid hex string in filename\n");
        exit(-1);
    }
    if (hex <= '9') {
        hex -= '0';
    } else {
        hex -= 'a' - 10;
    }
    return (unsigned int)hex;
}

static char*
asciihex2petscii(char* str) {
    unsigned char* ascii = (unsigned char *)str;
    int read = 0, write = 0;

    while (ascii[read] != '\0') {
        if (ascii[read] == '#') {
            unsigned int hi = hex2int(ascii[++read]);
            unsigned int lo = hex2int(ascii[++read]);
            ascii[write] = (unsigned char)(16 * hi + lo);
        } else {
            ascii[write] = ascii[read];
        }
        read++;
        write++;
    }
    ascii[write] = ascii[read]; /* copy final \0 */

    return str;
}

static void
update_directory(image_type type, unsigned char* image, char* header, char* id, int shadowdirtrack)
{
    unsigned int bam = linear_sector(type, DIRTRACK, 0) * BLOCKSIZE;

    image[bam + 0x03] = (type == IMAGE_D71) ? 0x80 : 0x00;

    /* Set header and ID */
    for (size_t i = 0; i < 16; i++) {
        if (i < strlen(header)) {
            image[bam + 0x90 + i] = header[i];
        } else {
            image[bam + 0x90 + i] = FILENAMEEMPTYCHAR;
        }
    }

    static const char DEFAULT_ID[] = "00 2A";

    for (size_t i = 0; i < 5; i++)    {
        if (i < strlen(id)) {
            image[bam + 0xa2 + i] = id[i];
        } else {
            image[bam + 0xa2 + i] = DEFAULT_ID[i];
        }
    }

    if (shadowdirtrack > 0) {
        unsigned int shadowbam = linear_sector(type, shadowdirtrack, 0) * BLOCKSIZE;
        memcpy(image + shadowbam, image + bam, BLOCKSIZE);

        image[shadowbam + 0x00] = shadowdirtrack;
    }
}

static void
initialize_directory(image_type type, unsigned char* image, char* header, char* id, int shadowdirtrack)
{
    unsigned int bam = linear_sector(type, DIRTRACK, 0) * BLOCKSIZE;

    /* Clear image */
    memset(image, 0, image_size(type));

    /* Write initial BAM */
    image[bam + 0x00] = DIRTRACK;
    image[bam + 0x01] = 1;
    image[bam + 0x02] = 0x41;
    image[bam + 0x03] = (type == IMAGE_D71) ? 0x80 : 0x00;

    /* Mark all sectors unused */
    const int* num_sectors_table = image_num_sectors_table(type);
    for (unsigned int t = 1; t <= image_num_tracks(type); t++) {
        for (int s = 0; s < num_sectors_table[t - 1]; s++) {
            mark_sector(type, image, t, s, 1, shadowdirtrack);
        }
    }

    image[bam + 0xa0] = FILENAMEEMPTYCHAR;
    image[bam + 0xa1] = FILENAMEEMPTYCHAR;

    image[bam + 0xa7] = FILENAMEEMPTYCHAR;
    image[bam + 0xa8] = FILENAMEEMPTYCHAR;
    image[bam + 0xa9] = FILENAMEEMPTYCHAR;
    image[bam + 0xaa] = FILENAMEEMPTYCHAR;

    /* Reserve space for BAM */
    mark_sector(type, image, DIRTRACK, 0, 0, shadowdirtrack);
    if (type == IMAGE_D71) {
        mark_sector(type, image, DIRTRACK + D64NUMTRACKS, 0, 0, shadowdirtrack);
    }

    /* first dir block */
    unsigned int dirblock = linear_sector(type, DIRTRACK, 1) * BLOCKSIZE;
    image[dirblock + SECTORLINKOFFSET] = 255;
    mark_sector(type, image, DIRTRACK, 1, 0, shadowdirtrack);

    if (shadowdirtrack > 0) {
        dirblock = linear_sector(type, shadowdirtrack, 1) * BLOCKSIZE;
        image[dirblock + SECTORLINKOFFSET] = 255;
        mark_sector(type, image, shadowdirtrack, 0, 0, shadowdirtrack);
        if (type == IMAGE_D71) {
            mark_sector(type, image, shadowdirtrack + D64NUMTRACKS, 0, 0, shadowdirtrack);
        }
        mark_sector(type, image, shadowdirtrack, 1, 0, shadowdirtrack);
    }

    update_directory(type, image, header, id, shadowdirtrack);
}

static void
wipe_file(image_type type, unsigned char* image, unsigned int track, unsigned int sector, int shadowdirtrack)
{
    if (sector >= 0x80) {
      return; /* loop file */
    }

    while (track != 0) {
        int block_offset = linear_sector(type, track, sector) * BLOCKSIZE;
        int next_track = image[block_offset + TRACKLINKOFFSET];
        int next_sector = image[block_offset + SECTORLINKOFFSET];
        memset(image + block_offset, 0, BLOCKSIZE);
        mark_sector(type, image, track, sector, 1, shadowdirtrack);
        track = next_track;
        sector = next_sector;
    }
}

static int
find_file(image_type type, unsigned char* image, char* filename, unsigned char *track, unsigned char *sector)
{
    int direntryindex = 0;

    int dirsector = 1;
    int dirblock;
    int entryOffset;
    do {
        dirblock = linear_sector(type, DIRTRACK, dirsector) * BLOCKSIZE;
        for (int j = 0; j < DIRENTRIESPERBLOCK; ++j) {
            entryOffset = j * DIRENTRYSIZE;
            int filetype = image[dirblock + entryOffset + FILETYPEOFFSET];
            switch (filetype) {
                case FILETYPE_PRG:
                    if (dirstrcmp((char *) image + dirblock + entryOffset + FILENAMEOFFSET, filename) == 0) {
                        track = &(image[dirblock + entryOffset + FILETRACKOFFSET]);
                        sector = &(image[dirblock + entryOffset + FILESECTOROFFSET]);

                        return direntryindex;
                    }
                    break;

                default:
                    break;
            }

            ++direntryindex;
        }

        /* file not found in current dir block, try next */
        if (image[dirblock + TRACKLINKOFFSET] == DIRTRACK) {
            dirsector = image[dirblock + SECTORLINKOFFSET];

            continue;
        }

        break;
    } while (1);

    return -1;
}

static void
create_dir_entries(image_type type, unsigned char* image, imagefile* files, int num_files, int dir_sector_interleave, unsigned int shadowdirtrack)
{
    /* this does not check for uniqueness of filenames */

    int num_overwritten_files = 0;

    for (int i = 0; i < num_files; i++) {
        /* find or create slot */
        imagefile *file = &(files[i]);

        fprintf(stdout, "\"%s\"\n", file->filename);

        int direntryindex = 0;

        int dirsector = 1;
        int dirblock;
        int shadowdirblock;
        int entryOffset;
        int found = 0;
        do {
            dirblock = linear_sector(type, DIRTRACK, dirsector) * BLOCKSIZE;
            if (shadowdirtrack > 0) {
                shadowdirblock = linear_sector(type, shadowdirtrack, dirsector) * BLOCKSIZE;
            }

            for (int j = 0; (!found) && (j < DIRENTRIESPERBLOCK); ++j, ++direntryindex) {
                entryOffset = j * DIRENTRYSIZE;
                /* this assumes the dir only holds PRG files */
                int filetype = image[dirblock + entryOffset + FILETYPEOFFSET];
                switch (filetype) {
                    case FILETYPE_PRG:
                        if (dirstrcmp((char *) image + dirblock + entryOffset + FILENAMEOFFSET, file->filename) == 0) {
                            wipe_file(type, image, image[dirblock + entryOffset + FILETRACKOFFSET], image[dirblock + entryOffset + FILESECTOROFFSET], shadowdirtrack);
                            num_overwritten_files++;
                            found = 1;
                        }
                        break;

                    default:
                        found = 1;
                        break;
                }

                if (found) {
                  break;
                }
            }

            if (!found) {
                if (image[dirblock + TRACKLINKOFFSET] == DIRTRACK) {
                    dirsector = image[dirblock + SECTORLINKOFFSET];
                } else {
                    /* allocate new dir block */
                    const int* num_sectors_table = image_num_sectors_table(type);
                    int next_sector;
                    for (next_sector = dirsector + dir_sector_interleave; next_sector < dirsector + num_sectors_table[DIRTRACK - 1]; next_sector++) {
                        int findSector = next_sector % num_sectors_table[DIRTRACK - 1];
                        if (is_sector_free(type, image, DIRTRACK, findSector, 0, 0)) {
                            found = 1;
                            next_sector = findSector;
                            break;
                        }
                    }
                    if (!found) {
                        fprintf(stderr, "Dir track full!\n");
                        exit(-1);
                    }

                    image[dirblock + TRACKLINKOFFSET] = DIRTRACK;
                    image[dirblock + SECTORLINKOFFSET] = next_sector;

                    mark_sector(type, image, DIRTRACK, next_sector, 0 /* not free */, shadowdirtrack);

                    /* initialize new dir block */
                    dirblock = linear_sector(type, DIRTRACK, next_sector) * BLOCKSIZE;

                    memset(image + dirblock, 0, BLOCKSIZE);
                    image[dirblock + TRACKLINKOFFSET] = 0;
                    image[dirblock + SECTORLINKOFFSET] = 255;

                    if (shadowdirtrack > 0) {
                        image[shadowdirblock + TRACKLINKOFFSET] = shadowdirtrack;
                        image[shadowdirblock + SECTORLINKOFFSET] = next_sector;

                        mark_sector(type, image, shadowdirtrack, next_sector, 0 /* not free */, shadowdirtrack);

                        /* initialize new dir block */
                        shadowdirblock = linear_sector(type, shadowdirtrack, next_sector) * BLOCKSIZE;

                        memset(image + shadowdirblock, 0, BLOCKSIZE);
                        image[shadowdirblock + TRACKLINKOFFSET] = 0;
                        image[shadowdirblock + SECTORLINKOFFSET] = 255;
                    }

                    dirsector = next_sector;
                    found = 0;
                }
            }
        } while (!found);

        if (shadowdirtrack > 0) {
            if (memcmp(image + dirblock + 1, image + shadowdirblock + 1, BLOCKSIZE - 1) != 0) {
                fprintf(stderr, "Dir vs shadow dir mismatch\n");
                exit(-1);
            }
        }

        /* set filetype */
        image[dirblock + entryOffset + FILETYPEOFFSET] = FILETYPE_PRG;

        /* set filename */
        asciihex2petscii(file->filename); /* evaluate hex numbers */
        for (unsigned int j = 0; j < FILENAMEMAXSIZE; j++) {
            if (j < strlen(file->filename)) {
                image[dirblock + entryOffset + FILENAMEOFFSET + j] = file->filename[j];
            } else {
                image[dirblock + entryOffset + FILENAMEOFFSET + j] = FILENAMEEMPTYCHAR;
            }
        }

        if (shadowdirtrack > 0) {
            /* set filetype */
            image[shadowdirblock + entryOffset + FILETYPEOFFSET] = FILETYPE_PRG;

            /* set filename */
            for (unsigned int j = 0; j < FILENAMEMAXSIZE; j++) {
                if (j < strlen(file->filename)) {
                    image[shadowdirblock + entryOffset + FILENAMEOFFSET + j] = file->filename[j];
                } else {
                    image[shadowdirblock + entryOffset + FILENAMEOFFSET + j] = FILENAMEEMPTYCHAR;
                }
            }
        }

        /* set directory entry reference */
        file->direntryindex = direntryindex;
        file->direntrysector = dirsector;
        file->direntryoffset = entryOffset;
    }

    if (!quiet && (num_overwritten_files > 0)) {
        fprintf(stdout, "%d files out of %d files are already existing and will be overwritten\n", num_overwritten_files, num_files);
    }
}

static void
print_file_allocation(image_type type, unsigned char* image, imagefile* files, int num_files)
{
    for (int i = 0; i < num_files; i++) {
        printf("%3d (0x%02x 0x%02x:0x%02x) \"%s\" => \"%s\" (SL:%d)", files[i].nrSectors, files[i].direntryindex, files[i].direntrysector, files[i].direntryoffset,
                                                                      files[i].localname, files[i].filename, files[i].sectorInterleave);

        int track = files[i].track;
        int sector = files[i].sector;
        int j = 0;
        while (track != 0) {
            if (j == 0) {
                printf("\n    ");
            }
            printf("%02d/%02d ", track, sector);
            int offset = linear_sector(type, track, sector) * BLOCKSIZE;
            track = image[offset + 0];
            sector = image[offset + 1];
            j++;
            if (j == 10) {
                j = 0;
            }
        }
        printf("\n");
    }
}

static void
print_bam(image_type type, unsigned char* image)
{
    const int* num_sectors_table = image_num_sectors_table(type);
    int sectorsFree = 0;
    int sectorsFreeOnDirTrack = 0;
    int sectorsOccupied = 0;
    int sectorsOccupiedOnDirTrack = 0;

    int max_track = ((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) ? D64NUMTRACKS_EXTENDED : D64NUMTRACKS;

    for (int t = 1; t <= max_track; t++) {

        printf("%2d: ", t);
        for (int s = 0; s < num_sectors_table[t - 1]; s++) {
            if (is_sector_free(type, image, t, s, 0, 0)) {
                printf("0");
                if (t != DIRTRACK) {
                    sectorsFree++;
                } else {
                    sectorsFreeOnDirTrack++;
                }
            } else {
                printf("1");
                if (t != DIRTRACK) {
                    sectorsOccupied++;
                } else {
                    sectorsOccupiedOnDirTrack++;
                }
            }
        }

        if (type == IMAGE_D71) {
            for (int i = num_sectors_table[t - 1]; i < 23; i++) {
                printf(" ");
            }

            printf("%2d: ", t + D64NUMTRACKS);
            for (int s = 0; s < num_sectors_table[t + D64NUMTRACKS - 1]; s++) {
                if (is_sector_free(type, image, t + D64NUMTRACKS, s, 0, 0)) {
                    printf("0");
                    if ((t + D64NUMTRACKS) != DIRTRACK) {
                        sectorsFree++;
                    } else {
                        /* track 53 is usually empty except the extra BAM block */
                        sectorsFreeOnDirTrack++;
                    }
                } else {
                    printf("1");
                    sectorsOccupied++;
                }
            }
        }

        printf("\n");
    }
    printf("%3d (%d) BLOCKS FREE (out of %d (%d) BLOCKS)\n", sectorsFree, sectorsFree + sectorsFreeOnDirTrack,
                                                             sectorsFree + sectorsOccupied, sectorsFree + sectorsFreeOnDirTrack + sectorsOccupied + sectorsOccupiedOnDirTrack);
}

static void
write_files(image_type type, unsigned char* image, imagefile* files, int num_files, int usedirtrack, int dirtracksplit, unsigned int shadowdirtrack, int numdirblocks, int dir_sector_interleave)
{
    unsigned char track = 1;
    unsigned char sector = 0;
    int bytes_to_write = 0;
    unsigned int lastTrack = track;
    int lastSector = sector;
    int lastOffset = linear_sector(type, lastTrack, lastSector) * BLOCKSIZE;

    for (int i = 0; i < num_files; i++) {
        imagefile *file = &(files[i]);

        if (file->mode & MODE_LOOPFILE) {
            /* find loopfile source files */
            int direntryindex = find_file(type, image, file->localname, &track, &sector);
            if (direntryindex >= 0) {
                file->track = track;
                file->sector = sector;

                /* update directory entry */
                int entryOffset = linear_sector(type, DIRTRACK, file->direntrysector) * BLOCKSIZE + file->direntryoffset;
                image[entryOffset + FILETRACKOFFSET] = file->track;
                image[entryOffset + FILESECTOROFFSET] = direntryindex - file->direntryindex;

                image[entryOffset + FILEBLOCKSLOOFFSET] = file->nrSectors & 255;
                image[entryOffset + FILEBLOCKSHIOFFSET] = file->nrSectors >> 8;

                if (shadowdirtrack > 0) {
                    entryOffset = linear_sector(type, shadowdirtrack, file->direntrysector) * BLOCKSIZE + file->direntryoffset;
                    image[entryOffset + FILETRACKOFFSET] = file->track;
                    image[entryOffset + FILESECTOROFFSET] = direntryindex - file->direntryindex;

                    image[entryOffset + FILEBLOCKSLOOFFSET] = file->nrSectors & 255;
                    image[entryOffset + FILEBLOCKSHIOFFSET] = file->nrSectors >> 8;
                }

                continue;
            } else {
                fprintf(stderr, "Loop source file '%s' (%d) not found\n", file->localname, i + 1);

                exit(-1);
            }
        }

        struct stat st;
        stat(files[i].localname, &st);

        int fileSize = st.st_size;

        unsigned char* filedata = (unsigned char*)calloc(fileSize, sizeof(unsigned char));
        if (filedata == NULL) {
            printf("Memory allocation error");
            exit(-1);
        }
        FILE* f = fopen(file->localname, "rb");
        if (f == NULL) {
            printf("could not open file %s for reading\n", file->localname);
            exit(-1);
        }
        fread(filedata, fileSize, 1, f);
        fclose(f);

        if ((file->mode & MODE_MIN_TRACK_MASK) > 0) {
            track = (file->mode & MODE_MIN_TRACK_MASK) >> MODE_MIN_TRACK_SHIFT;
            /* note that track may be smaller than lastTrack now */
            if (track > image_num_tracks(type)) {
                printf("invalid minimum track %d for file %s (%s) specified\n", track, file->localname, file->filename);

                exit(-1);
            }
            if ((!usedirtrack)
             && ((track == DIRTRACK) || (track == shadowdirtrack)
              || ((type == IMAGE_D71) && (track == (D64NUMTRACKS + DIRTRACK))))) { /* .d71 track 53 is usually empty except the extra BAM block */
              ++track; /* skip dir track */
            }
            if ((track - lastTrack) > 1) {
                /* previous file's last track and this file's beginning track have tracks in between */
                sector = file->first_sector_new_track;
            }
        }

        if ((file->mode & MODE_BEGINNING_SECTOR_MASK) > 0) {
            sector = (file->mode & MODE_BEGINNING_SECTOR_MASK) - 1;
        }

        if (((file->mode & MODE_SAVETOEMPTYTRACKS) != 0)
         || ((file->mode & MODE_FITONSINGLETRACK) != 0)) {

            /* find first empty track */
            int found = 0;
            while (!found) {
                const int* num_sectors_table = image_num_sectors_table(type);
                for (int s = 0; s < num_sectors_table[track - 1]; s++) {
                    if (is_sector_free(type, image, track, s, usedirtrack ? numdirblocks : 0, dir_sector_interleave)) {
                        if (s == num_sectors_table[track - 1] - 1) {
                            found = 1;
                            /* in first pass, use sector as left by previous file (or as set by -b) to reach first file block quickly */
                            if (sector >= num_sectors_table[track - 1]) {
                                if ((file->mode & MODE_BEGINNING_SECTOR_MASK) > 0) {
                                    printf("Invalid beginning sector %d on track %d for file %s (%s) specified\n", sector, track, file->localname, file->filename);

                                    exit(-1);
                                }

                                sector %= num_sectors_table[track - 1];
                            }
                        }
                    } else {
                        int prev_track = track;
                        if (file->mode & MODE_SAVECLUSTEROPTIMIZED) {
                            if (track > D64NUMTRACKS) {
                                int next_track = track - D64NUMTRACKS + 1; /* to next track on first side */
                                if (next_track < D64NUMTRACKS) {
                                    track = next_track;
                                } else {
                                  ++track; /* disk full */
                                }
                            } else {
                                track += D64NUMTRACKS; /* to same track on second side */
                            }
                        } else {
                            ++track;
                        }
                        if ((!usedirtrack)
                         && ((track == DIRTRACK) || (track == shadowdirtrack)
                          || ((type == IMAGE_D71) && (track == D64NUMTRACKS + DIRTRACK)))) { /* .d71 track 53 is usually empty except the extra BAM block */
                            ++track; /* skip dir track */
                        }
                        if (file->mode & MODE_FITONSINGLETRACK) {
                            int file_size = fileSize;
                            int first_sector = -1;
                            for (int s = 0; s < num_sectors_table[prev_track - 1]; s++) {
                                if (is_sector_free(type, image, prev_track, s, usedirtrack ? numdirblocks : 0, dir_sector_interleave)) {
                                    if (first_sector < 0) {
                                        first_sector = s;
                                    }
                                    file_size -= BLOCKSIZE + BLOCKOVERHEAD;
                                    if (file_size <= 0) {
                                        found = 1;
                                        track = prev_track;
                                        sector = first_sector;
                                        break;
                                    }
                                }
                            }
                        }

                        if (track > image_num_tracks(type)) {
                            fprintf(stderr, "Disk full, file %s (%s)\n", file->localname, file->filename);
                            exit(-1);
                        }
                        break;
                    }
                } /* for each sector on track */

                if ((track == (lastTrack + 2))
                 && (file->mode & MODE_BEGINNING_SECTOR_MASK) == 0) {
                    /* previous file's last track and this file's beginning track have tracks in between now */
                    sector = 0;
                }
            } /* while not found */
        }

        if ((file->mode & MODE_BEGINNING_SECTOR_MASK) > 0) {
            if (sector != ((file->mode & MODE_BEGINNING_SECTOR_MASK) - 1)) {
                fprintf(stderr, "Specified beginning sector of file %s (%s) not free on track %d\n", file->localname, file->filename, track);

                exit(-1);
            }
        }

        /* found start track, now save file */

        const int* num_sectors_table = image_num_sectors_table(type);

        int byteOffset = 0;
        int bytesLeft = fileSize;
        while (bytesLeft > 0) {
            /* Find free track & sector, starting from current T/S forward one revolution, then the next track etc... skip dirtrack (unless -t is active) */
            /* If the file didn't fit before dirtrack then restart on dirtrack + 1 and try again (unless -t is active). */
            /* If the file didn't fit before track 36/41/71 then the disk is full. */

            int blockfound = 0;
            int findSector = 0;

            while (!blockfound) {
                /* find spare block on the current track */
                for (int s = sector; s < sector + num_sectors_table[track - 1]; s++) {
                    findSector = s % num_sectors_table[track - 1];

                    if (is_sector_free(type, image, track, findSector, usedirtrack ? numdirblocks : 0, dir_sector_interleave)) {
                        blockfound = 1;
                        break;
                    }
                }

                if (!blockfound) {
                    /* find next track, use some magic to make up for track seek delay */
                    int prev_track = track;
                    int seek_delay = 1;
                    if (file->mode & MODE_SAVECLUSTEROPTIMIZED) {
                        if (track >= D64NUMTRACKS) {
                            track = track - D64NUMTRACKS + 1;
                        } else {
                            track += D64NUMTRACKS;
                            seek_delay = 0; /* switching to the other side, no head movement */
                        }
                    } else {
                        ++track;
                    }
                    if (file->first_sector_new_track < 0) {
                        sector += seek_delay - 1;
                    } else {
                        sector = file->first_sector_new_track;
                    }
                    if (sector < 0) {
                        sector += num_sectors_table[prev_track - 1];
                    }
                    sector %= num_sectors_table[prev_track - 1];

                    if ((!usedirtrack)
                     && ((track == DIRTRACK) || (track == shadowdirtrack)
                      || ((type == IMAGE_D71) && (track == D64NUMTRACKS + DIRTRACK)))) { /* .d71 track 53 is usually empty except the extra BAM block */
                        /* Delete old fragments and restart file */
                        if (!dirtracksplit) {
                            if (file->nrSectors > 0) {
                                int deltrack = file->track;
                                int delsector = file->sector;
                                while (deltrack != 0) {
                                    mark_sector(type, image, deltrack, delsector, 1, shadowdirtrack);
                                    int offset = linear_sector(type, deltrack, delsector) * BLOCKSIZE;
                                    deltrack = image[offset + 0];
                                    delsector = image[offset + 1];
                                    memset(image + offset, 0, BLOCKSIZE);
                                }
                            }

                            bytesLeft = fileSize;
                            byteOffset = 0;
                            file->nrSectors = 0;
                        }

                        if (track == shadowdirtrack) {
                          ++track;
                        } else {
                          track = DIRTRACK + 1;
                        }
                    }

                    if (track > image_num_tracks(type)) {
                        print_file_allocation(type, image, files, nrFiles);
                        print_bam(type, image);

                        fprintf(stderr, "Disk full, file %s (%s)\n", file->localname, file->filename);
                        free(filedata);

                        exit(-1);
                    }
                }
            } /* while not block found */

            sector = findSector;
            int offset = linear_sector(type, track, sector) * BLOCKSIZE;

            if (bytesLeft == fileSize) {
                file->track = track;
                file->sector = sector;
                lastTrack = track;
                lastSector = sector;
                lastOffset = offset;
            } else {
                image[lastOffset + 0] = track;
                image[lastOffset + 1] = sector;
            }

            /* write sector */
            bytes_to_write = min(BLOCKSIZE - BLOCKOVERHEAD, bytesLeft);
            memcpy(image + offset + 2, filedata + byteOffset, bytes_to_write);

            bytesLeft -= bytes_to_write;
            byteOffset += bytes_to_write;

            lastTrack = track;
            lastSector = sector;
            lastOffset = offset;

            mark_sector(type, image, track, sector, 0 /* not free */, shadowdirtrack);

            if (num_sectors_table[track - 1] <= abs(file->sectorInterleave)) {
                fprintf(stderr, "Invalid interleave %d on track %d (%d sectors), file %s (%s)\n", file->sectorInterleave, track, num_sectors_table[track - 1], file->localname, file->filename);

                exit(-1);
            }

            sector += abs(file->sectorInterleave);
            if (sector >= num_sectors_table[track - 1]) {
                sector -= num_sectors_table[track - 1];
                if ((file->sectorInterleave >= 0) && (sector > 0)) {
                    --sector; /* subtract one after wrap (supposedly due to large tail gap) */
                }
            }

            file->nrSectors++;
        } /* while bytes left */

        free(filedata);

        image[lastOffset + 0] = 0x00;
        image[lastOffset + 1] = bytes_to_write + 1;

        /* update directory entry */
        int entryOffset = linear_sector(type, DIRTRACK, file->direntrysector) * BLOCKSIZE + file->direntryoffset;
        image[entryOffset + FILETRACKOFFSET] = file->track;
        image[entryOffset + FILESECTOROFFSET] = file->sector;

        image[entryOffset + FILEBLOCKSLOOFFSET] = file->nrSectors & 255;
        image[entryOffset + FILEBLOCKSHIOFFSET] = file->nrSectors >> 8;

        if (shadowdirtrack > 0) {
            entryOffset = linear_sector(type, shadowdirtrack, file->direntrysector) * BLOCKSIZE + file->direntryoffset;
            image[entryOffset + FILETRACKOFFSET] = file->track;
            image[entryOffset + FILESECTOROFFSET] = file->sector;

            image[entryOffset + FILEBLOCKSLOOFFSET] = file->nrSectors & 255;
            image[entryOffset + FILEBLOCKSHIOFFSET] = file->nrSectors >> 8;
        }
    } /* for each file */
}

static size_t
write16(unsigned int value, FILE* f)
{
    char byte = value & 0xff;
    size_t bytes_written = fwrite(&byte, 1, 1, f);

    byte = (value >> 8) & 0xff;
    bytes_written += fwrite(&byte, 1, 1, f);

    return bytes_written;
}

static size_t
write32(unsigned int value, FILE* f)
{
    size_t bytes_written = write16(value, f);
    bytes_written += write16(value >> 16, f);

    return bytes_written;
}

static void
encode_4_bytes_gcr(char* in, char* out)
{
    static const uint8_t nibble_to_gcr[] = {
        0x0a, 0x0b, 0x12, 0x13,
        0x0e, 0x0f, 0x16, 0x17,
        0x09, 0x19, 0x1a, 0x1b,
        0x0d, 0x1d, 0x1e, 0x15
    };

    out[0] = (nibble_to_gcr[(in[0] >> 4) & 0xf] << 3) | (nibble_to_gcr[ in[0]       & 0xf] >> 2); /* 11111222 */
    out[1] = (nibble_to_gcr[ in[0]       & 0xf] << 6) | (nibble_to_gcr[(in[1] >> 4) & 0xf] << 1) | (nibble_to_gcr[ in[1]       & 0xf] >> 4); /* 22333334 */
    out[2] = (nibble_to_gcr[ in[1]       & 0xf] << 4) | (nibble_to_gcr[(in[2] >> 4) & 0xf] >> 1); /* 44445555 */
    out[3] = (nibble_to_gcr[(in[2] >> 4) & 0xf] << 7) | (nibble_to_gcr[ in[2]       & 0xf] << 2) | (nibble_to_gcr[(in[3] >> 4) & 0xf] >> 3); /* 56666677 */
    out[4] = (nibble_to_gcr[(in[3] >> 4) & 0xf] << 5) |  nibble_to_gcr[ in[3]       & 0xf]; /* 77788888 */
}

void
generate_uniformat_g64(unsigned char* image)
{
    printf("Generating UniFormAt G64 image\n");

    FILE* f = fopen("uniformat.g64", "wb");

    size_t filepos = 0;

    static const char signature[] = "GCR-1541";
    filepos += fwrite(signature, 1, sizeof signature - 1, f);

    const char version = 0;
    filepos += fwrite(&version, 1, 1, f);

    const char num_tracks = 35 * 2;
    filepos += fwrite(&num_tracks, 1, 1, f);

    const unsigned int track_size = 7692;
    filepos += write16(track_size, f);

    const unsigned int table_size = num_tracks * 4;
    const unsigned int tracks_offset = filepos + (table_size * 2);

    for (int track = 0; track < num_tracks; ++track) {
        unsigned int track_offset = 0;

        if ((track & 1) == 0) {
            track_offset = tracks_offset + ((track >> 1) * (2 + track_size));
        }

        filepos += write32(track_offset, f);
    }

    for (int track = 0; track < num_tracks; ++track) {
        unsigned int bit_rate = 0;

        if ((track & 1) == 0) {
            switch (sectors_per_track[track >> 1]) {
                case 21: bit_rate = 3; break;
                case 19: bit_rate = 2; break;
                case 18: bit_rate = 1; break;
                case 17: bit_rate = 0; break;
            }
        }

        filepos += write32(bit_rate, f);
    }

    const unsigned int block_size =
            5  /* sync */
        +  10  /* header */
        +   9  /* gap */
        +   5  /* sync */
        + 325; /* data */

    const unsigned char sync[5] = { 0xff, 0xff, 0xff, 0xff, 0xff };
    const char header_gap[9] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    const char id[3] = { '2', 'A', '\0' };
    const char block_gap[24] = {
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
    };

    for (int track = 0; track < (num_tracks >> 1); ++track) {
        unsigned int track_bytes = 0;
        unsigned int num_sectors = sectors_per_track[track];
        switch (num_sectors) {
            case 21: track_bytes = 7692; break;
            case 19: track_bytes = 7142; break;
            case 18: track_bytes = 6666; break;
            case 17: track_bytes = 6250; break;
        }

        filepos += write16(track_bytes, f);

        unsigned int data_bytes = num_sectors * block_size;
        unsigned int gap_size = (track_bytes - data_bytes) / num_sectors;
        float average_gap_remainder = (((float) (track_bytes - data_bytes)) / num_sectors) - gap_size;

        unsigned int gaps = 0;
        float remainder = 0.0f;
        for (unsigned int sector = 0; sector < num_sectors; ++sector) {
            unsigned int gap = gap_size;
            remainder += average_gap_remainder;
            if (remainder >= 0.5f) {
                remainder -= 1.0f;
                ++gap;
            }

            filepos += fwrite(sync, 1, sizeof sync, f);
            char header[8] = {
                0x08, /* header ID */
                (char) (sector ^ (track + 1) ^ id[1] ^ id[0]), /* checksum */
                (char) sector,
                (char) (track + 1),
                id[1],
                id[0],
                0x0f, 0x0f };

            char header_gcr[10];
            encode_4_bytes_gcr(header, header_gcr);
            encode_4_bytes_gcr(header + 4, header_gcr + 5);

            filepos += fwrite(header_gcr, 1, sizeof header_gcr, f);
            filepos += fwrite(header_gap, 1, sizeof header_gap, f);

            filepos += fwrite(sync, 1, sizeof sync, f);

            char group[5];

            char checksum = image[0] ^ image[1] ^ image[2];
            char data[4] = { 0x07, (char) image[0], (char) image[1], (char) image[2] };
            encode_4_bytes_gcr(data, group);
            filepos += fwrite(group, 1, sizeof group, f);
            for (int i = 0; i < 0x3f; ++i) {
                data[0] = image[(i * 4) + 3];
                data[1] = image[(i * 4) + 4];
                data[2] = image[(i * 4) + 5];
                data[3] = image[(i * 4) + 6];
                encode_4_bytes_gcr(data, group);
                filepos += fwrite(group, 1, sizeof group, f);
                checksum ^= (data[0] ^ data[1] ^ data[2] ^ data[3]);
            }
            data[0] = image[0xff];
            data[1] = data[0] ^ checksum;
            data[2] = 0;
            data[3] = 0;
            encode_4_bytes_gcr(data, group);
            filepos += fwrite(group, 1, sizeof group, f);

            filepos += fwrite(block_gap, 1, gap, f);

            image += 0x0100;

            gaps += gap;
        }

        for (int i = (track_size - track_bytes); i > 0; --i) {
            filepos += fwrite(sync, 1, 1, f);
        }
    }

    fclose(f);
}

int
main(int argc, char* argv[])
{
    imagefile files[144];
    memset(files, 0, sizeof files);

    image_type type = IMAGE_D64;
    char* imagepath = NULL;
    char* header = (char *) "DEFAULT";
    char* id     = (char *) "LODIS";
    int dirtracksplit = 1;
    int usedirtrack = 0;
    unsigned int shadowdirtrack = 0;

    int default_first_sector_new_track = 3;
    int first_sector_new_track = 3;
    int defaultSectorInterleave = 10;
    int sectorInterleave = 10;
    int dir_sector_interleave = 3;
    int numdirblocks = 2;
    char* filename = NULL;
    int set_header = 0;

    int i;

    if (argc == 1 || ((argc == 2) && (strcmp(argv[1], "-h") == 0))) {
        usage();
    }
    for (int j = 1; j < argc - 1; j++) {
        if (strcmp(argv[j], "-n") == 0) {
            if (argc < j + 3) {
                printf("Error parsing argument for -n\n");
                return -1;
            }
            header = argv[++j];
            set_header = 1;
        } else if (strcmp(argv[j], "-i") == 0) {
            if (argc < j + 3) {
                printf("Error parsing argument for -i\n");
                return -1;
            }
            id = argv[++j];
            set_header = 1;
        } else if (strcmp(argv[j], "-F") == 0) {
            if ((argc < j + 3) || !sscanf(argv[++j], "%d", &first_sector_new_track)) {
                printf("Error parsing argument for -F\n");
                return -1;
            }
        } else if (strcmp(argv[j], "-S") == 0) {
            if ((argc < j + 3) || !sscanf(argv[++j], "%d", &defaultSectorInterleave)) {
                printf("Error parsing argument for -S\n");
                return -1;
            }
        } else if (strcmp(argv[j], "-s") == 0) {
            if ((argc < j + 3) || !sscanf(argv[++j], "%d", &sectorInterleave)) {
                printf("Error parsing argument for -s\n");
                return -1;
            }
        } else if (strcmp(argv[j], "-f") == 0) {
            if (argc < j + 3) {
                printf("Error parsing argument for -f\n");
                return -1;
            }
            filename = argv[++j];
        } else if (strcmp(argv[j], "-e") == 0) {
            files[nrFiles].mode |= MODE_SAVETOEMPTYTRACKS;
        } else if (strcmp(argv[j], "-E") == 0) {
            files[nrFiles].mode |= MODE_FITONSINGLETRACK;
        } else if (strcmp(argv[j], "-r") == 0) {
            if ((argc < j + 3) || !sscanf(argv[++j], "%d", &i)) {
                printf("Error parsing argument for -r\n");
                return -1;
            }
            if ((i < 1) || (((i << MODE_MIN_TRACK_SHIFT) & MODE_MIN_TRACK_MASK) != (i << MODE_MIN_TRACK_SHIFT))) {
                printf("Invalid minimum track %d specified\n",  i);
                exit(-1);
            }
            files[nrFiles].mode = (files[nrFiles].mode & ~MODE_MIN_TRACK_MASK) | (i << MODE_MIN_TRACK_SHIFT);
        } else if (strcmp(argv[j], "-b") == 0) {
            if ((argc < j + 3) || !sscanf(argv[++j], "%d", &i)) {
                printf("Error parsing argument for -b\n");
                return -1;
            }
            if ((i < 0) || (i >= image_num_sectors_table(type)[0])) {
                printf("Invalid beginning sector %d specified\n", i);
                exit(-1);
            }
            files[nrFiles].mode = (files[nrFiles].mode & ~MODE_BEGINNING_SECTOR_MASK) | (i + 1);
        } else if (strcmp(argv[j], "-c") == 0) {
            files[nrFiles].mode |= MODE_SAVECLUSTEROPTIMIZED;
        } else if (strcmp(argv[j], "-w") == 0 || strcmp(argv[j], "-l") == 0) {
            if (argc < j + 3) {
                printf("Error parsing argument for -w or -l\n");
                return -1;
            }
            struct stat st;
            int loop_file = !strcmp(argv[j], "-l");
            if (loop_file) {
                files[nrFiles].mode |= MODE_LOOPFILE;
            }
            int file_exists = loop_file ? 1 /* will be checked later */
                : (stat(argv[j + 1], &st) == 0);
            if (file_exists) {
                files[nrFiles].localname = argv[j + 1];

                if (filename == NULL) {
                    strncpy(files[nrFiles].filename, files[nrFiles].localname, FILENAMEMAXSIZE);
                    ascii2petscii(files[nrFiles].filename);
                } else {
                    strncpy(files[nrFiles].filename, filename, FILENAMEMAXSIZE);
                }

                files[nrFiles].sectorInterleave = sectorInterleave;
                files[nrFiles].first_sector_new_track = first_sector_new_track;
                files[nrFiles].nrSectors = 0;
                nrFiles++;
            } else {
                fprintf(stderr, "File '%s' (%d) not found\n", argv[j + 1], nrFiles + 1);
                exit(-1);
            }

            filename = NULL;
            first_sector_new_track = default_first_sector_new_track;
            sectorInterleave = defaultSectorInterleave;
            j++;
        } else if (strcmp(argv[j], "-x") == 0) {
            dirtracksplit = 0;
        } else if (strcmp(argv[j], "-t") == 0) {
            usedirtrack = 1;
        } else if (strcmp(argv[j], "-d") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &shadowdirtrack)) {
                printf("Error parsing argument for -d\n");
                return -1;
            }
        } else if (strcmp(argv[j], "-u") == 0) {
            if ((argc < j + 3) || !sscanf(argv[++j], "%d", &numdirblocks)) {
                printf("Error parsing argument for -u\n");
                return -1;
            }
        } else if (strcmp(argv[j], "-4") == 0) {
            type = IMAGE_D64_EXTENDED_SPEED_DOS;
        } else if (strcmp(argv[j], "-5") == 0) {
            type = IMAGE_D64_EXTENDED_DOLPHIN_DOS;
        } else if (strcmp(argv[j], "-q") == 0) {
            quiet = 1;
        } else if (strcmp(argv[j], "-h") == 0) {
            usage();
        } else {
            printf("Error parsing commandline at \"%s\"\n", argv[j]);
            printf("Use -h for help.");
            return -1;
        }
    }
    imagepath = argv[argc-1];

    if ((strlen(imagepath) >= 4) && !strcmp(imagepath + strlen(imagepath) - 4, ".d71")) {
        if ((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) {
            printf("extended .d71 images are not supported\n");
            exit(-1);
        }
        type = IMAGE_D71;
    }

    /* open image */
    unsigned int imagesize = image_size(type);
    unsigned char* image = (unsigned char*)calloc(imagesize, sizeof(unsigned char));
    if (image == NULL) {
        printf("Memory allocation error");
        exit(-1);
    }
    FILE* f = fopen(imagepath, "rb");
    if (f == NULL) {
        initialize_directory(type, image, header, id, shadowdirtrack);
    } else {
        size_t read_size = fread(image, 1, imagesize, f);
        fclose(f);
        if (read_size != imagesize) {
            if (((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) && (read_size == D64SIZE)) {
                /* Clear extra tracks */
                memset(image + image_size(IMAGE_D64), 0, image_size(type) - image_size(IMAGE_D64));

                /* Mark all extra sectors unused */
                const int* num_sectors_table = image_num_sectors_table(type);
                for (unsigned int t = D64NUMTRACKS + 1; t <= image_num_tracks(type); t++) {
                    for (int s = 0; s < num_sectors_table[t - 1]; s++) {
                        mark_sector(type, image, t, s, 1 /* free */, shadowdirtrack);
                    }
                }
            } else {
                printf("wrong filesize: expected to read %d bytes, but read %d bytes\n", imagesize, (int) read_size);
                exit(-1);
            }
        }
        if (set_header) {
            update_directory(type, image, header, id, shadowdirtrack);
        }
    }

    /* Create directory entries */
    fprintf(stdout, "creating dir entries\n");
    create_dir_entries(type, image, files, nrFiles, dir_sector_interleave, shadowdirtrack);

    /* Write files and mark sectors in BAM */
    write_files(type, image, files, nrFiles, usedirtrack, dirtracksplit, shadowdirtrack, numdirblocks, dir_sector_interleave);

    if (!quiet) {
        printf("%s (%s,%s):\n", imagepath, header, id);
        print_file_allocation(type, image, files, nrFiles);

        print_bam(type, image);
    }

    /* Save image */
    f = fopen(imagepath, "wb");
    fwrite(image, imagesize, 1, f);
    fclose(f);

    /*if (type == IMAGE_D64) {
        generate_uniformat_g64(image);
    }*/

    free(image);

    return 0;
}
