/* Raw image viewer */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE					/* Always require POSIX standard */

#define NEED_WINDOWS_LIBRARY			/* Define to include windows.h call functions */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>						/* for defining several useful types and macros */
#include <stdlib.h>						/* for performing a variety of operations */
#include <stdio.h>
#include <string.h>						/* for manipulating several kinds of strings */
#include <time.h>
#include <direct.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>						 /* C99 extension to get known width integers */

/* Standard Windows libraries */
#ifdef NEED_WINDOWS_LIBRARY
#define STRICT							/* define before including windows.h for stricter type checking */
	#include <windows.h>					/* master include file for Windows applications */
	#include <windowsx.h>				/* Extensions for GET_X_LPARAM */
	#include <commctrl.h>
	#include <wingdi.h>					/* Bitmap headers */
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "resource.h"
#include "win32ex.h"

/* Load camera routines */
#include "camera.h"							/* Call either DCX or TL versions */
#include "tl.h"								/* TL  API camera routines & info */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	LinkDate	(__DATE__ "  "  __TIME__)

#ifndef PATH_MAX
	#define	PATH_MAX	(260)
#endif

typedef struct _VIEWER_INFO {
	HWND hdlg;									/* Handle to this dialog box */
	char pathname[PATH_MAX];				/* pathname being displayed */
	BOOL valid;									/* Is the data valid? */
	TL_RAW_FILE_HEADER header;				/* Header information */
	SHORT *data;								/* Pointer to the data */
	BITMAPINFOHEADER *bmih;					/* current bitmap of the image */
} VIEWER_INFO;

typedef struct _RENDER_OPTS {
	BOOL red, green, blue;					/* If TRUE, include in rendering */
	int gain;									/* 2^x gain values (0,1,2,3 allowed) */
} RENDER_OPTS;

#define	WMP_OPEN_FILE	(WM_APP+1)		/* Open a file (in WPARAM) */
#define	WMP_SHOW_INFO	(WM_APP+2)		/* Show the header file info */
#define	WMP_RENDER		(WM_APP+3)		/* Create a bitmap and render */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
int ReadRawFile(char *path, TL_RAW_FILE_HEADER *header, SHORT **data);
int RenderFrame(VIEWER_INFO *viewer, HWND hwnd, RENDER_OPTS *opts);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of  global vars		  */
/* ------------------------------- */
HINSTANCE hInstance;
VIEWER_INFO *viewer = NULL;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
	
/* ===========================================================================
=========================================================================== */
#define	ID_NULL		(-1)

BOOL CALLBACK ViewerDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "ViewerDlgProc";

	BOOL rcode, bFlag;
	int i, ival, ineed, nfree, ichan, rc, mode;

	int wID, wNotifyCode;
	char szBuf[256];

	static char local_dir[PATH_MAX]="";					/* Directory -- keep for multiple calls */
	char pathname[PATH_MAX];
	OPENFILENAME ofn;

	/* List of controls which will respond to <ENTER> with a WM_NEXTDLGCTL message */
	/* Make <ENTER> equivalent to losing focus */
	static int DfltEnterList[] = {					
		ID_NULL };

	VIEWER_INFO *viewer;
	RENDER_OPTS opts;
	HWND hwndTest;
	int *hptr;

/* Recover the information data associated with this window */
	if (msg != WM_INITDIALOG) {
		viewer = (VIEWER_INFO *) GetWindowLongPtr(hdlg, GWLP_USERDATA);
	}

/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			fprintf(stderr, "Initializing viewer interface\n"); fflush(stderr);
			DlgCenterWindow(hdlg);

			/* Since may not actually be the call, look up this applications instance */
			hInstance = (HINSTANCE) GetWindowLongPtr(hdlg, GWLP_HINSTANCE);

			sprintf_s(szBuf, sizeof(szBuf), "Version 2.0 [ %s ]", LinkDate);
