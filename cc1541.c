/*******************************************************************************
* Copyright (c) 2008-2019 JackAsser, Krill, Claus, Bjoern Esser
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

#define VERSION "2.1"

#define _CRT_SECURE_NO_WARNINGS /* avoid security warnings for MSVC */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

#define DIRENTRIESPERBLOCK     8
#define DIRTRACK_D41_D71       18
#define DIRTRACK_D81           40
#define SECTORSPERTRACK_D81    40
#define MAXNUMFILES_D81        ((SECTORSPERTRACK_D81 - 3) * DIRENTRIESPERBLOCK)
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
#define D81SIZE                (D81NUMTRACKS * SECTORSPERTRACK_D81 * BLOCKSIZE)
#define D64NUMTRACKS           35
#define D64NUMTRACKS_EXTENDED  (D64NUMTRACKS + 5)
#define D71NUMTRACKS           (D64NUMTRACKS * 2)
#define D81NUMTRACKS           80
#define BAM_OFFSET_SPEED_DOS   0xac
#define BAM_OFFSET_DOLPHIN_DOS 0xc0

typedef struct
{
    char* localname;
    char  filename[FILENAMEMAXSIZE + 1];
    int loopindex;
    int direntryindex;
    int direntrysector;
    int direntryoffset;
    int sectorInterleave;
    int first_sector_new_track;
    int track;
    int sector;
    int nrSectors;
    int nrSectorsShown;
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
    IMAGE_D71,
    IMAGE_D81
} image_type;

void
usage()
{
    printf("\n*** This is cc1541 version " VERSION " built on " __DATE__ " ***\n\n");
    printf("Usage: cc1541 -niMFSsfeErbcwLlBxtdu45q image.[d64|g64|d71|d81]\n\n");
    printf("-n diskname   Disk name, default='DEFAULT'.\n");
    printf("-i id         Disk ID, default='LODIS'.\n");
    printf("-M numchars   Hash computation maximum filename length, this must\n");
    printf("              match loader option FILENAME_MAXLENGTH.\n");
    printf("-F            Next file first sector on a new track (default=3).\n");
    printf("              Any negative value assumes aligned tracks and uses current\n");
    printf("              sector + interleave.\n");
    printf("              After each file, the value falls back to the default.\n");
    printf("-S value      Default sector interleave, default=10.\n");
    printf("              At track end, reduces this by 1 to accomodate large tail gap.\n");
    printf("              If negative, no special treatment of tail gap.\n");
    printf("-s value      Next file sector interleave, valid after each file.\n");
    printf("              At track end, reduces this by 1 to accomodate large tail gap.\n");
    printf("              If negative, no special treatment of tail gap.\n");
    printf("              The interleave value falls back to the default value set by -S\n");
    printf("              after the first sector of the next file.\n");
    printf("-f filename   Use filename as name when writing next file, use prefix # to\n");
    printf("              include arbitrary PETSCII characters (e.g. -f \"START#a0,8,1\").\n");
    printf("-e            Start next file on an empty track (default start sector is\n");
    printf("              current sector plus interleave).\n");
    printf("-E            Try to fit file on a single track.\n");
    printf("-r track      Restrict next file blocks to the specified track or higher.\n");
    printf("-b sector     Set next file beginning sector to the specified value.\n");
    printf("-c            Save next file cluster-optimized (d71 only).\n");
    printf("-w localname  Write local file to disk, if filename is not set then the\n");
    printf("              local name is used. After file written, the filename is unset.\n");
    printf("-L fileindex  Write loop file (an additional dir entry) to entry in directory\n");
    printf("              with given index (first file has index 1), set filename with -f.\n");
    printf("-l filename   Write loop file (an additional dir entry) to existing file to\n");
    printf("              disk, set filename with -f.\n");
    printf("-B numblocks  Write the given value as file size in blocks to the directory for\n");
    printf("              the next file.\n");
    printf("-x            Don't split files over dirtrack hole (default split files).\n");
    printf("-t            Use dirtrack to also store files (makes -x useless) (default no).\n");
    printf("-d track      Maintain a shadow directory (copy of the actual directory without a valid BAM).\n");
    printf("-u numblocks  When using -t, amount of dir blocks to leave free (default=2).\n");
    printf("-4            Use tracks 35-40 with SPEED DOS BAM formatting.\n");
    printf("-5            Use tracks 35-40 with DOLPHIN DOS BAM formatting.\n");
    printf("-q            Be quiet.\n");
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

static int num_files = 0;

static int max_hash_length = FILENAMEMAXSIZE;

static unsigned int
filenamehash(const char *filename)
{
  int pos = min(max_hash_length, (int) strlen(filename));
  while ((pos > 0) && (((unsigned char) filename[pos - 1]) == FILENAMEEMPTYCHAR)) {
    --pos;
  }

  unsigned char hashlo = pos;
  unsigned char hashhi = pos;
  int carry = 1;

  for (int i = pos - 1; i >= 0; --i) {
      unsigned int sum = hashlo + filename[i] + carry;
      carry = (sum >= 256) ? 1 : 0;
      sum &= 0xff;
      hashlo = sum;
      sum += hashhi + carry;
      carry = (sum >= 256) ? 1 : 0;
      hashhi = sum;
  }

  return (hashhi << 8) | hashlo;
}

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

        case IMAGE_D81:
            return D81SIZE;

        default:
            return 0;
    }
}

int
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

        case IMAGE_D81:
            return D81NUMTRACKS;

        default:
            return 0;
    }
}

int
dirtrack(image_type type)
{
    return (type == IMAGE_D81) ? DIRTRACK_D81 : DIRTRACK_D41_D71;
}

