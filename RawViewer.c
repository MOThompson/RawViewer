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

#undef _POSIX_
	#include <io.h>						/* Must get _findfirst defined */
	#include <process.h>					/* And _beginthread */
#define _POSIX_

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
#include "graph.h"
#include "timer.h"

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
	HWND hdlg;												/* Handle to this dialog box */
	char pathname[PATH_MAX];							/* pathname being displayed */
	BOOL valid;												/* Is the data valid? */
	TL_RAW_FILE_HEADER header;							/* Header information */
	struct {
		SHORT *data;										/* Pointer to the data */
		int width, height;								/* Width / height of sensor */
	} sensor;
	BITMAPINFOHEADER *bmih;								/* current bitmap of the image */
	GRAPH_CURVE *red_hist, *green_hist, *blue_hist;
	int *r_bin, *g_bin, *b_bin;						/* Bin data for pixel counts */
	double r_gain, g_gain, b_gain;					/* RGB color gains */
} VIEWER_INFO;

typedef enum {H_RAW=0, H_RAW_GAIN=1, H_BMP=2} HIST;

typedef struct _RENDER_OPTS {
	BOOL red, green, blue;								/* If TRUE, include in rendering */
	double r_gain, g_gain, b_gain;					/* Gains for each color channel */
	int gain;												/* 2^x gain values (0,1,2,3 allowed) */
	HIST hist;	/* Type of histogram to show */
} RENDER_OPTS;

#define	WMP_OPEN_FILE			(WM_APP+1)		/* Open a file (in WPARAM) */
#define	WMP_SHOW_INFO			(WM_APP+2)		/* Show the header file info */
#define	WMP_RENDER				(WM_APP+3)		/* Create a bitmap and render */
#define	WMP_ENABLE_DIR_WALK	(WM_APP+4)		/* Enable the forward/back arrows */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
int ReadRawFile(char *path, TL_RAW_FILE_HEADER *header, SHORT **data);
int BinData(VIEWER_INFO *viewer, RENDER_OPTS *opts);
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
-- Routine to generate a list of .raw files in the current directory.
-- Will enable scrolling left and right through images once established.
=========================================================================== */
#define DIR_SEARCH_MAGIC	(0x1845)
typedef struct _DIR_SEARCH_INFO {
	int magic;
	char path[PATH_MAX];
	HWND hdlg;
} DIR_SEARCH_INFO;
	
typedef char IMAGES[PATH_MAX];

int image_count=0, image_alloc=0;
int image_active=0;										/* Which images is currently displayed */
IMAGES *images = NULL;

