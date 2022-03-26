/* Read an image file and report the header information */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#ifndef NAME_MAX
	#define	NAME_MAX	255
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
	
#include <windows.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "camera.h"
#include "tl.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	panic		SysPanic(__FILE__, __LINE__)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
int ReadRawFile(char *path, TL_RAW_FILE_HEADER *header, SHORT **data);
int ShowInfo(char *path, TL_RAW_FILE_HEADER *header, SHORT *raw);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/*	===========================================================================
=========================================================================== */
int main(int argc, char *argv[]) {

	TL_RAW_FILE_HEADER header;
	SHORT *raw;
	
	argc--; argv++;
	while (argc) {
		if (ReadRawFile(*argv, &header, &raw) == 0) {
			ShowInfo(*argv, &header, raw);
			free(raw);
		}
		argv++; argc--;
	}
	return 0;
}

/* ============================================================================
-- Read a raw format file
--
-- Usage:  int ReadRawFile(char *path, TL_RAW_FILE_HEADER *header, SHORT **data);
--
-- Inputs: path - file to open and scan
--         header - if not NULL, pointer to structure to be filled with header info
--         data   - if not NULL, pointer to variable to receive malloc'd raw data
--
-- Output: *header - if header != NULL, filled in structure
--         *data   - if data != NULL, pointer to malloc'd data ... caller must free()
--
-- Returns:  0 if successful, !0 on errors
============================================================================ */
int ReadRawFile(char *path, TL_RAW_FILE_HEADER *pheader, SHORT **data) {
	static char *rname = "ReadRawFile";

	FILE *funit;
	TL_RAW_FILE_HEADER header;
	int rc;
	short *raw;

	/* Default values in case of file read failure */
	if (pheader != NULL) memset(pheader, 0, sizeof(*pheader));
	if (data    != NULL) *data = NULL;
	
	rc = 0;									/* Assume success */

	/* Open the file, read header, validate, read data (maybe) */
	if ( (fopen_s(&funit, path, "rb")) != 0) {
		fprintf(stderr, "[%s] Unable to open file (%s) for binary read\n", rname, path); fflush(stderr);
		funit = NULL;
		rc = 1;
	} else if (fread(&header, sizeof(header), 1, funit) != 1) {
		fprintf(stderr, "[%s] Unable to read raw image header structure from file\n", rname); fflush(stderr);
		rc = 2;
	} else if (header.magic != TL_RAW_FILE_MAGIC) {
		fprintf(stderr, "[%s] Header magic key (0x%8.8x) does not match expected value (0x%8.8x)\n", rname, header.magic, TL_RAW_FILE_MAGIC); fflush(stderr);
		rc = 3;
	} else if (header.header_size != sizeof(header)) {
		fprintf(stderr, "[%s] Listed header size (%d) does not match expected value (%d)\n", rname, header.header_size, sizeof(header)); fflush(stderr);
		rc = 4;
	} else if (header.major_version != 1 || header.minor_version != 0) {
		fprintf(stderr, "[%s] Header's version (%d.%d) is not 1.0 as this code expects\n", rname, header.major_version, header.minor_version); fflush(stderr);
		rc = 5;

	/* Successful to this point */
	} else {
		/* If caller wants the header information, provide it now */
		if (pheader != NULL) *pheader = header;

		/* If data not NULL, read actual pixel data -- only valid for 12-bit cameras for now */
		if (data != NULL) {
			int nx,ny, npt;
			nx = header.width; ny = header.height;
			raw = malloc(nx*ny*sizeof(*raw));
			if ( (npt = fread(raw, sizeof(*raw), nx*ny, funit)) != nx*ny) {
				fprintf(stderr, "[%s] Failed to read all expected pixels (saw %d but expect %d)\n", rname, npt, nx*ny); fflush(stderr);
				free(raw);
				rc = 6;
			} else {
				*data = raw;
			}
		}
	}

	/* Close the file */
	if (funit != NULL) fclose(funit);
	return rc;
}


/* ============================================================================
-- Show info of a .raw file
--
-- Usage:  int ShowInfo(char *path, );
--
-- Inputs: path - file to open and scan
--
-- Output: stdout
--
-- Returns:  0 if successful, !0 on errors
============================================================================ */
int ShowInfo(char *path, TL_RAW_FILE_HEADER *header, SHORT *raw) {
	static char *rname = "ShowInfo";

	int i, nx, ny;
	SHORT zmin, zmax;
	
	nx = header->width; ny = header->height;

	/* Scan for peak and minimum intensity */
	zmin = zmax = raw[0];
	for (i=0; i<nx*ny; i++) { 
		if (raw[i] < zmin) {
			zmin = raw[i];
		} else if (raw[i] > zmax) {
			zmax = raw[i];
		}
	}

	/* Output the information */
	printf("File: %s\n", path);
   printf(" Timestamp: %4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d.%3.3d  (camera time: %f)\n", header->year, header->month, header->day, header->hour, header->min, header->sec, header->ms, header->camera_time);
	printf(" Camera: %s           S/N: %s\n", header->camera_model, header->camera_serial);
	printf(" Image size: %4d x %4d   Bit depth: %2d   Pixel size (um): %.2f x %.2f\n", header->width, header->height, header->bit_depth, header->pixel_width, header->pixel_height);
   printf(" Exposure (ms): %6.2f     gain (dB): %2.f\n", header->ms_expose, header->dB_gain);
	printf(" Minimum counts: %4d      maximum counts: %4d\n", zmin, zmax);

	fflush(NULL);

	return 0;
}