static int
num_sectors(image_type type, int track)
{
    return (type == IMAGE_D81) ? SECTORSPERTRACK_D81
                               : (((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) ? sectors_per_track_extended[track - 1]
                                                                                                                       : sectors_per_track[track - 1]);
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
    if ((track < 1) || (track > ((type == IMAGE_D81) ? D81NUMTRACKS : ((type == IMAGE_D64) ? D64NUMTRACKS : (type == IMAGE_D71 ? D71NUMTRACKS : D64NUMTRACKS_EXTENDED))))) {
        fprintf(stderr, "Illegal track %d\n", track);

        exit(-1);
    }

    int numsectors = num_sectors(type, track);
    if ((sector < 0) || (sector >= numsectors)) {
        fprintf(stderr, "Illegal sector %d for track %d (max. is %d)\n", sector, track, numsectors - 1);

        exit(-1);
    }

    int linear_sector = 0;
    for (int i = 0; i < track - 1; i++) {
        linear_sector += num_sectors(type, i + 1);
    }
    linear_sector += sector;

    return linear_sector;
}

static char DIRFILENAME[FILENAMEMAXSIZE + 1];

static char *
dirfilename(const char *filename)
{
    for (int i = 0; i < max_hash_length; ++i) {
        if (((unsigned char) filename[i]) != FILENAMEEMPTYCHAR) {
            DIRFILENAME[i] = filename[i];
        } else {
            DIRFILENAME[i] = '\0';

            break;
        }
    }

    return DIRFILENAME;
}

static int
count_hashes(image_type type, unsigned char* image, unsigned int hash, bool print)
{
    int num = 0;

    int dirsector = (type == IMAGE_D81) ? 3 : 1;
    do {
        int dirblock = linear_sector(type, dirtrack(type), dirsector) * BLOCKSIZE;

        for (int i = 0; i < DIRENTRIESPERBLOCK; ++i) {
            int entryOffset = i * DIRENTRYSIZE;
            int filetype = image[dirblock + entryOffset + FILETYPEOFFSET];
            if (filetype != 0) {
                char *filename = (char *) image + dirblock + entryOffset + FILENAMEOFFSET;
                if (hash == filenamehash(filename)) {
                    ++num;

                    if (print) {
                        printf(" [$%04x] \"%s\"\n", filenamehash(filename), dirfilename(filename));
                    }
                }
            }
        }

        if (image[dirblock + TRACKLINKOFFSET] == dirtrack(type)) {
            dirsector = image[dirblock + SECTORLINKOFFSET];
        } else {
            dirsector = 0;
        }
    } while (dirsector > 0);

    return num;
}

static bool
check_hashes(image_type type, unsigned char* image)
{
    bool collision = false;

    int dirsector = 1;
    do {
        int dirblock = linear_sector(type, dirtrack(type), dirsector) * BLOCKSIZE;

        for (int i = 0; i < DIRENTRIESPERBLOCK; ++i) {
            int entryOffset = i * DIRENTRYSIZE;
            int filetype = image[dirblock + entryOffset + FILETYPEOFFSET];
            if (filetype != 0) {
                char *filename = (char *) image + dirblock + entryOffset + FILENAMEOFFSET;
                if (count_hashes(type, image, filenamehash(filename), false /* print */) > 1) {
                    collision = 1;

                    printf("Error: Hash of filename \"%s\" [$%04x] is not unique\n", dirfilename(filename), filenamehash(filename));
                    count_hashes(type, image, filenamehash(filename), true /* print */);
                }
            }
        }

        if (image[dirblock + TRACKLINKOFFSET] == dirtrack(type)) {
            dirsector = image[dirblock + SECTORLINKOFFSET];
        } else {
            dirsector = 0;
        }
    } while (dirsector > 0);

    return collision;
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

    if (type == IMAGE_D81) {
        if (track <= 40) {
            bam = linear_sector(type, dirtrack(type), 1 /* sector */) * BLOCKSIZE;
            bitmap = image + bam + (track * 6) + 11;
        } else {
            bam = linear_sector(type, dirtrack(type), 2 /* sector */) * BLOCKSIZE;
            bitmap = image + bam + ((track - 40) * 6) + 11;
        }
    } else if ((type == IMAGE_D71) && (track > D64NUMTRACKS)) {
        /* access second side bam */
        bam = linear_sector(type, dirtrack(type) + D64NUMTRACKS, 0) * BLOCKSIZE;
        bitmap = image + bam + (track - D64NUMTRACKS - 1) * 3;
    } else {
        if (((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) && (track > D64NUMTRACKS)) {
            track -= D64NUMTRACKS;
            bam = linear_sector(type, dirtrack(type), 0) * BLOCKSIZE + ((type == IMAGE_D64_EXTENDED_SPEED_DOS) ? BAM_OFFSET_SPEED_DOS : BAM_OFFSET_DOLPHIN_DOS);
        } else {
            bam = linear_sector(type, dirtrack(type), 0) * BLOCKSIZE;
        }
        bitmap = image + bam + (track * 4) + 1;
    }

    int byte = sector >> 3;
    int bit = sector & 7;

    int is_not_dir_block = 1;
    if ((track == dirtrack(type)) && (numdirblocks > 0)) {
        int dirsector = 0;
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
                    if (dirsector >= num_sectors(type, track)) {
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
mark_sector(image_type type, unsigned char* image, int track, int sector, int free)
{
    if (free != is_sector_free(type, image, track, sector, 0, 0)) {
        int bam;
        unsigned char* bitmap;
        if (type == IMAGE_D81) {
            if (track <= 40) {
                bam = linear_sector(type, dirtrack(type), 1 /* sector */) * BLOCKSIZE;
                bitmap = image + bam + (track * 6) + 11;
            } else {
                bam = linear_sector(type, dirtrack(type), 2 /* sector */) * BLOCKSIZE;
                bitmap = image + bam + ((track - 40) * 6) + 11;
            }

            /* update number of free sectors on track */
            if (free) {
                ++bitmap[-1];
            } else {
                --bitmap[-1];
            }
        } else if ((type == IMAGE_D71) && (track > D64NUMTRACKS)) {
            /* access second side bam */
            bam = linear_sector(type, dirtrack(type) + D64NUMTRACKS, 0) * BLOCKSIZE;
            bitmap = image + bam + (track - D64NUMTRACKS - 1) * 3;

            /* update number of free sectors on track */
            if (free) {
                image[bam + 0xdd + track - D64NUMTRACKS - 1]++;
            } else {
                image[bam + 0xdd + track - D64NUMTRACKS - 1]--;
            }
                } else {
            if (((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) && (track > D64NUMTRACKS)) {
                track -= D64NUMTRACKS;
                bam = linear_sector(type, dirtrack(type), 0) * BLOCKSIZE + ((type == IMAGE_D64_EXTENDED_SPEED_DOS) ? BAM_OFFSET_SPEED_DOS : BAM_OFFSET_DOLPHIN_DOS);
            } else {
                bam = linear_sector(type, dirtrack(type), 0) * BLOCKSIZE;
                }
            bitmap = image + bam + (track * 4) + 1;

            /* update number of free sectors on track */
            if (free) {
                ++image[bam + (track * 4)];
            } else {
                --image[bam + (track * 4)];
                }
            }

        /* update bitmap */
        int byte = sector >> 3;
        int bit = sector & 7;

        if (free) {
            bitmap[byte] |= 1 << bit;
        } else {
            bitmap[byte] &= ~(1 << bit);
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
hex2int(char hex)
{
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
evalhexescape(char *str)
{
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
    unsigned int bam = linear_sector(type, dirtrack(type), 0) * BLOCKSIZE;

    unsigned int header_offset;
    unsigned int id_offset;
    if (type == IMAGE_D81) {
        header_offset = 4;
        id_offset = 0x16;
    } else {
    	image[bam + 0x03] = (type == IMAGE_D71) ? 0x80 : 0x00;
        header_offset = 0x90;
        id_offset = 0xa2;
    }

    /* Set header and ID */
    for (size_t i = 0; i < 16; ++i) {
        if (i < strlen(header)) {
            image[bam + header_offset + i] = header[i];
        } else {
            image[bam + header_offset + i] = FILENAMEEMPTYCHAR;
        }
    }

    static const char DEFAULT_ID[] = "00 2A";

    for (size_t i = 0; i < 5; ++i)    {
        if (i < strlen(id)) {
            image[bam + id_offset + i] = id[i];
        } else {
            image[bam + id_offset + i] = DEFAULT_ID[i];
        }
    }

    if (type == IMAGE_D81) {
        unsigned int bam = linear_sector(type, dirtrack(type), 1 /* sector */) * BLOCKSIZE;
        image[bam + 0x04] = id[0];
        image[bam + 0x05] = id[1];

        bam = linear_sector(type, dirtrack(type), 2 /* sector */) * BLOCKSIZE;
        image[bam + 0x04] = id[0];
        image[bam + 0x05] = id[1];
    }

    if (shadowdirtrack > 0) {
        unsigned int shadowbam = linear_sector(type, shadowdirtrack, 0 /* sector */) * BLOCKSIZE;
        memcpy(image + shadowbam, image + bam, BLOCKSIZE);

        image[shadowbam + 0x00] = shadowdirtrack;
    }
}

static void
initialize_directory(image_type type, unsigned char* image, char* header, char* id, int shadowdirtrack)
{
    unsigned int dir = linear_sector(type, dirtrack(type), 0 /* sector */) * BLOCKSIZE;

    /* Clear image */
    memset(image, 0, image_size(type));

    /* Write initial BAM */
    if (type == IMAGE_D81) {
        image[dir + 0x00] = dirtrack(type);
        image[dir + 0x01] = 3;
        image[dir + 0x02] = 0x44;

        image[dir + 0x14] = FILENAMEEMPTYCHAR;
        image[dir + 0x15] = FILENAMEEMPTYCHAR;

        image[dir + 0x1b] = FILENAMEEMPTYCHAR;
        image[dir + 0x1c] = FILENAMEEMPTYCHAR;

        unsigned int bam = linear_sector(type, dirtrack(type), 1 /* sector */) * BLOCKSIZE;
        image[bam + 0x00] = dirtrack(type);
        image[bam + 0x01] = 2;
        image[bam + 0x02] = 0x44;
        image[bam + 0x03] = 0xbb;
        image[bam + 0x06] = 0xc0;

        bam = linear_sector(type, dirtrack(type), 2 /* sector */) * BLOCKSIZE;
        image[bam + 0x00] = 0;
        image[bam + 0x01] = 255;
        image[bam + 0x02] = 0x44;
        image[bam + 0x03] = 0xbb;
        image[bam + 0x06] = 0xc0;
    } else {
        image[dir + 0x00] = dirtrack(type);
        image[dir + 0x01] = 1;
        image[dir + 0x02] = 0x41;
        image[dir + 0x03] = (type == IMAGE_D71) ? 0x80 : 0x00;

        image[dir + 0xa0] = FILENAMEEMPTYCHAR;
        image[dir + 0xa1] = FILENAMEEMPTYCHAR;

        image[dir + 0xa7] = FILENAMEEMPTYCHAR;
        image[dir + 0xa8] = FILENAMEEMPTYCHAR;
        image[dir + 0xa9] = FILENAMEEMPTYCHAR;
        image[dir + 0xaa] = FILENAMEEMPTYCHAR;
    }

    /* Mark all sectors unused */
    for (int t = 1; t <= image_num_tracks(type); t++) {
        for (int s = 0; s < num_sectors(type, t); s++) {
            mark_sector(type, image, t, s, 1 /* free */);
        }
    }

    /* Reserve space for BAM */
    mark_sector(type, image, dirtrack(type), 0 /* sector */, 0 /* not free */);
    if (type == IMAGE_D71) {
        mark_sector(type, image, dirtrack(type) + D64NUMTRACKS, 0 /* sector */, 0 /* not free */);
    } else if (type == IMAGE_D81) {
        mark_sector(type, image, dirtrack(type), 1 /* sector */, 0 /* not free */);
        mark_sector(type, image, dirtrack(type), 2 /* sector */, 0 /* not free */);
    }

    /* first dir block */
    unsigned int dirblock = linear_sector(type, dirtrack(type), (type == IMAGE_D81) ? 3 : 1) * BLOCKSIZE;
    image[dirblock + SECTORLINKOFFSET] = 255;
    mark_sector(type, image, dirtrack(type), (type == IMAGE_D81) ? 3 : 1 /* sector */, 0 /* not free */);

    if (shadowdirtrack > 0) {
        dirblock = linear_sector(type, shadowdirtrack, (type == IMAGE_D81) ? 3 : 1 /* sector */) * BLOCKSIZE;
        image[dirblock + SECTORLINKOFFSET] = 255;

        mark_sector(type, image, shadowdirtrack, 0 /* sector */, 0 /* not free */);
        mark_sector(type, image, shadowdirtrack, (type == IMAGE_D81) ? 3 : 1 /* sector */, 0 /* not free */);
    }

    update_directory(type, image, header, id, shadowdirtrack);
}

static void
wipe_file(image_type type, unsigned char* image, unsigned int track, unsigned int sector)
{
    if (sector >= 0x80) {
      return; /* loop file */
    }

    while (track != 0) {
        int block_offset = linear_sector(type, track, sector) * BLOCKSIZE;
        int next_track = image[block_offset + TRACKLINKOFFSET];
        int next_sector = image[block_offset + SECTORLINKOFFSET];
        memset(image + block_offset, 0, BLOCKSIZE);
        mark_sector(type, image, track, sector, 1 /* free */);
        track = next_track;
        sector = next_sector;
    }
}

static int
find_file(image_type type, unsigned char* image, char* filename, unsigned char *track, unsigned char *sector, int *numblocks)
{
    int direntryindex = 0;

    int dirsector = (type == IMAGE_D81) ? 3 : 1;
    int dirblock;
    int entryOffset;
    do {
        dirblock = linear_sector(type, dirtrack(type), dirsector) * BLOCKSIZE;
        for (int j = 0; j < DIRENTRIESPERBLOCK; ++j) {
            entryOffset = j * DIRENTRYSIZE;
            int filetype = image[dirblock + entryOffset + FILETYPEOFFSET];
            switch (filetype) {
                case FILETYPE_PRG:
                    if (dirstrcmp((char *) image + dirblock + entryOffset + FILENAMEOFFSET, filename) == 0) {
                        *track = image[dirblock + entryOffset + FILETRACKOFFSET];
                        *sector = image[dirblock + entryOffset + FILESECTOROFFSET];
                        *numblocks = image[dirblock + entryOffset + FILEBLOCKSLOOFFSET] + 256*image[dirblock + entryOffset + FILEBLOCKSHIOFFSET];
                        return direntryindex;
                    }
                    break;

                default:
                    break;
            }

            ++direntryindex;
        }

        /* file not found in current dir block, try next */
        if (image[dirblock + TRACKLINKOFFSET] == dirtrack(type)) {
            dirsector = image[dirblock + SECTORLINKOFFSET];

            continue;
        }

        break;
    } while (1);

    return -1;
}

/* sets image offset to the next DIR entry, returns 0 when the DIR end was reached */
static int
next_dir_entry(image_type type, unsigned char* image, int *offset)
{
    if (*offset % BLOCKSIZE == 7 * DIRENTRYSIZE) {
        /* last entry in sector */
        int sector_offset = (*offset / BLOCKSIZE) * BLOCKSIZE;
        int track = image[sector_offset];
        if (track == 0) {
            /* this was the last DIR sector */
            return 0;
        } else {
            /* follow the t/s link */
            int sector = image[sector_offset + 1];
            *offset = linear_sector(type, track, sector) * BLOCKSIZE;
        }
    } else {
        *offset += DIRENTRYSIZE;
    }
    return 1;
}

static int
num_dir_entries(image_type type, unsigned char* image)
{
    int entries = 0;
    int offset = linear_sector(type, dirtrack(type), 1) * BLOCKSIZE;
    do {
        if (image[offset + 2] != 0) {
            entries++;
        }
    } while (next_dir_entry(type, image, &offset));
    return entries;
}

static void
create_dir_entries(image_type type, unsigned char* image, imagefile* files, int num_files, int dir_sector_interleave, unsigned int shadowdirtrack)
{
    /* this does not check for uniqueness of filenames */

    int num_overwritten_files = 0;

    for (int i = 0; i < num_files; i++) {
        /* find or create slot */
        imagefile *file = files + i;

        fprintf(stdout, "[$%04x] \"%s\"\n", filenamehash(file->filename), file->filename);

        int direntryindex = 0;

        int dirsector = (type == IMAGE_D81) ? 3 : 1;
        int dirblock;
        int shadowdirblock = 0;
        int entryOffset;
        int found = 0;
        do {
            dirblock = linear_sector(type, dirtrack(type), dirsector) * BLOCKSIZE;
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
                            wipe_file(type, image, image[dirblock + entryOffset + FILETRACKOFFSET], image[dirblock + entryOffset + FILESECTOROFFSET]);
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
                if (image[dirblock + TRACKLINKOFFSET] == dirtrack(type)) {
                    dirsector = image[dirblock + SECTORLINKOFFSET];
                } else {
                    /* allocate new dir block */
                    int next_sector;
                    for (next_sector = dirsector + dir_sector_interleave; next_sector < dirsector + num_sectors(type, dirtrack(type)); next_sector++) {
                        int findSector = next_sector % num_sectors(type, dirtrack(type));
                        if (is_sector_free(type, image, dirtrack(type), findSector, 0, 0)) {
                            found = 1;
                            next_sector = findSector;
                            break;
                        }
                    }
                    if (!found) {
                        fprintf(stderr, "Dir track full\n");

                        exit(-1);
                    }

                    image[dirblock + TRACKLINKOFFSET] = dirtrack(type);
                    image[dirblock + SECTORLINKOFFSET] = next_sector;

                    mark_sector(type, image, dirtrack(type), next_sector, 0 /* not free */);

                    /* initialize new dir block */
                    dirblock = linear_sector(type, dirtrack(type), next_sector) * BLOCKSIZE;

                    memset(image + dirblock, 0, BLOCKSIZE);
                    image[dirblock + TRACKLINKOFFSET] = 0;
                    image[dirblock + SECTORLINKOFFSET] = 255;

                    if (shadowdirtrack > 0) {
                        image[shadowdirblock + TRACKLINKOFFSET] = shadowdirtrack;
                        image[shadowdirblock + SECTORLINKOFFSET] = next_sector;

                        mark_sector(type, image, shadowdirtrack, next_sector, 0 /* not free */);

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

        /* set filetype */
        image[dirblock + entryOffset + FILETYPEOFFSET] = FILETYPE_PRG;

        /* set filename */
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
        printf("%3d (0x%02x 0x%02x:0x%02x) \"%s\" => \"%s\" [$%04x] (SL: %d)", files[i].nrSectors, files[i].direntryindex, files[i].direntrysector, files[i].direntryoffset,
                                                                               files[i].localname, files[i].filename, filenamehash(files[i].filename), files[i].sectorInterleave);
        int track = files[i].track;
        int sector = files[i].sector;
        int j = 0;
        while (track != 0) {
            if (j == 0) {
                printf("\n    ");
            }
            printf("%02d/%02d ", track, sector);
            int offset = linear_sector(type, track, sector) * BLOCKSIZE;
            int next_track = image[offset + 0];
            int next_sector = image[offset + 1];
            if ((track != next_track) && (next_track != 0)) {
                printf("-");
            } else if ((next_sector < sector) && (next_track != 0)) {
                printf(".");
            } else {
                printf(" ");
            }
            track = next_track;
            sector = image[offset + 1];
            j++;
            if (j == 10) {
                j = 0;
            }
        }
        printf("\n");
    }
}

static bool
fileontrack(image_type type, unsigned char *image, int track, int filetrack, int filesector)
{
    while (filetrack != 0) {
        if (filetrack == track) {
            return true;
        }

        int block_offset = linear_sector(type, filetrack, filesector) * BLOCKSIZE;
        int next_track = image[block_offset + TRACKLINKOFFSET];
        int next_sector = image[block_offset + SECTORLINKOFFSET];
        filetrack = next_track;
        filesector = next_sector;
    }

    return false;
}

static void
print_track_usage(image_type type, unsigned char *image, int track)
{
    int dirsector = (type == IMAGE_D81) ? 3 : 1;
    do {
        int dirblock = linear_sector(type, dirtrack(type), dirsector) * BLOCKSIZE;

        for (int i = 0; i < DIRENTRIESPERBLOCK; ++i) {
            int entryOffset = i * DIRENTRYSIZE;
            int filetype = image[dirblock + entryOffset + FILETYPEOFFSET];
            if (filetype != 0) {
                int filetrack = image[dirblock + entryOffset + FILETRACKOFFSET];
                int filesector = image[dirblock + entryOffset + FILESECTOROFFSET];
                bool ontrack = (type == IMAGE_D71) ? fileontrack(type, image, track, (filetrack > D64NUMTRACKS) ? filetrack - D64NUMTRACKS : filetrack + D64NUMTRACKS, filesector) : false;
                if (ontrack || fileontrack(type, image, track, filetrack, filesector)) {
                    char *filename = (char *) image + dirblock + entryOffset + FILENAMEOFFSET;
                    printf("\"%s\" ", dirfilename(filename));
                }
            }
        }

        if (image[dirblock + TRACKLINKOFFSET] == dirtrack(type)) {
            dirsector = image[dirblock + SECTORLINKOFFSET];
        } else {
            dirsector = 0;
        }
    } while (dirsector > 0);
}

static void
print_bam(image_type type, unsigned char* image)
{
    int sectorsFree = 0;
    int sectorsFreeOnDirTrack = 0;
    int sectorsOccupied = 0;
    int sectorsOccupiedOnDirTrack = 0;

    int max_track = (type == IMAGE_D81) ? D81NUMTRACKS
                                        : (((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) ? D64NUMTRACKS_EXTENDED
                                                                                                                                : D64NUMTRACKS);
    for (int t = 1; t <= max_track; t++) {

        printf("%2d: ", t);
        for (int s = 0; s < num_sectors(type, t); s++) {
            if (is_sector_free(type, image, t, s, 0, 0)) {
                printf("0");
                if (t != dirtrack(type)) {
                    sectorsFree++;
                } else {
                    sectorsFreeOnDirTrack++;
                }
            } else {
                printf("1");
                if (t != dirtrack(type)) {
                    sectorsOccupied++;
                } else {
                    sectorsOccupiedOnDirTrack++;
                }
            }
        }

        if (type == IMAGE_D71) {
            for (int i = num_sectors(type, t); i < 23; i++) {
                printf(" ");
            }
            int t2 = t + D64NUMTRACKS;

            printf("%2d: ", t2);
            for (int s = 0; s < num_sectors(type, t2); s++) {
                if (is_sector_free(type, image, t2, s, 0, 0)) {
                    printf("0");
                    if (t2 != dirtrack(type)) {
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

        for (int i = ((type == IMAGE_D81) ? 42 : 23) - num_sectors(type, t); i > 0; --i) {
            printf(" ");
        }
        print_track_usage(type, image, t);
        printf("\n");
    }
    printf("%3d (%d) BLOCKS FREE (out of %d (%d) BLOCKS)\n", sectorsFree, sectorsFree + sectorsFreeOnDirTrack,
                                                             sectorsFree + sectorsOccupied, sectorsFree + sectorsFreeOnDirTrack + sectorsOccupied + sectorsOccupiedOnDirTrack);
}

static void
write_files(image_type type, unsigned char *image, imagefile *files, int num_files, int usedirtrack, int dirtracksplit, int shadowdirtrack, int numdirblocks, int dir_sector_interleave)
{
    unsigned char track = 1;
    unsigned char sector = 0;
    int bytes_to_write = 0;
    int lastTrack = track;
    int lastSector = sector;
    int lastOffset = linear_sector(type, lastTrack, lastSector) * BLOCKSIZE;

    /* make sure the first file already takes first sector per track into account */
    if (num_files > 0) {
        sector = (type == IMAGE_D81) ? 0 : files[0].first_sector_new_track;
    }

    for (int i = 0; i < num_files; i++) {
        imagefile *file = files + i;
        if (type == IMAGE_D81) {
            file->sectorInterleave = 1;
        }

        if (file->mode & MODE_LOOPFILE) {


            int direntryindex = file->loopindex;
            track = files[file->loopindex].track;
            sector = files[file->loopindex].sector;
            file->nrSectors = files[file->loopindex].nrSectors;

            if (file->localname != NULL) {
              /* find loopfile source file */
                direntryindex = find_file(type, image, file->localname, &track, &sector, &file->nrSectors);
            } else {
                if (direntryindex == file->direntryindex) {
                    fprintf(stderr, "Loop file index %d cannot refer to itself\n", file->loopindex);
                    exit(-1);
                }
                int numfiles = num_dir_entries(type, image);
                if (direntryindex >= numfiles) {
                    fprintf(stderr, "Loop file index %d is higher than number of files %d\n", file->loopindex, numfiles);
                    exit(-1);
                }
            }
            if (direntryindex >= 0) {
                file->track = track;
                file->sector = sector;

                /* update directory entry */
                int entryOffset = linear_sector(type, dirtrack(type), file->direntrysector) * BLOCKSIZE + file->direntryoffset;
                image[entryOffset + FILETRACKOFFSET] = file->track;
                image[entryOffset + FILESECTOROFFSET] = file->sector;

                if (file->nrSectorsShown == -1) {
                    file->nrSectorsShown = file->nrSectors;
                }
                image[entryOffset + FILEBLOCKSLOOFFSET] = file->nrSectorsShown % 256;
                image[entryOffset + FILEBLOCKSHIOFFSET] = file->nrSectorsShown / 256;

                if (shadowdirtrack > 0) {
                    entryOffset = linear_sector(type, shadowdirtrack, file->direntrysector) * BLOCKSIZE + file->direntryoffset;
                    image[entryOffset + FILETRACKOFFSET] = file->track;
                    image[entryOffset + FILESECTOROFFSET] = file->sector;

                    image[entryOffset + FILEBLOCKSLOOFFSET] = file->nrSectors;
                    image[entryOffset + FILEBLOCKSHIOFFSET] = file->nrSectors;
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
            fprintf(stderr, "Memory allocation error\n");

            exit(-1);
        }
        FILE* f = fopen(file->localname, "rb");
        if (f == NULL) {
            fprintf(stderr, "Could not open file \"%s\" for reading\n", file->localname);

            exit(-1);
        }
        if (fread(filedata, fileSize, 1, f) != 1) {
            fprintf(stderr, "ERROR: Unexpected filesize when reading %s\n", file->localname);
            exit(-1);
        }
        fclose(f);

        if ((file->mode & MODE_MIN_TRACK_MASK) > 0) {
            track = (file->mode & MODE_MIN_TRACK_MASK) >> MODE_MIN_TRACK_SHIFT;
            /* note that track may be smaller than lastTrack now */
            if (track > image_num_tracks(type)) {
                fprintf(stderr, "Invalid minimum track %d for file %s (%s) specified\n", track, file->localname, file->filename);

                exit(-1);
            }
            if ((!usedirtrack)
             && ((track == dirtrack(type)) || (track == shadowdirtrack)
              || ((type == IMAGE_D71) && (track == (D64NUMTRACKS + dirtrack(type)))))) { /* .d71 track 53 is usually empty except the extra BAM block */
              ++track; /* skip dir track */
            }
            if ((track - lastTrack) > 1) {
                /* previous file's last track and this file's beginning track have tracks in between */
                sector = (type == IMAGE_D81) ? 0 : file->first_sector_new_track;
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
                for (int s = 0; s < num_sectors(type, track); s++) {
                    if (is_sector_free(type, image, track, s, usedirtrack ? numdirblocks : 0, dir_sector_interleave)) {
                        if (s == (num_sectors(type, track) - 1)) {
                            found = 1;
                            /* In first pass, use sector as left by previous file (or as set by -b) to reach first file block quickly. */
                            /* Claus: according to Krill, on real HW tracks are not aligned anyway, so it does not make a difference. */
                            /* Emulators tend to reset the disk angle on track changes, so this should rather be 3. */
                            if (sector >= num_sectors(type, track)) {
                                if ((file->mode & MODE_BEGINNING_SECTOR_MASK) > 0) {
                                    fprintf(stderr, "Invalid beginning sector %d on track %d for file %s (%s) specified\n", sector, track, file->localname, file->filename);

                                    exit(-1);
                                }

                                sector %= num_sectors(type, track);
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
                         && ((track == dirtrack(type)) || (track == shadowdirtrack)
                          || ((type == IMAGE_D71) && (track == D64NUMTRACKS + dirtrack(type))))) { /* .d71 track 53 is usually empty except the extra BAM block */
                            ++track; /* skip dir track */
                        }
                        if (file->mode & MODE_FITONSINGLETRACK) {
                            int file_size = fileSize;
                            int first_sector = -1;
                            for (int s = 0; s < num_sectors(type, prev_track); s++) {
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
        if (type == IMAGE_D81) {
            sector = 0;
        }

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
                for (int s = sector; s < sector + num_sectors(type, track); s++) {
                    findSector = s % num_sectors(type, track);

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
                        if (track > D64NUMTRACKS) {
                            track = track - D64NUMTRACKS + 1;
                        } else {
                            track += D64NUMTRACKS;
                            seek_delay = 0; /* switching to the other side, no head movement */
                        }
                    } else {
                        ++track;
                    }
                    if (type == IMAGE_D81) {
                        sector = 0;
                    } else if (file->first_sector_new_track < 0) {
                        sector += seek_delay - 1;
                    } else {
                        sector = file->first_sector_new_track;
                    }
                    sector += num_sectors(type, prev_track);
                    sector %= num_sectors(type, prev_track);

                    if ((!usedirtrack)
                     && ((track == dirtrack(type)) || (track == shadowdirtrack)
                      || ((type == IMAGE_D71) && (track == D64NUMTRACKS + dirtrack(type))))) { /* .d71 track 53 is usually empty except the extra BAM block */
                        /* Delete old fragments and restart file */
                        if (!dirtracksplit) {
                            if (file->nrSectors > 0) {
                                int deltrack = file->track;
                                int delsector = file->sector;
                                while (deltrack != 0) {
                                    mark_sector(type, image, deltrack, delsector, 1 /* free */);
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
                          track = dirtrack(type) + 1;
                        }
                        if (track == dirtrack(type)) {
                          ++track;
                        }
                    }

                    if (track > image_num_tracks(type)) {
                        print_file_allocation(type, image, files, num_files);
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

            mark_sector(type, image, track, sector, 0 /* not free */);

            if (num_sectors(type, track) <= abs(file->sectorInterleave)) {
                fprintf(stderr, "Invalid interleave %d on track %d (%d sectors), file %s (%s)\n", file->sectorInterleave, track, num_sectors(type, track), file->localname, file->filename);

                exit(-1);
            }

            sector += abs(file->sectorInterleave);
            if (sector >= num_sectors(type, track)) {
                sector -= num_sectors(type, track);
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
        int entryOffset = linear_sector(type, dirtrack(type), file->direntrysector) * BLOCKSIZE + file->direntryoffset;
        image[entryOffset + FILETRACKOFFSET] = file->track;
        image[entryOffset + FILESECTOROFFSET] = file->sector;

        if (file->nrSectorsShown < 0) {
            file->nrSectorsShown = file->nrSectors;
        }
        image[entryOffset + FILEBLOCKSLOOFFSET] = file->nrSectorsShown & 255;
        image[entryOffset + FILEBLOCKSHIOFFSET] = file->nrSectorsShown >> 8;

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
generate_uniformat_g64(unsigned char* image, const char *imagepath)
{
    FILE* f = fopen(imagepath, "wb");

    size_t filepos = 0;

    static const char signature[] = "GCR-1541";
    filepos += fwrite(signature, 1, sizeof signature - 1, f);

    const char version = 0;
    filepos += fwrite(&version, 1, 1, f);

    const char num_tracks = 35 * 2;
    filepos += fwrite(&num_tracks, 1, 1, f);

    const unsigned int track_size = 7692; /* = track_bytes on tracks 1..17 */
    filepos += write16(track_size, f);

    const unsigned int table_size = num_tracks * 4;
    const unsigned int tracks_offset = (int)filepos + (table_size * 2);

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

    const unsigned char sync[5] = { 0xff, 0xff, 0xff, 0xff, 0xff };
    const char gap[9] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    char header_gcr[10];

    const unsigned int block_size =
          (sizeof sync)
        + (sizeof header_gcr)
        + (sizeof gap)
        + (sizeof sync)
        + 325; /* data */

    const char id[3] = { '2', 'A', '\0' };

    bool is_uniform = true;

    for (int track = 0; track < (num_tracks >> 1); ++track) {
        int track_bytes = 0;
        int num_sectors = sectors_per_track[track];
        switch (num_sectors) {
            case 21: track_bytes = 7692; break; /* = track_size */
            case 19: track_bytes = 7142; break;
            case 18: track_bytes = 6666; break;
            case 17: track_bytes = 6250; break;
        }

        filepos += write16(track_bytes, f);
        size_t track_begin = filepos;

        int data_bytes = num_sectors * block_size;
        int gap_size = (track_bytes - data_bytes) / num_sectors;
        if (gap_size < 0) {
            printf("Error: track too small\n");

            exit(-1);
        }

        float average_gap_remainder = (((float) (track_bytes - data_bytes)) / num_sectors) - gap_size;
        if (average_gap_remainder >= 1.0f) {
            average_gap_remainder = 0.0f; /* 0..1 */
        }

        float remainder = 0.0f;
        for (int sector = 0; sector < num_sectors; ++sector) {
            unsigned int gap_bytes = gap_size;
            remainder += average_gap_remainder;
            if (remainder >= 0.5f) {
                remainder -= 1.0f;
                ++gap_bytes;
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

            encode_4_bytes_gcr(header, header_gcr);
            encode_4_bytes_gcr(header + 4, header_gcr + 5);

            filepos += fwrite(header_gcr, 1, sizeof header_gcr, f);
            filepos += fwrite(gap, 1, sizeof gap, f);

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

            for (int i = gap_bytes; i > 0; --i) {
                filepos += fwrite(&gap, 1, 1, f);
            }

            image += 0x0100;
        } /* for each sector */

        size_t tail_gap = track_bytes - filepos + track_begin;
        if (tail_gap > 0) {
            for (size_t i = tail_gap; i > 0; --i) {
                filepos += fwrite(&gap, 1, 1, f);
        }

            is_uniform = false;
        }

        for (int i = (track_size - track_bytes); i > 0; --i) {
            filepos += fwrite(sync, 1, 1, f);
        }
    } /* for each track */

    fclose(f);

    if (!is_uniform) {
        printf("Warning: \"%s\" is not UniFormAt'ed\n", imagepath);
}
}

int
main(int argc, char* argv[])
{
    imagefile files[MAXNUMFILES_D81];
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
    int sectorInterleave = 0;
    int dir_sector_interleave = 3;
    int numdirblocks = 2;
    int nrSectorsShown = -1;
    int loopindex;
    char* filename = NULL;
    int set_header = 0;
    int retval = 0;

    int i, j;

    if (argc == 1 || strcmp(argv[argc-1], "-h") == 0) {
        usage();
    }
    for (j = 1; j < argc - 1; j++) {
        if (strcmp(argv[j], "-n") == 0) {
            if (argc < j + 2) {
                fprintf(stderr, "Error parsing argument for -n\n");

                exit(-1);
            }
            header = argv[++j];
            set_header = 1;
        } else if (strcmp(argv[j], "-i") == 0) {
            if (argc < j + 2) {
                fprintf(stderr, "Error parsing argument for -i\n");

                exit(-1);
            }
            id = argv[++j];
            set_header = 1;
        } else if (strcmp(argv[j], "-M") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &max_hash_length)) {
                fprintf(stderr, "Error parsing argument for -M\n");

                exit(-1);
            }
            if ((max_hash_length < 1) || (max_hash_length > FILENAMEMAXSIZE)) {
                fprintf(stderr, "Hash computation maximum filename length %d specified\n", max_hash_length);

                exit(-1);
            }
        } else if (strcmp(argv[j], "-F") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &first_sector_new_track)) {
                fprintf(stderr, "Error parsing argument for -F\n");

                exit(-1);
            }
        } else if (strcmp(argv[j], "-S") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &defaultSectorInterleave)) {
                fprintf(stderr, "Error parsing argument for -S\n");

                exit(-1);
            }
        } else if (strcmp(argv[j], "-s") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &sectorInterleave)) {
                fprintf(stderr, "Error parsing argument for -s\n");

                exit(-1);
            }
        } else if (strcmp(argv[j], "-f") == 0) {
            if (argc < j + 2) {
                fprintf(stderr, "Error parsing argument for -f\n");

                exit(-1);
            }
            filename = argv[++j];
        } else if (strcmp(argv[j], "-e") == 0) {
            files[num_files].mode |= MODE_SAVETOEMPTYTRACKS;
        } else if (strcmp(argv[j], "-E") == 0) {
            files[num_files].mode |= MODE_FITONSINGLETRACK;
        } else if (strcmp(argv[j], "-r") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &i)) {
                fprintf(stderr, "Error parsing argument for -r\n");

                exit(-1);
            }
            if ((i < 1) || (((i << MODE_MIN_TRACK_SHIFT) & MODE_MIN_TRACK_MASK) != (i << MODE_MIN_TRACK_SHIFT))) {
                fprintf(stderr, "Invalid minimum track %d specified\n",  i);

                exit(-1);
            }
            files[num_files].mode = (files[num_files].mode & ~MODE_MIN_TRACK_MASK) | (i << MODE_MIN_TRACK_SHIFT);
        } else if (strcmp(argv[j], "-b") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &i)) {
                fprintf(stderr, "Error parsing argument for -b\n");

                exit(-1);
            }
            if ((i < 0) || (i >= num_sectors(type, 1))) {
                fprintf(stderr, "Invalid beginning sector %d specified\n", i);

                exit(-1);
            }
            files[num_files].mode = (files[num_files].mode & ~MODE_BEGINNING_SECTOR_MASK) | (i + 1);
        } else if (strcmp(argv[j], "-c") == 0) {
            files[num_files].mode |= MODE_SAVECLUSTEROPTIMIZED;
        } else if (strcmp(argv[j], "-w") == 0) {
            if (argc < j + 2) {
                printf("Error parsing argument for -w\n");
                return -1;
            }
            struct stat st;
            if (stat(argv[j + 1], &st) != 0) {
                fprintf(stderr, "File '%s' (%d) not found\n", argv[j + 1], num_files + 1);
                exit(-1);
            }
            files[num_files].localname = argv[j + 1];
                if (filename == NULL) {
                    strncpy(files[num_files].filename, files[num_files].localname, FILENAMEMAXSIZE + 1);
                    ascii2petscii(files[num_files].filename);
                } else {
                    evalhexescape(filename);
                    strncpy(files[num_files].filename, filename, FILENAMEMAXSIZE + 1);
                }
            files[num_files].filename[FILENAMEMAXSIZE] = '\0';
            files[num_files].sectorInterleave = sectorInterleave ? sectorInterleave : defaultSectorInterleave;
            files[num_files].first_sector_new_track = first_sector_new_track;
            files[num_files].nrSectorsShown = nrSectorsShown;
            first_sector_new_track = default_first_sector_new_track;
            filename = NULL;
            sectorInterleave = 0;
            nrSectorsShown = -1;
            num_files++;
            j++;
        } else if (strcmp(argv[j], "-l") == 0) {
            if (argc < j + 2) {
                printf("Error parsing argument for -l\n");
                return -1;
            }
            if (filename == NULL) {
              printf("Loop files require a filename set with -f\n");
              return -1;
            }
            files[num_files].mode |= MODE_LOOPFILE;
            files[num_files].localname = argv[j + 1];
            evalhexescape(filename);
            strncpy(files[num_files].filename, filename, FILENAMEMAXSIZE);
            files[num_files].sectorInterleave = sectorInterleave ? sectorInterleave : defaultSectorInterleave;
            files[num_files].first_sector_new_track = first_sector_new_track;
            first_sector_new_track = default_first_sector_new_track;
            files[num_files].nrSectorsShown = nrSectorsShown;
            filename = NULL;
            sectorInterleave = 0;
            nrSectorsShown = -1;
            num_files++;
            j++;
        } else if (strcmp(argv[j], "-L") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &loopindex)) {
                printf("Error parsing argument for -L\n");
                return -1;
            }
            if (loopindex < 1) {
                printf("Argument must be greater or equal to 1 for -L\n");
                return -1;
            }
            if (filename == NULL) {
              printf("Loop files require a filename set with -f\n");
              return -1;
            }
            files[num_files].mode |= MODE_LOOPFILE;
            files[num_files].loopindex = loopindex - 1;
            evalhexescape(filename);
            strncpy(files[num_files].filename, filename, FILENAMEMAXSIZE);
            files[num_files].sectorInterleave = sectorInterleave ? sectorInterleave : defaultSectorInterleave;
            files[num_files].first_sector_new_track = first_sector_new_track;
            files[num_files].nrSectorsShown = nrSectorsShown;
            first_sector_new_track = default_first_sector_new_track;
            filename = NULL;
            sectorInterleave = 0;
            nrSectorsShown = -1;
            num_files++;
        } else if (strcmp(argv[j], "-x") == 0) {
            dirtracksplit = 0;
        } else if (strcmp(argv[j], "-t") == 0) {
            usedirtrack = 1;
        } else if (strcmp(argv[j], "-d") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%u", &shadowdirtrack)) {
                fprintf(stderr, "Error parsing argument for -d\n");

                exit(-1);
            }
        } else if (strcmp(argv[j], "-u") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &numdirblocks)) {
                fprintf(stderr, "Error parsing argument for -u\n");

                exit(-1);
            }
        } else if (strcmp(argv[j], "-B") == 0) {
            if ((argc < j + 2) || !sscanf(argv[++j], "%d", &nrSectorsShown)) {
                printf("Error parsing argument for -B\n");
                return -1;
            }
            if (nrSectorsShown < 0 || nrSectorsShown > 65535) {
                printf("Argument must be between 0 and 65535 for -B\n");
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
            fprintf(stderr, "Error parsing commandline at \"%s\"\n", argv[j]);
            printf("Use -h for help.");

            exit(-1);
        }
    }
    if (j >= argc) {
        fprintf(stderr, "No image file provided, or misparsed last option\n");

        exit(-1);
    }
    imagepath = argv[argc-1];

    if (strlen(imagepath) >= 4) {
        if (strcmp(imagepath + strlen(imagepath) - 4, ".d71") == 0) {
			if ((type == IMAGE_D64_EXTENDED_SPEED_DOS) || (type == IMAGE_D64_EXTENDED_DOLPHIN_DOS)) {
					fprintf(stderr, "Extended .d71 images are not supported\n");
	
				exit(-1);
			}
			type = IMAGE_D71;
        } else if (strcmp(imagepath + strlen(imagepath) - 4, ".d81") == 0) {
            type = IMAGE_D81;
            dir_sector_interleave = 1;
        }
    }

    /* open image */
    unsigned int imagesize = image_size(type);
    unsigned char* image = (unsigned char*)calloc(imagesize, sizeof(unsigned char));
    if (image == NULL) {
        fprintf(stderr, "Memory allocation error\n");

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
                for (int t = D64NUMTRACKS + 1; t <= image_num_tracks(type); t++) {
                    for (int s = 0; s < num_sectors(type, t); s++) {
                        mark_sector(type, image, t, s, 1 /* free */);
                    }
                }
            } else {
                fprintf(stderr, "Wrong filesize: expected to read %d bytes, but read %d bytes\n", imagesize, (int) read_size);

                exit(-1);
            }
        }
        if (set_header) {
            update_directory(type, image, header, id, shadowdirtrack);
        }
    }

    /* Create directory entries */
    fprintf(stdout, "creating dir entries\n");
    create_dir_entries(type, image, files, num_files, dir_sector_interleave, shadowdirtrack);

    /* Write files and mark sectors in BAM */
    write_files(type, image, files, num_files, usedirtrack, dirtracksplit, shadowdirtrack, numdirblocks, dir_sector_interleave);

    if (!quiet) {
        printf("%s (%s,%s):\n", imagepath, header, id);
        print_file_allocation(type, image, files, num_files);

        print_bam(type, image);
    }

    /* Save image */
    if ((strlen(imagepath) >= 4) && !strcmp(imagepath + strlen(imagepath) - 4, ".g64")) {
        generate_uniformat_g64(image, imagepath);
    } else {
        f = fopen(imagepath, "wb");
        if (fwrite(image, imagesize, 1, f) != 1) {
            fprintf(stderr, "ERROR: Failed to write %s\n", image);
            retval = -1;
        }
        fclose(f);
    }

    if (check_hashes(type, image)) {
        fprintf(stderr, "Filename hash collision detected\n");
        retval = -1;
    }

    free(image);

    if (retval != 0) {
        exit(-1);
    }

    return 0;
}