static void DirectoryScanThread(void *args) {
	static char *rname="DirectoryScanThread";

	int i;
	intptr_t hdir;										/* Open directory */
	struct _finddatai64_t findbuf;				/* Information from FindFirst */
	DIR_SEARCH_INFO *info;
	char *path, *aptr, dir[PATH_MAX], fname[PATH_MAX], search[PATH_MAX];

	/* Quick error checks */
	if (args == NULL) return;
	info = (DIR_SEARCH_INFO *) args;
	if (info->magic != DIR_SEARCH_MAGIC) return;
	if (*info->path == '\0') { free(info); return; }
	
	/* If multiple call, release previous results (probably a new directory) */
	if (images != NULL) {
		image_count = image_active = 0;										/* Clear number and current */
		SendMessage(info->hdlg, WMP_ENABLE_DIR_WALK, 0, 0);			/* DIsable for the moment */
	}

	/* Parse the passed argument to find the directory (default ./) and the filename */
	strcpy_s(dir, sizeof(dir), info->path);
	aptr = dir+strlen(dir)-1;						/* Last character */
	while (aptr != dir && strchr("\\/:", *aptr) == NULL) aptr--;
	if (strchr("\\/:", *aptr) != NULL) {									/* Did terminate on a directory marker */
		strcpy_s(fname, sizeof(fname), aptr+1);
	} else {
		strcpy_s(fname, sizeof(fname), aptr);
	}
	*aptr = '\0';										/* End of diretory */
	if (*dir == '\0') strcpy_s(dir, sizeof(dir), ".");
	sprintf_s(search, sizeof(search), "%s/*.raw", dir);

	/* open the directory ... potentially finding no images */
	if ( (hdir = _findfirst64(search, &findbuf)) >= 0) {
		for (i=0; ; i++) {
			/* Find the next one */
			if (i != 0 && _findnext64(hdir, &findbuf) != 0) break;		/* First already loaded */

			/* Make space */
			if (image_count >= image_alloc) {
				image_alloc += 100;
				images = realloc(images, sizeof(*images)*image_alloc);
			}

			/* Set the filename */
			memset(images+image_count, 0, sizeof(*images));
			sprintf_s(images[image_count], sizeof(images[image_count]), "%s\\%s", dir, findbuf.name);
			image_count++;

			/* Is this the one we are currently viewing? */
			if (stricmp(findbuf.name, fname) == 0) image_active = i;
		}
		_findclose(hdir);
		SendMessage(info->hdlg, WMP_ENABLE_DIR_WALK, 0, 1);		/* Enable now */
	}
	free(info);																	/* My responsibility to free when done */

	/* Just for debugging */
//	fprintf(stderr, " path: %s\n dir: %s\n fname: %s\n search: %s\n", info->path, dir, fname, search); fflush(stderr);
//	fprintf(stderr, " icount: %d  ialloc: %d  iactive: %d  file: %s\n", image_count, image_alloc, image_active, images[image_active]);
//	fflush(stderr);

	return;
}

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
		IDV_RED_GAIN, IDV_GREEN_GAIN, IDV_BLUE_GAIN,
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
			viewer->r_gain = 2.45; viewer->g_gain = 1.00; viewer->b_gain = 4.0;

			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR) viewer);
			viewer->hdlg   = hdlg;									/* Have this available for other use */

			/* Set gains and show all channels */
			SetDlgItemCheck(hdlg, IDC_RED, TRUE);
			SetDlgItemCheck(hdlg, IDC_GREEN, TRUE);
			SetDlgItemCheck(hdlg, IDC_BLUE, TRUE);
			SetRadioButton(hdlg, IDR_GAIN_0, IDR_GAIN_5, IDR_GAIN_0);
			EnableDlgItem(hdlg, IDB_SAVE_AS_BITMAP, FALSE);

			/* Set type of Histograms */
			SetRadioButton(hdlg, IDR_HIST_RAW, IDR_HIST_BMP, IDR_HIST_RAW);

			/* Set default color gains (for RGB render) */
			SetDlgItemDouble(hdlg, IDV_RED_GAIN,   "%.2f", viewer->r_gain);
			SendDlgItemMessage(hdlg, IDS_RED_GAIN,    TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->r_gain+0.5));		/* Gains are 0-5 */
			SetDlgItemDouble(hdlg, IDV_GREEN_GAIN, "%.2f", viewer->g_gain);
			SendDlgItemMessage(hdlg, IDS_GREEN_GAIN,  TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->g_gain+0.5));
			SetDlgItemDouble(hdlg, IDV_BLUE_GAIN,  "%.2f", viewer->b_gain);
			SendDlgItemMessage(hdlg, IDS_BLUE_GAIN,   TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->b_gain+0.5));

			InitializeHistogramCurves(hdlg, viewer, 256);
			