//			SetDlgItemText(hdlg, IDT_COMPILE_VERSION, szBuf);

			/* Create the information block and save it within this dialog.  Initialize critical parameters */
			viewer = (VIEWER_INFO *) calloc(1, sizeof(VIEWER_INFO));

			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR) viewer);
			viewer->hdlg   = hdlg;									/* Have this available for other use */

			SetDlgItemCheck(hdlg, IDC_RED, TRUE);
			SetDlgItemCheck(hdlg, IDC_GREEN, TRUE);
			SetDlgItemCheck(hdlg, IDC_BLUE, TRUE);
			SetRadioButton(hdlg, IDR_GAIN_0, IDR_GAIN_5, IDR_GAIN_0);
			EnableDlgItem(hdlg, IDB_SAVE, FALSE);

			/* Okay ... see if we should pre-load with an image given on the command line */
			if (__argc > 1) PostMessage(hdlg, WMP_OPEN_FILE, (WPARAM) __argv[1], 0);

#define	TIMER_INITIAL_RENDER				(1)
			SetTimer(hdlg, TIMER_INITIAL_RENDER, 100, NULL);				/* First draw seems to fail */
			rcode = TRUE; break;

		case WMP_RENDER:
			opts.gain  = GetRadioButtonIndex(hdlg, IDR_GAIN_0, IDR_GAIN_5);
			opts.red   = GetDlgItemCheck(hdlg, IDC_RED);
			opts.green = GetDlgItemCheck(hdlg, IDC_GREEN);
			opts.blue  = GetDlgItemCheck(hdlg, IDC_BLUE);
			RenderFrame(viewer, GetDlgItem(hdlg, IDC_IMAGE), &opts);
			rcode = TRUE; break;
			
		case WMP_SHOW_INFO:
			if (viewer->valid) {
				TL_RAW_FILE_HEADER *header;				/* Header information */

				SetWindowText(hdlg, viewer->pathname);

				header = &viewer->header;
				sprintf_s(szBuf, sizeof(szBuf), "%4.4d.%2.2d.%2.2d\r\n%2.2d:%2.2d:%2.2d.%3.3d", header->year, header->month, header->day, header->hour, header->min, header->sec, header->ms);
				SetDlgItemText(hdlg, IDT_TIMESTAMP, szBuf);
				sprintf_s(szBuf, sizeof(szBuf), "%f", header->camera_time);
				SetDlgItemText(hdlg, IDT_CAMERA_TIME, szBuf);
				sprintf_s(szBuf, sizeof(szBuf), "%s (%s)", header->camera_model, header->camera_serial);
				SetDlgItemText(hdlg, IDT_CAMERA, szBuf);
				sprintf_s(szBuf, sizeof(szBuf), "%4d x %4d", header->width, header->height);
				SetDlgItemText(hdlg, IDT_SENSOR_SIZE, szBuf);
				sprintf_s(szBuf, sizeof(szBuf), "%.2f", header->ms_expose);
				SetDlgItemText(hdlg, IDT_EXPOSURE, szBuf);
				sprintf_s(szBuf, sizeof(szBuf), "%.2f", header->dB_gain);
				SetDlgItemText(hdlg, IDT_GAIN, szBuf);
			} else {
				SetDlgItemText(hdlg, IDT_TIMESTAMP, "<invalid>");
				SetDlgItemText(hdlg, IDT_CAMERA_TIME, "<invalid>");
				SetDlgItemText(hdlg, IDT_CAMERA, "<invalid>");
				SetDlgItemText(hdlg, IDT_SENSOR_SIZE, "<invalid>");
				SetDlgItemText(hdlg, IDT_GAIN, "<invalid>");
			}
			rcode = TRUE; break;

		case WMP_OPEN_FILE:
			/* Delete current */
			if (viewer->bmih != NULL) { free(viewer->bmih); viewer->bmih = NULL; }
			if (viewer->data != NULL) { free(viewer->data); viewer->data = NULL; }
			EnableDlgItem(hdlg, IDB_SAVE, FALSE);

			/* Copy over the filename in wParam */
			strcpy_m(viewer->pathname, sizeof(viewer->pathname), (char *) wParam);
			viewer->valid = ReadRawFile(viewer->pathname, &viewer->header, &viewer->data) == 0;
			if (viewer->valid) {
				SendMessage(hdlg, WMP_SHOW_INFO, 0, 0);
				SendMessage(hdlg, WMP_RENDER, 0, 0);
				EnableDlgItem(hdlg, IDB_SAVE, TRUE);
			}
			break;

		case WM_CLOSE:
			if (viewer != NULL) {									/* Free associated memory */
				viewer->hdlg = NULL;
				free(viewer);
			}
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_TIMER:
			if (wParam == TIMER_INITIAL_RENDER) { SendMessage(hdlg, WMP_RENDER, 0, 0); KillTimer(hdlg, TIMER_INITIAL_RENDER); }
			rcode = TRUE; break;

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			rcode = FALSE;												/* Assume we don't process */
			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
					hwndTest = GetFocus();							/* Just see if we use to change focus */
					for (hptr=DfltEnterList; *hptr!=ID_NULL; hptr++) {
						if (GetDlgItem(hdlg, *hptr) == hwndTest) {
							PostMessage(hdlg, WM_NEXTDLGCTL, 0, 0L);
							break;
						}
					}
					rcode = TRUE; break;

				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				case IDC_RED:
				case IDC_GREEN:
				case IDC_BLUE:
				case IDR_GAIN_0:
				case IDR_GAIN_1:
				case IDR_GAIN_2:
				case IDR_GAIN_3:
				case IDR_GAIN_4:
				case IDR_GAIN_5:
					SendMessage(hdlg, WMP_RENDER, 0, 0);
					break;

				case IDB_OPEN:
					strcpy_m(pathname, sizeof(pathname), viewer->pathname);	/* Maybe empty, but that's okay for open */
					ofn.lStructSize       = sizeof(OPENFILENAME);
					ofn.hwndOwner         = hdlg;
					ofn.lpstrTitle        = "Open RAW image file";
					ofn.lpstrFilter       = "raw (*.raw)\0*.raw\0All files (*.*)\0*.*\0\0";
					ofn.lpstrCustomFilter = NULL;
					ofn.nMaxCustFilter    = 0;
					ofn.nFilterIndex      = 1;
					ofn.lpstrFile         = pathname;		/* Full path */
					ofn.nMaxFile          = sizeof(pathname);
					ofn.lpstrFileTitle    = NULL;						/* Partial path */
					ofn.nMaxFileTitle     = 0;
					ofn.lpstrDefExt       = "raw";
					ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

					/* Query a filename ... if abandoned, just return now with no complaints */
					if (! GetOpenFileName(&ofn)) break;

					/* Save the directory for the next time */
					strcpy_m(local_dir, sizeof(local_dir), pathname);
					local_dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */

					SendMessage(hdlg, WMP_OPEN_FILE, (WPARAM) pathname, 0);
					break;

				case IDB_SAVE:
					if (! viewer->valid || viewer->bmih == NULL) { 
						Beep(300, 200); break;
					} else {
						int isize;
						BITMAPINFOHEADER *bmih;
						BITMAPFILEHEADER bmfh;
						FILE *funit;
						
						strcpy_m(pathname, sizeof(pathname), viewer->pathname);		/* Default name must be initialized with something */
						if (strlen(pathname) > 4) {											/* Try to strip the .raw extension */
							if (_stricmp(pathname+strlen(pathname)-4, ".raw") == 0) pathname[strlen(pathname)-4] = '\0';
						}
						ofn.lStructSize       = sizeof(ofn);
						ofn.hwndOwner         = hdlg;
						ofn.lpstrTitle        = "Save as BMP image";
						ofn.lpstrFilter       = "bmp (*.bmp)\0*.bmp\0All files (*.*)\0*.*\0\0";
						ofn.lpstrCustomFilter = NULL;
						ofn.nMaxCustFilter    = 0;
						ofn.nFilterIndex      = 1;
						ofn.lpstrFile         = pathname;				/* Full path */
						ofn.nMaxFile          = sizeof(pathname);
						ofn.lpstrFileTitle    = NULL;						/* Partial path */
						ofn.nMaxFileTitle     = 0;
						ofn.lpstrDefExt       = "bmp";
						ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
						ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

						/* Get filename and maybe abort */
						if (! GetSaveFileName(&ofn) != 0) break;		/* If aborted, just skip and go back to re-enabling the image */

						/* Save the directory for the next time */
						strcpy_m(local_dir, sizeof(local_dir), pathname);
						local_dir[ofn.nFileOffset-1] = '\0';						/* Save for next time! */

						/* Full size of the bitmap header and the data */
						bmih = viewer->bmih;
						isize = sizeof(*bmih)+3*bmih->biWidth*bmih->biHeight;

						/* Create the file header */
						memset(&bmfh, 0, sizeof(bmfh));	
						bmfh.bfType = 19778;
						bmfh.bfSize = sizeof(bmfh)+isize;
						bmfh.bfOffBits = sizeof(bmfh)+sizeof(*bmih);

						if ( (fopen_s(&funit, pathname, "wb")) != 0) {
							fprintf(stderr, "[%s] Failed to open \"%s\"\n", rname, pathname); fflush(stderr);
						} else {
							fwrite(&bmfh, 1, sizeof(bmfh), funit);
							fwrite(bmih, 1, isize, funit);
							fclose(funit);
						}
					}
					break;
					
				/* Intentionally unused IDs */
				case IDC_IMAGE:
				case IDT_TIMESTAMP:
				case IDT_CAMERA_TIME:
				case IDT_CAMERA:
				case IDT_SENSOR_SIZE:
				case IDT_EXPOSURE:
				case IDT_GAIN:
					break;

				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return rcode;
}

