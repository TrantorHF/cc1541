# README #

This is cc1541 v3.0, a tool for creating Commodore 1541 Floppy disk
images in D64, G64, D71 or D81 format with custom sector interleaving
etc.  Also supports extended tracks 35-40 using either SPEED DOS or
DOLPHIN DOS BAM-formatting.

Originally written by JackAsser, with improvements and fixes by
Krill, some more improvements by Claus and further improvements by
Bjoern Esser.

The file test_cc1541.c contains a simple test suite that will call
the cc1541 binary with different commandlines and examine the
resulting image file.

The program is provided under the terms of the MIT license, please
refer to the included LICENSE.txt file for its terms and conditions.

The public source code repository can be found here:
https://bitbucket.org/PTV_Claus/cc1541/src/master/

## Usage examples ##

* "cc1541 image.d64" lists the content of image.d64
* "cc1541 -f program -w program.prg image.d64" adds the file
  program.prg to image.d64 using the name "program"
* "cc1541 -f program1 -w program1.prg -f program2 -w program2.prg
  image.d64" adds two files under the names program1 and program2
* "cc1541 -s 4 -f program -w program.prg image.d64" writes file
  with a dedicated sector interleave for a fastloader (the best
  value depends on the used fastloader and its configuration)

## Version history ##

v3.0

* ASCII to PETSCII conversion added, this breaks backward
  compatibility (and therefore warrants a major version increase)!
* Support for D81 images
* Default printout is now a full directory similar to how it would be
  displayed on a Commodore machine
* -v switch added for verbose output of file and block allocation
* -M switch added to perform hash calculation and collision check for
  latest Krill loader
* -B switch added to allow setting the displayed file size
* -o switch added to prevent overwriting of existing files on an image
* -V switch added to validate images before editing them
* -T switch added to allow setting the file type
* -O switch added to allow setting the open flag
* -P switch added to allow setting the protected flag
* Hex escapes are now also allowed for diskname and ID
* When no disk file name is provided, only the base name of the input
  file is used as disk file name instead of the full path
* Bugfix: fixed memory access issue for GCC 8 and -O2
* Bugfix: G64 output is now an optional additional output using -g,
  avoiding the utterly broken reading of G64 files
* Bugfix: loop files have actual file size per default instead of 0
* Bugfix: printouts to stderr and stdout are more consistent now

v2.0

* The first version with a release number
* All existing modifications consolidated (hopefully)
* G64 output dependent on output file name instead of a souce code
  define
* Converted to ANSI C99
* MSVC build files added
* getopt removed
* Simple test suite added
* Bugfix: hex escape was not considered for file overwrite detection
* Bugfix: first sector per track was ignored for track 1
* Bugfix: default sector interleave was ignored for first file