#define	TIMER_INITIAL_RENDER				(1)
			SetTimer(hdlg, TIMER_INITIAL_RENDER, 200, NULL);				/* First draw seems to fail */
			rcode = TRUE; break;

		case WMP_RENDER:
			static HIST last_hist = -1;
			opts.gain  = GetRadioButtonIndex(hdlg, IDR_GAIN_0, IDR_GAIN_5);
			opts.red   = GetDlgItemCheck(hdlg, IDC_RED);
			opts.green = GetDlgItemCheck(hdlg, IDC_GREEN);
			opts.blue  = GetDlgItemCheck(hdlg, IDC_BLUE);
			opts.r_gain = viewer->r_gain;
			opts.g_gain = viewer->g_gain;
			opts.b_gain = viewer->b_gain;
			opts.hist = GetRadioButtonIndex(hdlg, IDR_HIST_RAW, IDR_HIST_BMP);
			RenderFrame(viewer, GetDlgItem(hdlg, IDC_IMAGE), &opts);
			BinData(viewer, &opts);													/* Must be after RenderFrame so RGB valid */
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

		case WMP_ENABLE_DIR_WALK:
			EnableDlgItem(hdlg, IDB_NEXT, lParam);
			EnableDlgItem(hdlg, IDB_PREV, lParam);
			rcode = TRUE; break;
			
		case WMP_OPEN_FILE:
			/* Delete current */
			if (viewer->bmih         != NULL) { free(viewer->bmih);         viewer->bmih         = NULL; }
			if (viewer->sensor.data  != NULL) { free(viewer->sensor.data);  viewer->sensor.data  = NULL; }
			EnableDlgItem(hdlg, IDB_SAVE_AS_BITMAP, FALSE);

			/* Copy over the filename in wParam */
			strcpy_m(viewer->pathname, sizeof(viewer->pathname), (char *) wParam);

			/* For first images (or if reset by OpenDialogBox), start a thread to create all .raw files in the directory */
			if (image_count == 0) {
				DIR_SEARCH_INFO *info;
				info = calloc(1, sizeof(*info));
				info->magic = DIR_SEARCH_MAGIC;
				info->hdlg  = hdlg;
				strcpy_s(info->path, sizeof(info->path), viewer->pathname);
				_beginthread(DirectoryScanThread, 0, info);
			}

			/* Read the file and display */
			viewer->valid = ReadRawFile(viewer->pathname, &viewer->header, &viewer->sensor.data) == 0;
			if (viewer->valid) {
				viewer->sensor.height = viewer->header.height;		/* Put these into the header */
				viewer->sensor.width  = viewer->header.width;

				SendMessage(hdlg, WMP_SHOW_INFO, 0, 0);
				SendMessage(hdlg, WMP_RENDER, 0, 0);
				EnableDlgItem(hdlg, IDB_SAVE_AS_BITMAP, TRUE);
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
			if (wParam == TIMER_INITIAL_RENDER) {
				/* Okay ... see if we should pre-load with an image given on the command line */
				if (__argc > 1) SendMessage(hdlg, WMP_OPEN_FILE, (WPARAM) __argv[1], 0);
				KillTimer(hdlg, TIMER_INITIAL_RENDER);
			}
			rcode = TRUE; break;

		case WM_VSCROLL:
			wID = ID_NULL;							/* Determine unerlying wID and set ichan for set below */
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_RED_GAIN))    { wID = IDS_RED_GAIN;    ichan = R_CHAN; }
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_GREEN_GAIN))  { wID = IDS_GREEN_GAIN;	ichan = G_CHAN; }
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_BLUE_GAIN))   { wID = IDS_BLUE_GAIN;	ichan = B_CHAN; }
			if (wID != ID_NULL) {
				static HIRES_TIMER *timer = NULL;
				static double t_last = 0.0;
				int ipos;

				if (timer == NULL) timer = HiResTimerReset(NULL, 0.0);

				ipos = -99999;
				switch (LOWORD(wParam)) {
					case SB_THUMBPOSITION:									/* Moved manually */
					case SB_THUMBTRACK:										/* Limit slider updates to 5 Hz */
						ipos = HIWORD(wParam); break;
					case SB_LINEDOWN:
					case SB_LINEUP:
					case SB_PAGEDOWN:
					case SB_PAGEUP:
					case SB_BOTTOM:
					case SB_TOP:
					default:
						ipos = (int) SendDlgItemMessage(hdlg, wID, TBM_GETPOS, 0, 0); break;
				}
				if (ipos != -99999) {
					if (wID == IDS_RED_GAIN) {
						viewer->r_gain = (100.0-ipos) / 20.0;
						SetDlgItemDouble(hdlg, IDV_RED_GAIN, "%.2f", viewer->r_gain);
					} else if (wID == IDS_GREEN_GAIN) {
						viewer->g_gain = (100.0-ipos) / 20.0;
						SetDlgItemDouble(hdlg, IDV_GREEN_GAIN, "%.2f", viewer->g_gain);
					} else if (wID == IDS_BLUE_GAIN) {
						viewer->b_gain = (100.0-ipos) / 20.0;
						SetDlgItemDouble(hdlg, IDV_BLUE_GAIN, "%.2f", viewer->b_gain);
					}
					SendMessage(hdlg, WMP_RENDER, 0, 0);
				}
			}
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

				/* Controls that just modify the image or histogram */
				case IDC_RED:
				case IDC_GREEN:
				case IDC_BLUE:
				case IDR_GAIN_0:
				case IDR_GAIN_1:
				case IDR_GAIN_2:
				case IDR_GAIN_3:
				case IDR_GAIN_4:
				case IDR_GAIN_5:
				case IDR_HIST_RAW:
				case IDR_HIST_RAW_GAIN:
				case IDR_HIST_BMP:
					SendMessage(hdlg, WMP_RENDER, 0, 0);
					break;

				case IDB_OPEN:
					strcpy_m(pathname, sizeof(pathname), viewer->pathname);	/* Maybe empty, but that's okay for open */
					*pathname = '\0';
					ofn.lStructSize       = sizeof(OPENFILENAME);
					ofn.hwndOwner         = hdlg;
					ofn.lpstrTitle        = "Open RAW image file";
					ofn.lpstrFilter       = "raw (*.raw)\0*.raw\0All files (*.*)\0*.*\0\0";
					ofn.lpstrCustomFilter = NULL;
					ofn.nMaxCustFilter    = 0;
					ofn.nFilterIndex      = 1;
					ofn.lpstrFile         = pathname;				/* Full path */
					ofn.nMaxFile          = sizeof(pathname);
					ofn.lpstrFileTitle    = NULL;						/* Partial path */
					ofn.nMaxFileTitle     = 0;
					ofn.lpstrDefExt       = "raw";
					ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

					/* Query a filename ... if abandoned, just return now with no complaints */
					if (! GetOpenFileName(&ofn)) {
						fprintf(stderr, "GetOpenFileName failed\n"); fflush(stderr);
						break;
					}

					/* Save the directory for the next time */
					strcpy_m(local_dir, sizeof(local_dir), pathname);
					local_dir[ofn.nFileOffset-1] = '\0';							/* Save for next time! */

					image_count = 0;														/* Reset for a new directory search */
					SendMessage(hdlg, WMP_OPEN_FILE, (WPARAM) pathname, 0);
					break;

				case IDB_SAVE_AS_BITMAP:
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
						memset(&ofn, 0, sizeof(ofn));
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
					
				case IDV_RED_GAIN:
					if (EN_KILLFOCUS == wNotifyCode) {
						viewer->r_gain = GetConstrainedDouble(hdlg, wID, TRUE, "%.2f", 0.0, 5.0, 1.0);
						SendDlgItemMessage(hdlg, IDS_RED_GAIN,    TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->r_gain+0.5));		/* Gains are 0-5 */
						SendMessage(hdlg, WMP_RENDER, 0, 0);
					}
					break;
					
				case IDV_GREEN_GAIN:
					if (EN_KILLFOCUS == wNotifyCode) {
						viewer->g_gain = GetConstrainedDouble(hdlg, wID, TRUE, "%.2f", 0.0, 5.0, 1.0);
						SendDlgItemMessage(hdlg, IDS_GREEN_GAIN,  TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->g_gain+0.5));
						SendMessage(hdlg, WMP_RENDER, 0, 0);
					}
					break;

				case IDV_BLUE_GAIN:
					if (EN_KILLFOCUS == wNotifyCode) {
						viewer->b_gain = GetConstrainedDouble(hdlg, wID, TRUE, "%.2f", 0.0, 5.0, 1.0);
						SendDlgItemMessage(hdlg, IDS_BLUE_GAIN,   TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->b_gain+0.5));
						SendMessage(hdlg, WMP_RENDER, 0, 0);
					}
					break;

				/* Set gains to default values or neutral (all 1) */
				case IDB_RGB:
				case IDB_NEUTRAL:
					if (wID == IDB_RGB) {
						viewer->r_gain = 2.45; viewer->g_gain = 1.00; viewer->b_gain = 4.0;			/* Reset defaults */
					} else {
						viewer->r_gain = viewer->g_gain = viewer->b_gain = 1.00;							/* All neutral */
					}
					SetDlgItemDouble(hdlg, IDV_RED_GAIN,   "%.2f", viewer->r_gain);
					SetDlgItemDouble(hdlg, IDV_GREEN_GAIN, "%.2f", viewer->g_gain);
					SetDlgItemDouble(hdlg, IDV_BLUE_GAIN,  "%.2f", viewer->b_gain);
					SendDlgItemMessage(hdlg, IDS_RED_GAIN,    TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->r_gain+0.5));		/* Gains are 0-5 */
					SendDlgItemMessage(hdlg, IDS_GREEN_GAIN,  TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->g_gain+0.5));
					SendDlgItemMessage(hdlg, IDS_BLUE_GAIN,   TBM_SETPOS, TRUE, 100 - (int) (20.0*viewer->b_gain+0.5));
					SendMessage(hdlg, WMP_RENDER, 0, 0);
					rcode = TRUE; break;

				case IDB_NEXT:
					if (images != NULL && image_count > 0) {
						image_active++; if (image_active >= image_count) image_active = 0;
						SendMessage(hdlg, WMP_OPEN_FILE, (WPARAM) images[image_active], 0);
					}
					break;
				case IDB_PREV:
					if (images != NULL && image_count > 0) {
						image_active--; if (image_active < 0) image_active = image_count-1;
						SendMessage(hdlg, WMP_OPEN_FILE, (WPARAM) images[image_active], 0);
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

	/* If this isn't added, get "Access is denied" error when run */
	fprintf(stderr, "WinMain\n"); fflush(stderr);

	/* See how many arguments there are */
	for (int i=0; i<__argc; i++) printf("Arg[%d]: %s\n", i, __argv[i]);
	fflush(stdout);

	/* Load the class for the graph window */
	Graph_StartUp(hThisInstance);					/* Initialize the graphics control */

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
-- Generate the pixel count histograms from the raw data
--
-- Usage: int BinData(VIEWER_INFO *viewer, RENDER_OPTS *opts);
--
-- Inputs: viewer - pointer to structure with an image
--         opts   - options for rendering
--           ->hist - type of hisogram desired
--           ->gain - amount of gain
--
-- Output: Fills in the r_bin, g_bin and b_bin arrays 
--
-- Return: 0 if successful, !0 on error
--         1 --> bad data
--         2 --> invalid value of nbins
=========================================================================== */
int BinData(VIEWER_INFO *viewer, RENDER_OPTS *opts) {

	int i,j, idiv,iadd, width, height;
	int *r_bin, *g_bin, *b_bin;
	int nbins, gain;
	SHORT *raw;

	/* Make sure that we actually have data */
	if (viewer == NULL || ! viewer->valid || viewer->sensor.data == NULL) return 1;
	nbins = viewer->red_hist->npt;
	if (nbins < 16 || nbins > 4096) return 2;

	/* Make space for 255 bins and zero it out */
	r_bin = viewer->r_bin = realloc(viewer->r_bin, nbins*sizeof(*r_bin));	memset(r_bin, 0, nbins*sizeof(*r_bin));
	g_bin = viewer->g_bin = realloc(viewer->g_bin, nbins*sizeof(*g_bin));	memset(g_bin, 0, nbins*sizeof(*g_bin));
	b_bin = viewer->b_bin = realloc(viewer->b_bin, nbins*sizeof(*b_bin));	memset(b_bin, 0, nbins*sizeof(*b_bin));

	if (opts->hist == H_RAW || opts->hist == H_RAW_GAIN) {
		/* Get the parameters */
		width  = viewer->sensor.width;
		height = viewer->sensor.height;
		raw    = viewer->sensor.data;

		/* Deal with gain request if H_RAW_GAIN */
		gain = (opts->hist == H_RAW || opts->gain >= 5) ? 1 : 0x01 << max(0, min(4, opts->gain));

		/* Go through all the data */
		idiv = 4096/nbins;
		iadd = (idiv+1)/2;
		for (i=0; i<height; i+=2) {
			for (j=0; j<width; j+=2) {
				g_bin[ max(0, min(nbins-1, (gain*raw[(i  )*width+j  ]+iadd)/idiv ) ) ]++;
				r_bin[ max(0, min(nbins-1, (gain*raw[(i  )*width+j+1]+iadd)/idiv ) ) ]++;
				b_bin[ max(0, min(nbins-1, (gain*raw[(i+1)*width+j  ]+iadd)/idiv ) ) ]++;
				g_bin[ max(0, min(nbins-1, (gain*raw[(i+1)*width+j+1]+iadd)/idiv ) ) ]++;
			}
		}

		/* Transfer the data to the graphs as well */
		for (i=0; i<nbins; i++) {
			viewer->red_hist->y[i]   = r_bin[i];
			viewer->green_hist->y[i] = g_bin[i] / 2.0;			/* Since 2x as many green pixels */
			viewer->blue_hist->y[i]  = b_bin[i];
		}

	} else {			/* opts->hist == H_RGB */
		BITMAPINFOHEADER *bmih;
		unsigned char *rgb24;

		bmih   = viewer->bmih;
		rgb24  = ((unsigned char *) bmih) + sizeof(*bmih);
		width  = bmih->biWidth;
		height = bmih->biHeight;
		
		/* Go through all the data */
		idiv = 256/nbins;								/* Data only goes to 255 */
		iadd = idiv/2;									/* Don't want to add if 256 */
		for (i=0; i<height*width; i+=3) {
			b_bin[ max(0, min(nbins-1, (rgb24[i+0]+iadd)/idiv)) ]++;
			g_bin[ max(0, min(nbins-1, (rgb24[i+1]+iadd)/idiv)) ]++;
			r_bin[ max(0, min(nbins-1, (rgb24[i+2]+iadd)/idiv)) ]++;
		}

		/* Transfer the data to the graphs as well */
		for (i=0; i<nbins; i++) {
			viewer->red_hist->y[i]   = r_bin[i];
			viewer->green_hist->y[i] = g_bin[i];
			viewer->blue_hist->y[i]  = b_bin[i];
		}
	}

	viewer->red_hist->y[nbins-1] = viewer->green_hist->y[nbins-1] = viewer->blue_hist->y[nbins-1] = 0;
	viewer->red_hist->modified = viewer->green_hist->modified = viewer->blue_hist->modified = TRUE;
	viewer->red_hist->visible  = viewer->green_hist->visible  = viewer->blue_hist->visible  = TRUE;
	SendDlgItemMessage(viewer->hdlg, IDG_HISTOGRAMS, WMP_REDRAW, 0, 0);

#if 0
	{
		FILE *funit;
		funit = fopen("bins.dat", "w");
		for (i=0; i<nbins; i++) fprintf(funit, "%d %d %d\n", r_bin[i], g_bin[i], b_bin[i]);
		fclose(funit);
	}
#endif

	return 0;
}


int InitializeHistogramCurves(HWND hdlg, VIEWER_INFO *viewer, int nbins) {

	int i, npt;
	GRAPH_CURVE *cv;
	GRAPH_SCALES scales;
	GRAPH_AXIS_PARMS parms;

	/* Initialize the curves for histograms */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 0;											/* Main curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "red");
	cv->master        = TRUE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = TRUE;
	cv->npt = nbins;
	cv->x = calloc(sizeof(*cv->x), nbins);
	cv->y = calloc(sizeof(*cv->y), nbins);
	for (i=0; i<nbins; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(255,0,0);
	viewer->red_hist = cv;

	/* Initialize the green histogram */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 1;											/* dark curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "green");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = TRUE;
	cv->npt = nbins;
	cv->x = calloc(sizeof(*cv->x), nbins);
	cv->y = calloc(sizeof(*cv->y), nbins);
	for (i=0; i<nbins; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(0,255,0);
	viewer->green_hist = cv;

	/* Initialize the blue histogram */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 2;											/* blue curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "blue");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = TRUE;
	cv->npt = nbins;
	cv->x = calloc(sizeof(*cv->x), nbins);
	cv->y = calloc(sizeof(*cv->y), nbins);
	for (i=0; i<nbins; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(128,128,255);
	viewer->blue_hist = cv;

	/* Set up the scales ... have to modify Y each time but X is infrequent */
	memset(&scales, 0, sizeof(scales));
	scales.xmin = 0;	scales.xmax = nbins;
	scales.ymin = 0;  scales.ymax = 10000;
	scales.autoscale_x = FALSE; scales.force_scale_x = TRUE;   
	scales.autoscale_y = TRUE;  scales.force_scale_y = FALSE;
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_SCALES, (WPARAM) &scales, (LPARAM) 0);

	/* Turn off the y lines */
	memset(&parms, 0, sizeof(parms));
	parms.suppress_y_grid = parms.suppress_y_ticks = TRUE;
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_AXIS_PARMS, (WPARAM) &parms, (LPARAM) 0);

	/* Clear the graph, and set the force parameters */
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_CLEAR, (WPARAM) 0, (LPARAM) 0);
//	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_X_TITLE, (WPARAM) "counts in pixel", (LPARAM) 0);
//	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_Y_TITLE, (WPARAM) "number", (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_LABEL_VISIBILITY, 0, 0);

	/* Add all the curves now */
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) viewer->red_hist,    (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) viewer->green_hist,  (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) viewer->blue_hist,   (LPARAM) 0);

	return 0;
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
	double r,g,b;												/* Channel gains */
	SHORT *raw;

	/* Have to have data, both a header and real data numbers */
	if (viewer == NULL || ! viewer->valid || viewer->sensor.data == NULL) return NULL;

	/* Get pointer to actual sensor data and size */
	raw    =  viewer->sensor.data;
	width  = viewer->sensor.width;
	height = viewer->sensor.height;		/* RGB will be 1/2 resolution */

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
	r = opts->r_gain; g = opts->g_gain; b = opts->b_gain;
	for (i=0; i<height; i+=2) {
		for (j=0; j<width; j+=2) {
			if (! logmode) {
				*(rgb24++) = ! bchan ? 0 : min(255, (int) (b * ( (raw[(i+1)*width+j] + inorm/2) / inorm) ));			/* Blue channel */
				*(rgb24++) = ! gchan ? 0 : min(255, (int) (g * ( (raw[i*width+j] + raw[(i+1)*width+j+1] + inorm) / 2 / inorm)));	/* Green average */
				*(rgb24++) = ! rchan ? 0 : min(255, (int) (r * ( (raw[i*width+j+1] + inorm/2) / inorm) ));			/* Red channel */
			} else {
				*(rgb24++) = ! bchan ? 0 : (int) (30.658*log(max(1,b*raw[(i+1)*width+j])));								/* Blue channel */
				*(rgb24++) = ! gchan ? 0 : (int) (28.299*log(max(1,g*(raw[i*width+j] + raw[(i+1)*width+j+1]))));	/* Green average */
				*(rgb24++) = ! rchan ? 0 : (int) (30.658*log(max(1,r*raw[i*width+j+1])));									/* Red channel */
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
	if (viewer == NULL || ! viewer->valid || viewer->sensor.data == NULL || ! IsWindow(hwnd)) return 1;

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
