# README #

This is cc1541 v2.1, a tool for creating Commodore 1541 Floppy disk images in D64, G64, D71 or D81 format with custom sector interleaving etc. Also supports extended tracks 35-40 using either SPEED DOS or DOLPHIN DOS BAM-formatting.

Originally written by JackAsser, with improvements and fixes by Krill, some more improvements by Claus and further improvements by Bjoern Esser.

The file test_cc1541.c contains a simple test suite that will call the cc1541 binary with different commandlines and examine the resulting image file.

The program is provided under the terms of the MIT license, please refer to the included LICENSE.txt file for its terms and conditions. The public source code repository can be found here:
https://bitbucket.org/PTV_Claus/cc1541/src/master/

## Version history ##

v2.1

* Support for D81 images
* Support for hash calculation for latest Krill loader
* ...

v2.0

* The first version with a release number
* All existing modifications consolidated (hopefully)
* G64 output dependent on output file name instead of a souce code define
* Converted to ANSI C99
* MSVC build files added
* getopt removed
* simple test suite added
* bugfix: hex escape was not considered for file overwrite detection
* bugfix: first sector per track was ignored for track 1
* bugfix: default sector interleave was ignored for first file