/* ===========================================================================
=========================================================================== */

int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	/* If not done, make sure we are loaded.  Assume safe to call multiply */
	InitCommonControls();
	LoadLibrary("RICHED20.DLL");

	/* And show the dialog box */
	hInstance = hThisInstance;
	DialogBox(hInstance, "VIEWER_DIALOG", HWND_DESKTOP, (DLGPROC) ViewerDlgProc);

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


/* ===========================================================================
-- Generate a device independent bitmap (DIB) from image and options
-- 
-- Usage: BITMAPINFOHEADER *CreateDIB(VIEWER_INFO *viewer, RENDER_OPTS *opts);
--
-- Inputs: viewer - structure with current image
--         opts   - if not NULL, options for modifying image generation
--
-- Output: if rc != NULL, *rc has error code (or 0 if successful)
--				 0 ==> successful
--           1 ==> camera pointer not valid
--           2 ==> path invalid
--           3 ==> no RGB24 image data in camera structure
--           4 ==> unable to allocate memory for new RGB data
--           5 ==> unable to get the mutex semaphore
--
-- Return: pointer to bitmap or NULL on any error
=========================================================================== */
BITMAPINFOHEADER *CreateDIB(VIEWER_INFO *viewer, RENDER_OPTS *opts) {
	static char *rname = "CreateDIB";

	BITMAPINFOHEADER *bmih;
	unsigned char *data;
	int width, height, i, j, ineed, inorm;
	int my_rc;
	unsigned char *rgb24;
	BOOL rchan, gchan, bchan, logmode;
	TL_RAW_FILE_HEADER *header;
	SHORT *raw;

	/* Have to have data, both a header and real data numbers */
	if (viewer == NULL || ! viewer->valid || viewer->data == NULL) return NULL;

	/* Figure out the size */
	header = &viewer->header;
	raw    =  viewer->data;
	width = header->width; height = header->height;					/* RGB is 1/2 resolution */

	/* Allocate the structure */
	ineed = sizeof(*bmih)+3*width/2*height/2;
	if ( (bmih = calloc(1, ineed)) == NULL) return NULL;
	rgb24 = ((unsigned char *) bmih) + sizeof(*bmih);

	logmode = FALSE;							/* Default behaviors */
	inorm = 16;									/* Divisor to normalize to 0-255 from 12 bit depth */
	rchan = gchan = bchan = TRUE;			/* Show all channels */
	if (opts != NULL) {
		if (opts->gain >= 5) {
			logmode = TRUE;
		} else {
			inorm = 16 >> max(0, min(4, opts->gain));
		}
		rchan = opts->red; gchan = opts->green; bchan = opts->blue;
	}

	/* Convert to true RGB format */
	for (i=0; i<height; i+=2) {
		for (j=0; j<width; j+=2) {
			if (! logmode) {
				*(rgb24++) = ! bchan ? 0 : min(255, ( raw[(i+1)*width+j] + inorm/2) / inorm);								/* Blue channel */
				*(rgb24++) = ! gchan ? 0 : min(255, (raw[i*width+j] + raw[(i+1)*width+1] + inorm) / 2 / inorm);		/* Green average */
				*(rgb24++) = ! rchan ? 0 : min(255, ( raw[i*width+j+1] + inorm/2) / inorm);								/* Red channel */
			} else {
				*(rgb24++) = ! bchan ? 0 : (int) (30.658*log(max(1,raw[(i+1)*width+j])) + 0.5);							/* Blue channel */
				*(rgb24++) = ! gchan ? 0 : (int) (28.299*log(max(1,raw[i*width+j] + raw[(i+1)*width+1])) + 0.5);	/* Green average */
				*(rgb24++) = ! rchan ? 0 : (int) (30.658*log(max(1,raw[i*width+j+1])) + 0.5);								/* Red channel */
			}
		}
	}

	/* Image is now 1/2 the resolution */
	width /= 2; height /= 2;

	/* Fill in the bitmap information */
	bmih->biSize = sizeof(*bmih);					/* Only size of the header itself */
	bmih->biWidth         = width;
	bmih->biHeight        = height;				/* Make the image upright when saved */
	bmih->biPlanes        = 1;
	bmih->biBitCount      = 24;					/* Value for RGB24 color images */
	bmih->biCompression   = BI_RGB;
	bmih->biSizeImage     = width*height;
	bmih->biXPelsPerMeter = 3780;					/* Just make it 96 ppi (doesn't matter) */
	bmih->biYPelsPerMeter = 3780;					/* Same value ThorCam reports */
	bmih->biClrUsed       = 0;
	bmih->biClrImportant  = 0;

	return bmih;
}

/* ===========================================================================
-- Render an image in a specified window
-- 
-- Usage: int RenderFrame(VIEWER_INFO *viewer, HWND hwnd, RENDER_OPTS *opts);
--
-- Inputs: viewer info with image data
--         hwnd   - window to render the bitmap to
--         opts   - if not NULL, pointer to options to use when rendering
--
-- Output: converts image to RGB, generates bitmap, and displays in window
--
-- Return: 0 if successful, otherwise an error code
--           1 ==> camera pointer not valid
--           2 ==> path invalid
--           3 ==> no RGB24 image data in camera structure
--           4 ==> unable to allocate memory for new RGB data
--           5 ==> unable to get the mutex semaphore
=========================================================================== */
int RenderFrame(VIEWER_INFO *viewer, HWND hwnd, RENDER_OPTS *opts) {
	static char *rname = "RenderFrame";

	HDC hdc;
	BITMAPINFOHEADER *bmih;
	HBITMAP hBitmap;
	HDC       hDCBits;
	BITMAP    Bitmap;
	BOOL      bResult;
	RECT		 Client;

	/* Make sure we have data to show and somewhere to show it */
	if (viewer == NULL || ! viewer->valid || viewer->data == NULL || ! IsWindow(hwnd)) return 1;

	/* Free previous bitmap if in the structure */
	if (viewer->bmih != NULL) { free(viewer->bmih); viewer->bmih = NULL; }

	/* Get the bitmap to render (will deal with processing to RGB and all semaphores) */
	if ( NULL == (bmih = CreateDIB(viewer, opts))) return 2;					/* Unable to create the bitmap */

	hdc = GetDC(hwnd);				/* Get DC */
	SetStretchBltMode(hdc, COLORONCOLOR);

	hBitmap = CreateDIBitmap(hdc, bmih, CBM_INIT, (LPSTR) bmih + bmih->biSize, (BITMAPINFO *) bmih, DIB_RGB_COLORS);
	GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&Bitmap);

	hDCBits = CreateCompatibleDC(hdc);
	SelectObject(hDCBits, hBitmap);

	GetClientRect(hwnd, &Client);
//	bResult = BitBlt(hdc, 0, 0, Bitmap.bmWidth, Bitmap.bmHeight, hDCBits, 0, 0, SRCCOPY);
	bResult = StretchBlt(hdc, 0,0, Client.right, Client.bottom, hDCBits, 0, 0, bmih->biWidth, bmih->biHeight, SRCCOPY);

	DeleteDC(hDCBits);				/* Delete the temporary DC for the bitmap */
	DeleteObject(hBitmap);			/* Delete the bitmap object itself */

	ReleaseDC(hwnd, hdc);			/* Release the main DC */
	
	viewer->bmih = bmih;				/* Save so can be released next time */
	return 0;
}
