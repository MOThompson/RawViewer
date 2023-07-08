/* Graph window procedure */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* To use the double buffering with a memory device context, define USE_MEMORY_DC and set ERASE... to FALSE */
#define	USE_MEMORY_DC
#define	ERASE_BACKGROUND_ON_INVALIDATE	(FALSE)

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#undef _POSIX_
#include <process.h>			  /* for process control fuctions (e.g. threads, programs) */
#define _POSIX_

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "win32ex.h"
#include "graph.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
#endif
// #define	DEBUG

#define	nint(x)	(((x)>0) ? ( (int) (x+0.5)) : ( (int) (x-0.5)) )

/* Structure to help with labeling of tick marks on graphs */
typedef struct _LABEL_FORMAT {
	BOOL sci;											/* Is scientific notation required */
	int iexp;											/* Power of 10 for encoding -- factr = 10^iexp */
	double scale;										/* Factor to divide values before encoding */
	int mx;												/* Labeling mode */
	char format[10];									/* Format for encoding the value */
} LABEL_FORMAT;

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
LRESULT CALLBACK GraphWndProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam);

static void UpdateCurveMinMax(GRAPH_CURVE *cv);
static void	UpdateMeshMinMax(GRAPH_MESH *mesh);

static BOOL OutOfRange(double x, double y, double xmin, double xmax, double ymin, double ymax);
static void PlotAutoScaleZ(double fmin, double fmax, double *SMIN, double *SMAX, double *DX, double *DX2, int *M, double zforce);
static void ArrayMinMax(double *x, int npt, double *xmin, double *xmax);
static void ArrayLogMinMax(double *x, int npt, double *pxmin, double *pxmax);

static int FormatLabels(LABEL_FORMAT *lab, double rmin, double rmax, double dx);
static char *EncodeLabel(double x, LABEL_FORMAT *lab, char *result, size_t len);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- Called during program initialization to enable registering for classes,
-- creation of semaphores, etc.  Maybe called multiple times, it is
-- responsibility of routine to remember and ignore multiple calls.
--
-- Usage: int Graph_StartUp(HINSTANCE hInst);
--
-- Inputs: hInst - current instance
--
-- Output: Initializes any classes and starts threads as needed
--
-- Return: 0 if successful, !0 otherwise
=========================================================================== */
int Graph_StartUp(HINSTANCE hInst) {

	WNDCLASS wc; 
	static BOOL first=TRUE;

	if (first) {																/* Only once */
		first = TRUE;
		
		/* Create my special classes */
		wc.style = CS_HREDRAW | CS_VREDRAW;									/* Timeline graphs class */
		wc.lpfnWndProc = (WNDPROC) GraphWndProc; 
		wc.cbClsExtra = 0; 
		wc.cbWndExtra = sizeof(PVOID);										/* Pointer to private data which is accessed via SetWindowLongPtr */
		wc.hInstance = hInst; 
		wc.hIcon = NULL;
		wc.hCursor = NULL;
		wc.hbrBackground = GetStockObject(BLACK_BRUSH); 
		wc.lpszMenuName =  NULL;
		wc.lpszClassName = "GraphClass";
		if (! RegisterClass(&wc)) {
			fprintf(stderr, "ERROR: Unable to register the graph class window procedure\n");
			fflush(stderr);
			return 1;
		}
	}
	return 0;
}

/* ===========================================================================
-- Check to see if we are to release memory associated with a curve, and do so
--
-- Usage: void free_curve(GRAPH_CURVE *cv);
--        void free_function(GRAPH_FNC *fnc);
--
-- Inputs: cv - pointer to a curve structure or a function structure
--
-- Output: if cv->free_on_clear is set, release cv and associated memory
--
-- Return: none
=========================================================================== */
static void free_curve(GRAPH_CURVE *cv) {
	if (cv->free_on_clear) {					/* Should I release the memory */
		if (cv->x != NULL) free(cv->x);
		if (cv->y != NULL) free(cv->y);
		if (cv->z != NULL) free(cv->z);
		if (cv->s != NULL) free(cv->s);
		if (cv->pt_rgb != NULL) free(cv->pt_rgb);
		free(cv);
	}
	return;
}

static void free_mesh(GRAPH_MESH *mesh) {
	if (mesh->free_on_clear) {
		free(mesh->t);
	}
	return;
}

static void free_function(GRAPH_FNC *fnc) {
	if (fnc->free_on_clear) {					/* Should I release the memory */
		free(fnc);
	}
	return;
}

/* ===========================================================================
-- Routines to convert a given x/y with graph min/max to pixels in range [0,imax]
--
-- Usage: int get_ix(double x, double xmin, double xmax, GRAPH_DATA *graph);
--        int get_iy(double y, double ymin, double ymax, GRAPH_DATA *graph);
--        double get_x(int ix, double xmin, double xmax, GRAPH_DATA *graph);
--        double get_y(int iy, double ymin, double ymax, GRAPH_DATA *graph);
--
-- Inputs: x,y - real value to plot
--         ix,iy - screen locations (bounded [0,cxClient], [1,cyClient])
--         xmin,xmax - min/max desired for the graph
--         graph->cxClient - limits to the screen area in x
--         graph->cyClient - limits to the screen area in y
--			  graph->...margin..
=========================================================================== */
static int get_ix(double x, double xmin, double xmax, GRAPH_DATA *graph) {
	x = (x-xmin)/(xmax-xmin);
	if (x < 0) x = 0;
	if (x > 1) x = 1;
	return (int) (graph->x_left_margin + (graph->cxClient-graph->x_left_margin-graph->x_right_margin)*x+0.5);
}
static int get_iy(double y, double ymin, double ymax, GRAPH_DATA *graph) {
	y = (y-ymin)/(ymax-ymin);
	if (y < 0) y = 0;
	if (y > 1) y = 1;
	return (int) (graph->y_right_margin+(graph->cyClient-graph->y_left_margin-graph->y_right_margin)*(1.0-y)+0.5);
}

static double get_x(int ix, double xmin, double xmax, GRAPH_DATA *graph) {
	double x;
	if (ix < 0) ix = 0;  if (ix > graph->cxClient) ix = graph->cxClient;
	x = (1.0*ix-graph->x_left_margin) / max(1,graph->cxClient-graph->x_left_margin-graph->x_right_margin);
	return xmin+x*(xmax-xmin);

}
static double get_y(int iy, double ymin, double ymax, GRAPH_DATA *graph) {
	double y;
	if (iy < 0) iy = 0;  if (iy > graph->cyClient) iy = graph->cyClient;
	iy = graph->cyClient - iy;												/* Invert sense */
	y = (1.0*iy-graph->y_left_margin) / max(1,graph->cyClient-graph->y_left_margin-graph->y_right_margin);
	return ymin+y*(ymax-ymin);
}

/* ===========================================================================
-- Class procedure for drawing scope trace graphics
--
-- Usage: CALLBACK CalGraphWndProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam);
--
-- Inputs: hwnd - handle for the client window
--         umsg - message to be processed
--         wParam, lParam - parameters for message
--
-- Output: message dependend
--
-- Return: 0 if message processed
=========================================================================== */
LRESULT CALLBACK GraphWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

/* Handles to simply drawing through a memory device context or real context */
/* hdc is the DC for drawing always ... either points to real DC or memory DC */
	HDC     hdc;								/* Device context for drawing (screen or memory) */
#ifdef USE_MEMORY_DC
	HDC     hdcWindow;						/* Device context for the actual graph window */
	HBITMAP hbmMem;							/* Bitmap to draw into */
	HANDLE  hOld;								/* Just to hold old bitmap */
#endif

	GRAPH_SCALES *scales;
	GRAPH_ZFORCE *zforce;
	GRAPH_AXIS_PARMS *axis_parms;
	GRAPH_DATA *graph;
	GRAPH_CURVE *cv;
	GRAPH_MESH *mesh;
	GRAPH_FNC *fnc;

	PAINTSTRUCT paintstruct;
	HBRUSH hbrush;
	HPEN	hpen, open;
	HFONT hfont_labels, hfont_sup, hfont_titles;
	RECT rect, myrect;
	POINT point; 

	double x,y, xtmp,ytmp;
	double xmin,xmax, dx,dx2;
	double ymin,ymax, dy,dy2;
	double xgmin, xgmax, ygmin, ygmax;			/* Graph scales */
	int rc;
	int cxClient, cyClient;
	int wID, wNotifyCode;
	int i,ig, ix,iy,idy, ipen;
	char szTmp[20];

/* Scaling */
	LABEL_FORMAT x_labels, y_labels;
	int iexp;

	static int dflt_colors[GRAPH_MAX_CURVES] = {
		RGB(255,255,255), RGB(255,0,0),		RGB(0,255,0), RGB(128,128,255),		/* 0-3 */
		RGB(255,255,0),	RGB(255,0,255),	RGB(0,255,255),						/* 4-6 */
		RGB(128,0,0),		RGB(0,128,0),		RGB(0,0,128)							/* 7-9 */
	};

/* Recover the pointer to the graph data (actually set by WM_CREATE if starting */
	graph = (GRAPH_DATA *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

/* Display what is happening in this window */
/*	SendMessage(hdlgLog, WMP_LOG_TEXT, (WPARAM) GetMsgInfo("GraphWndProc", msg, wParam, lParam), (LPARAM) 0); */
/*	ShowMsgInfo("CalGraphWindow", msg, wParam, lParam); */

	switch (msg) {

		case WM_CREATE:
			graph = calloc(sizeof(*graph),1);
			graph->hwnd = hwnd;										/* My handle */
			graph->mode = GR_LINEAR;
			graph->autox  = graph->autoy  = graph->autoz  = TRUE;
			graph->forcex = graph->forcey = graph->forcez = FALSE;
			graph->zforce_x = graph->zforce_y = graph->zforce_z = 0.3;
			graph->suppress_x_grid  = graph->suppress_y_grid  = FALSE;
			graph->suppress_x_major = graph->suppress_y_major = FALSE;
			graph->suppress_x_minor = graph->suppress_y_minor = FALSE;
			graph->nfncs = graph->ncurves = 0;
			graph->xmin = graph->ymin = 0;
			graph->xmax = graph->ymax = 1;
			graph->show_X_labels = TRUE;
			graph->show_Y_labels = TRUE;
			graph->show_X_title = TRUE;
			graph->show_Y_title = TRUE;
			graph->no_margins = FALSE;
			graph->background_color = -1;							/* Mark as no color */
			*graph->x_title = '\0';
			*graph->y_title = '\0';
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) graph);
			return 0;

		case WM_DESTROY:
			if (! graph->slave_process) SendMessage(hwnd, WMP_CLEAR, 0, 0);
			if (graph != NULL) free(graph);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG) 0);
			return 0;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			GetCursorPos((LPPOINT) &point);						/* Get where the cursor is located */
			ScreenToClient(hwnd, &point);							/* Convert to screen coordinates */
			GetClientRect(hwnd, &rect);
			if (graph->cursor_callback.hwnd != NULL) {
				GRAPH_CURSOR_INFO info;
				info.hwnd = hwnd;
				info.msg = msg;
				info.point = point;
				info.cxClient = rect.right - rect.left;		/* Don't really need substraction since client size */
				info.cyClient = rect.bottom - rect.top;
				info.xpixel = point.x;
				info.ypixel = info.cyClient - point.y;
				info.ingraph = FALSE;
				info.xfrac = (1.0*info.xpixel-graph->x_left_margin)/max(1,graph->cxClient-graph->x_left_margin-graph->x_right_margin);
				info.yfrac = (1.0*info.ypixel-graph->y_left_margin)/max(1,graph->cyClient-graph->y_left_margin-graph->y_right_margin);
				info.x = graph->xgmin + (graph->xgmax-graph->xgmin)*info.xfrac;
				info.y = graph->ygmin + (graph->ygmax-graph->ygmin)*info.yfrac;
				info.ingraph = (info.xfrac >= 0.0) && (info.xfrac <= 1.0) && (info.yfrac >= 0.0) && (info.yfrac <= 1.0);
				SendMessage(graph->cursor_callback.hwnd, graph->cursor_callback.wID, (WPARAM) &info, (LPARAM) graph);
			}
			break;

			if (msg == WM_RBUTTONDOWN) {
				{
					HMENU hMenu, hMenuTrackPopup;
					POINTL point;

					GetCursorPos((LPPOINT) &point);
					hMenu = LoadMenu(NULL, "IDM_GRAPH_POPUP_MENU");
					hMenuTrackPopup = GetSubMenu(hMenu, 0);
					TrackPopupMenu(hMenuTrackPopup, 
										TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON,
										point.x,point.y, 0, hwnd, NULL);
					DestroyMenu(hMenu);
				}
			}
			rc = 0; break;

		case WM_PAINT:
			#define	LABEL_MARGIN	(20)
			#define	TITLE_MARGIN	(16)
			#define	RIGHT_MARGIN	(5)

			/* Determine the size of what we need to paint */
			GetClientRect(hwnd, &rect);					/* left=top=0 right/bottom real */
			cxClient = rect.right;
			cyClient = rect.bottom;

			/* Create a device context, either raw of memory DC */
#ifdef USE_MEMORY_DC
			hdcWindow = BeginPaint(hwnd, &paintstruct);		/* Get DC */
			hdc       = CreateCompatibleDC(hdcWindow);
			hbmMem    = CreateCompatibleBitmap(hdcWindow, cxClient, cyClient);
			hOld      = SelectObject(hdc, hbmMem);
#else
			hdc       = BeginPaint(hwnd, &paintstruct);		/* Get DC */
#endif
			/* Save the extent of the window in graph structure (needed by get_ix) */
			graph->cxClient = cxClient; 
			graph->cyClient = cyClient;

			/* Initialize the margins (in pixels) -- used by get_ix / get_iy */
			graph->x_left_margin  = LABEL_MARGIN;
			graph->x_right_margin = RIGHT_MARGIN;
			graph->y_left_margin  = LABEL_MARGIN;
			graph->y_right_margin = RIGHT_MARGIN;

			/* If a background color is specified, fill it now */
			if (graph->background_color != -1) {
				hbrush = CreateSolidBrush(graph->background_color);
				FillRect(hdc, &rect, hbrush);
				DeleteObject(hbrush);
			}

			/* Go through all curves, update the min/max range if modified flag set */
			for (i=0; i<graph->ncurves; i++) {
				cv = graph->curve[i];
				if (cv->modified) UpdateCurveMinMax(cv);
				cv->modified = FALSE;
			}

			/* Find first master curve (if any) for autoscaling and grids (even if not displayed) */
			cv = NULL;
			for (i=0; i<graph->ncurves; i++) {
				if (graph->curve[i]->master && graph->curve[i]->visible) { cv = graph->curve[i]; break; }
			}

			/* Scan for the min/max in X */
			if (graph->mode != GR_LOGLOG) {
				xmin = graph->xmin;
				xmax = graph->xmax;
				if (graph->autox) {
					if (cv != NULL) {
						xmin = cv->xmin; xmax = cv->xmax;
					} else {
						xmin = DBL_MAX; xmax = -DBL_MAX;
						for (i=0; i<graph->ncurves; i++) {
							if (! graph->curve[i]->visible) continue;				/* Only base on displayed curves if no master */
							if (graph->curve[i]->xmin < xmin) xmin = graph->curve[i]->xmin;
							if (graph->curve[i]->xmax > xmax) xmax = graph->curve[i]->xmax;
						}
						if (xmin == DBL_MAX || xmax == -DBL_MAX) { xmin = 0; xmax = 1; }
					}
				}
			} else {
				xmin = graph->xmin;
				xmax = graph->xmax;
				if (graph->autox) {
					if (cv != NULL) {
						xmin = cv->logxmin; xmax = cv->logxmax;
					} else {
						xmin = DBL_MAX; xmax = -DBL_MAX;
						for (i=0; i<graph->ncurves; i++) {
							if (! graph->curve[i]->visible) continue;				/* Only base on displayed curves if no master */
							if (graph->curve[i]->xmin < xmin) xmin = graph->curve[i]->logxmin;
							if (graph->curve[i]->xmax > xmax) xmax = graph->curve[i]->logxmax;
						}
						if (xmin == DBL_MAX || xmax == -DBL_MAX) { xmin = 0; xmax = 1; }
					}
				}
			}
			if (xmin == xmax) xmax = (xmin == 0) ? 1.0 : ( (xmin > 0) ? 2*xmin : 0 );
			if (xmin > xmax) { xmin = 0; xmax = 1; }
			PlotAutoScaleZ(xmin, xmax, &xgmin, &xgmax, &dx, &dx2, NULL, graph->zforce_x);

			/* And do the same for Y */
			if (graph->mode == GR_LINEAR) {
				ymin = graph->ymin;
				ymax = graph->ymax;
				if (graph->autoy) {
					if (cv != NULL) {
						ymin = cv->ymin; ymax = cv->ymax;
					} else {
						ymin = DBL_MAX; ymax = -DBL_MAX;
						for (i=0; i<graph->ncurves; i++) {
							if (! graph->curve[i]->visible) continue;				/* Only base on displayed curves if no master */
							if (graph->curve[i]->ymin < ymin) ymin = graph->curve[i]->ymin;
							if (graph->curve[i]->ymax > ymax) ymax = graph->curve[i]->ymax;
						}
						if (ymin == DBL_MAX || ymax == -DBL_MAX) { ymin = 0; ymax = 1; }
					}
				} else {
				}
			} else {
				ymin = graph->ymin;
				ymax = graph->ymax;
				if (graph->autoy) {
					if (cv != NULL) {
						ymin = cv->logymin; ymax = cv->logymax;
					} else {
						ymin = DBL_MAX; ymax = -DBL_MAX;
						for (i=0; i<graph->ncurves; i++) {
							if (! graph->curve[i]->visible) continue;				/* Only base on displayed curves if no master */
							if (graph->curve[i]->ymin < ymin) ymin = graph->curve[i]->logymin;
							if (graph->curve[i]->ymax > ymax) ymax = graph->curve[i]->logymax;
						}
						if (ymin == DBL_MAX || ymax == -DBL_MAX) { ymin = 0; ymax = 1; }
					}
				}
			}
			if (ymin == ymax) ymax = (ymin == 0) ? 1.0 : ( (ymin > 0) ? 2*ymin : 0 );
			if (ymin > ymax) { ymin = 0; ymax = 1; }
			PlotAutoScaleZ(ymin, ymax, &ygmin, &ygmax, &dy, &dy2, NULL, graph->zforce_y);

			/* Either use the new nice values or revert to exact if forced */
			if (graph->forcex) {
				xgmin = xmin; xgmax = xmax;
			} else {
				xmin = xgmin; xmax = xgmax;
			}
			if (graph->forcey) {
				ygmin = ymin; ygmax = ymax;
			} else {
				ymin = ygmin; ymax = ygmax;
			}
			
			/* Save values for other routines to use */
			graph->xgmin = xmin; graph->xgmax = xmax;
			graph->ygmin = ymin; graph->ygmax = ymax;

			/* Figure out how to label the x and y axes */
			memset(&x_labels, 0, sizeof(LABEL_FORMAT));		/* Clear all values */
			memset(&y_labels, 0, sizeof(LABEL_FORMAT));		/* For both X and Y */
			if (graph->show_X_labels) {
				FormatLabels(&x_labels, xmin, xmax, dx);
				graph->y_left_margin = LABEL_MARGIN;
			} else {
				graph->y_left_margin = RIGHT_MARGIN;
			}
			if (graph->show_Y_labels) {
				FormatLabels(&y_labels, ymin, ymax, dy);
				graph->x_left_margin = LABEL_MARGIN;
			} else {
				graph->x_left_margin = RIGHT_MARGIN;
			}

			/* Adjust the margins for titles if required */
			if ((x_labels.sci && graph->show_X_labels) || (graph->show_X_title && (strlen(graph->x_title) != 0))) graph->y_left_margin += TITLE_MARGIN;
			if ((y_labels.sci && graph->show_Y_labels) || (graph->show_Y_title && (strlen(graph->y_title) != 0))) graph->x_left_margin += TITLE_MARGIN;

			if (graph->no_margins) {
				graph->x_left_margin = graph->x_right_margin = graph->y_left_margin = graph->y_right_margin = 1;
			}

			/* Draw the constant X lines */
			hpen = CreatePen(PS_SOLID, 1, RGB(128,128,128));
			open = SelectObject(hdc, hpen);
			SetTextColor(hdc, RGB(255,255,255));
			SetBkColor(hdc, RGB(0,0,0));
			if (TRUE || graph->show_X_labels || (graph->show_X_title && (strlen(graph->x_title)) != 0)) {
				hfont_labels = CreateFont(15, 0, 0, 0, FW_MEDIUM, 
												  FALSE, FALSE, FALSE, 
												  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
												  CLEARTYPE_QUALITY, VARIABLE_PITCH, NULL);
				hfont_titles = CreateFont(16, 0, 0, 0, FW_MEDIUM, 
												  FALSE, FALSE, FALSE, 
												  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
												  CLEARTYPE_QUALITY, VARIABLE_PITCH, NULL);
				hfont_sup    = CreateFont(12, 0, 0, 0, FW_MEDIUM, 
												  FALSE, FALSE, FALSE, 
												  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
												  CLEARTYPE_QUALITY, VARIABLE_PITCH, NULL);
				SelectObject(hdc, hfont_labels);
			}
			/* Draw box around graph */
			MoveToEx(hdc, get_ix(0.0, 0.0,1.0, graph), get_iy(0.0, 0.0,1.0, graph), (LPPOINT) NULL);
			LineTo  (hdc, get_ix(1.0, 0.0,1.0, graph), get_iy(0.0, 0.0,1.0, graph));
			LineTo  (hdc, get_ix(1.0, 0.0,1.0, graph), get_iy(1.0, 0.0,1.0, graph));
			LineTo  (hdc, get_ix(0.0, 0.0,1.0, graph), get_iy(1.0, 0.0,1.0, graph));
			LineTo  (hdc, get_ix(0.0, 0.0,1.0, graph), get_iy(0.0, 0.0,1.0, graph));

			for (x=dx*((int) (xmin/dx)); x<xmax+dx/20; x+=dx) {
				ix = get_ix(x, xmin,xmax, graph);
				if (x < xmin-dx/20) continue;
				if (! graph->suppress_x_grid) {														/* Full lines */
					MoveToEx(hdc, ix, get_iy(0.0, 0.0,1.0, graph), (LPPOINT) NULL);
					LineTo  (hdc, ix, get_iy(1.0, 0.0,1.0, graph));
				} else if (! graph->suppress_x_major) {											/* Maybe just tick marks */
					MoveToEx(hdc, ix, get_iy( 0, 0,30, graph), (LPPOINT) NULL);
					LineTo  (hdc, ix, get_iy( 2, 0,30, graph));
					MoveToEx(hdc, ix, get_iy(30, 0,30, graph), (LPPOINT) NULL);
					LineTo  (hdc, ix, get_iy(28, 0,30, graph));
				}
				if (graph->show_X_labels) {
					if (x == xmin) ix -= 3;
					if (x == xmax) ix += 3;
					EncodeLabel(x, &x_labels, szTmp, sizeof(szTmp));
					SetTextAlign(hdc, (x==xmin) ? TA_LEFT : (x>=xmax-dx/4) ? TA_RIGHT : TA_CENTER);
					iy = get_iy(0.0, 0.0,1.0, graph)+2;
					TextOut(hdc, ix, iy, szTmp, (int) strlen(szTmp));
				}
			}
			if (! graph->suppress_x_minor) {						/* Maybe minor tick marks */
				for (x=dx2*((int) (xmin/dx2)); x<xmax+dx2/20; x+=dx2) {
					if (x < xmin-dx2/20) continue;
					ix = get_ix(x, xmin,xmax, graph);
					MoveToEx(hdc, ix, get_iy( 0, 0,30, graph), (LPPOINT) NULL);
					LineTo  (hdc, ix, get_iy( 1, 0,30, graph));
					MoveToEx(hdc, ix, get_iy(30, 0,30, graph), (LPPOINT) NULL);
					LineTo  (hdc, ix, get_iy(29, 0,30, graph));
				}
			}
			if (graph->show_X_labels && x_labels.sci) {		/* Add x10^-3 */
				iexp = x_labels.iexp;
				SetTextAlign(hdc, TA_RIGHT);
				ix = get_ix(1.0, 0.0,1.0, graph)-5;				/* 1 for edge / 4 for one character */
				if (iexp < 0) ix -= 3;
				if (fabs(iexp) >= 10) ix -= 4;
				iy = get_iy(0.0, 0.0,1.0, graph)+LABEL_MARGIN-2;
				TextOut(hdc, ix, iy, "x 10", 4);

				sprintf_s(szTmp, sizeof(szTmp), "%d", iexp);
				SelectObject(hdc, hfont_sup);
				SetTextAlign(hdc, TA_LEFT);
				TextOut(hdc, ix, iy-3, szTmp, (int) strlen(szTmp));	/* Shift up as superscript */
			}
			if (graph->show_X_title && (strlen(graph->x_title) != 0)) {
				ix = get_ix(0.5, 0.0,1.0, graph);
				iy = get_iy(0.0, 0.0,1.0, graph)+ (graph->show_X_labels ? LABEL_MARGIN-1 : 3) ;
				SelectObject(hdc, hfont_titles);
				SetTextAlign(hdc, TA_CENTER);
				TextOut(hdc, ix, iy, graph->x_title, (int) strlen(graph->x_title));
			}
			if (TRUE || graph->show_X_labels || (graph->show_X_title && (strlen(graph->x_title) != 0))) {
				DeleteObject(hfont_labels);
				DeleteObject(hfont_titles);
				DeleteObject(hfont_sup);
			}
			
			/* And now the constant Y lines */
			if (graph->show_Y_labels || (graph->show_Y_title && (strlen(graph->y_title) != 0))) {
				hfont_labels = CreateFont(12, 0, 899, 899, FW_MEDIUM, 
												  FALSE, FALSE, FALSE, 
												  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
												  CLEARTYPE_QUALITY, VARIABLE_PITCH, NULL);
				hfont_titles = CreateFont(14, 0, 899, 899, FW_MEDIUM, 
												  FALSE, FALSE, FALSE, 
												  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
												  CLEARTYPE_QUALITY, VARIABLE_PITCH, NULL);
				hfont_sup    = CreateFont(10, 0, 899, 899, FW_MEDIUM, 
												  FALSE, FALSE, FALSE, 
												  DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, 
												  CLEARTYPE_QUALITY, VARIABLE_PITCH, NULL);
				SelectObject(hdc, hfont_labels);
			}
			for (y=dy*((int) (ymin/dy)); y<ymax+dy/20; y+=dy) {
				if (y < ymin-dy/20) continue;
				iy = get_iy(y, ymin,ymax, graph);
				if (! graph->suppress_y_grid) {														/* Full lines */
					MoveToEx(hdc, get_ix(0.0, 0.0,1.0, graph), iy, (LPPOINT) NULL);
					LineTo  (hdc, get_ix(1.0, 0.0,1.0, graph), iy);
				} else if (! graph->suppress_y_major) {												/* Maybe just tick marks */
					MoveToEx(hdc, get_ix( 0, 0,30, graph), iy, (LPPOINT) NULL);
					LineTo  (hdc, get_ix( 2, 0,30, graph), iy);
					MoveToEx(hdc, get_ix(30, 0,30, graph), iy, (LPPOINT) NULL);
					LineTo  (hdc, get_ix(28, 0,30, graph), iy);
				}
				if (graph->show_Y_labels) {
					if (y == ymin) iy += 3;
					if (y == ymax) iy -= 3;
					EncodeLabel(y, &y_labels, szTmp, sizeof(szTmp));
					SetTextAlign(hdc, (y==ymin) ? TA_LEFT : (y>=ymax-dx/4) ? TA_RIGHT : TA_CENTER);
					TextOut(hdc, get_ix(0.0, 0.0,1.0, graph) - 15, iy, szTmp, (int) strlen(szTmp));
				}
			}
			if (! graph->suppress_y_minor) {						/* Maybe minor tick marks */
				for (y=dy2*((int) (ymin/dy2)); y<ymax+dy2/20; y+=dy2) {
					if (y < ymin-dy2/20) continue;
					iy = get_iy(y, ymin,ymax, graph);
					MoveToEx(hdc, get_ix( 0, 0,30, graph), iy, (LPPOINT) NULL);
					LineTo  (hdc, get_ix( 1, 0,30, graph), iy);
					MoveToEx(hdc, get_ix(30, 0,30, graph), iy, (LPPOINT) NULL);
					LineTo  (hdc, get_ix(29, 0,30, graph), iy);
				}
			}
			if (graph->show_Y_labels && y_labels.sci) {		/* Add x10^-3 */
				iexp = y_labels.iexp;
				SetTextAlign(hdc, TA_RIGHT);
				ix = get_ix(0.0, 0.0,1.0, graph) - 27;			/* Was 25 previously */
				iy = get_iy(1.0, 0.0,1.0, graph) + 4;			/* 1 for edge / 3 for one char */
				if (iexp < 0) iy += 5;
				if (fabs(iexp) >= 10) iy += 4;
				TextOut(hdc, ix, iy, "x 10", 4);

				sprintf_s(szTmp, sizeof(szTmp), "%d", iexp);
				SelectObject(hdc, hfont_sup);
				SetTextAlign(hdc, TA_LEFT);
				TextOut(hdc, ix-4, iy, szTmp, (int) strlen(szTmp));
			}
			if (graph->show_Y_title && (strlen(graph->y_title) != 0)) {
				ix = get_ix(0.0, 0.0,1.0, graph)-(graph->show_Y_labels ? LABEL_MARGIN : 4) - TITLE_MARGIN + 2;
				iy = get_iy(0.5, 0.0,1.0, graph);
				SelectObject(hdc, hfont_titles);
				SetTextAlign(hdc, TA_CENTER);
				TextOut(hdc, ix, iy, graph->y_title, (int) strlen(graph->y_title));
			}
			if (TRUE || graph->show_Y_labels || (graph->show_Y_title && (strlen(graph->y_title) != 0))) {
				DeleteObject(hfont_labels);
				DeleteObject(hfont_sup);
				DeleteObject(hfont_titles);
			}
			SelectObject(hdc, open);
			DeleteObject(hpen);

			/* ========================================== */
			/* Draw all the solid lines (functions) first */
			/* ========================================== */
			for (ig=0; ig<graph->nfncs; ig++) {
				double rmin,rmax;
				int npt;
				
				fnc = graph->function[ig];
				if (! fnc->visible) continue;
				xmin = xgmin; xmax = xgmax; ymin = ygmin; ymax = ygmax;
				if (fnc->force_scale_x) { xmin = fnc->xmin; xmax = fnc->xmax; }
				if (fnc->force_scale_y) { ymin = fnc->ymin; ymax = fnc->ymax; }

				ipen = (fnc->rgb != 0) ? fnc->rgb : dflt_colors[ig] ;
				hpen = CreatePen(PS_SOLID, 1, ipen);
				open = SelectObject(hdc, hpen);

				rmin = (fnc->rmin != fnc->rmax) ? fnc->rmin : xmin ;
				rmax = (fnc->rmin != fnc->rmax) ? fnc->rmax : xmax ;
				npt  = (fnc->npt  >= 2) ? fnc->npt : 100 ;
				for (i=0; i<npt; i++) {
					x = rmin + (rmax-rmin) * i / (npt-1.0) ;
					y = (*fnc->fnc)(x, fnc->args);
					ix = get_ix(x, xmin,xmax, graph);
					iy = get_iy(y, ymin,ymax, graph);
					if (i == 0) {
						MoveToEx(hdc, ix, iy, (LPPOINT) NULL);
					} else {
						LineTo(hdc, ix, iy);
					}
				}
				if (fnc->draw_x_axis && ymin*ymax <= 0.0) {
					iy = get_iy(0, ymin, ymax, graph);
					MoveToEx(hdc, get_ix(0.0, 0.0,1.0, graph), iy, (LPPOINT) NULL);
					LineTo(hdc,   get_ix(1.0, 0.0,1.0, graph), iy);
				}
				if (fnc->draw_y_axis && xmin*xmax <= 0.0) {
					ix = get_ix(0, xmin, xmax, graph);
					MoveToEx(hdc, ix, get_iy(0.0, 0.0,1.0, graph), (LPPOINT) NULL);
					LineTo(hdc,   ix, get_iy(1.0, 0.0,1.0, graph));
				}
				SelectObject(hdc, open);
				DeleteObject(hpen);
			}

			/* ====================================== */
			/* Draw all the data points (curves) next */
			/* ====================================== */
			for (ig=0; ig<graph->ncurves; ig++) {
				cv = graph->curve[ig];
				if (! cv->visible) continue;
				switch (graph->mode) {
					case GR_LINEAR:
						xmin = xgmin; xmax = xgmax; ymin = ygmin; ymax = ygmax;
						if (cv->force_scale_x) { xmin = cv->xmin; xmax = cv->xmax; }
						if (cv->force_scale_y) { ymin = cv->ymin; ymax = cv->ymax; }
						if (cv->autoscale_x) PlotAutoScaleZ(cv->xmin, cv->xmax, &xmin, &xmax, NULL, NULL, NULL, graph->zforce_x);
						if (cv->autoscale_y) PlotAutoScaleZ(cv->ymin, cv->ymax, &ymin, &ymax, NULL, NULL, NULL, graph->zforce_y);
						break;
					case GR_LOGLIN:
						xmin = xgmin; xmax = xgmax; ymin = ygmin; ymax = ygmax;
						if (cv->force_scale_x) { xmin = cv->xmin; xmax = cv->xmax; }
						if (cv->force_scale_y) { ymin = cv->logymin; ymax = cv->logymax; }
						break;
					case GR_LOGLOG:
						xmin = xgmin; xmax = xgmax; ymin = ygmin; ymax = ygmax;
						if (cv->force_scale_y) { xmin = cv->logxmin; ymax = cv->logxmax; }
						if (cv->force_scale_y) { ymin = cv->logymin; ymax = cv->logymax; }
						break;
				}

				/* Independent of whether there are independent point colors, set one default now to get current values */
				ipen = (cv->rgb != 0) ? cv->rgb : dflt_colors[ig] ;
				hpen = CreatePen(PS_SOLID, 1, ipen);
				open = SelectObject(hdc, hpen);
				hbrush = CreateSolidBrush(ipen);

				/* Need the connecting lines? */
				if (cv->flags & CURVE_FLAG_LINES) {
					BOOL penup = TRUE;
					for (i=0; i<cv->npt; i++) {
						xtmp = cv->x[i];	ytmp = cv->y[i];
						if (xtmp == 0 && graph->mode == GR_LOGLOG) xtmp = 1E-12;
						if (ytmp == 0 && graph->mode != GR_LINEAR) ytmp = 1E-12;
						xtmp = (graph->mode != GR_LOGLOG) ? xtmp : log(fabs(xtmp));
						ytmp = (graph->mode == GR_LINEAR) ? ytmp : log(fabs(ytmp));
						if (OutOfRange(xtmp, ytmp, xmin, xmax, ymin, ymax)) {
							penup = TRUE;
							continue;
						}
						ix = get_ix(xtmp, xmin,xmax, graph);
						iy = get_iy(ytmp, ymin,ymax, graph);
						if (penup) {
							MoveToEx(hdc, ix, iy, (LPPOINT) NULL);
							penup = FALSE;
						} else {
							LineTo(hdc, ix, iy);
						}
					}
				} 
				if (cv->flags == 0 || cv->flags & CURVE_FLAG_POINTS) {
					/* if < 200 points, make each a 3x3 square, otherwise small */
					for (i=0; i<cv->npt; i++) {
						xtmp = cv->x[i];	ytmp = cv->y[i];
						if (xtmp == 0 && graph->mode == GR_LOGLOG) xtmp = 1E-12;
						if (ytmp == 0 && graph->mode != GR_LINEAR) ytmp = 1E-12;
						xtmp = (graph->mode != GR_LOGLOG) ? xtmp : log(fabs(xtmp));
						ytmp = (graph->mode == GR_LINEAR) ? ytmp : log(fabs(ytmp));
						if (OutOfRange(xtmp, ytmp, xmin, xmax, ymin, ymax)) continue;
						ix = get_ix(xtmp, xmin,xmax, graph);
						iy = get_iy(ytmp, ymin,ymax, graph);
						if (cv->isize > 0) {							/* Specified size */
							myrect.left = ix-cv->isize+1; myrect.right  = ix+cv->isize;
							myrect.top  = iy-cv->isize+1; myrect.bottom = iy+cv->isize;
						} else if (cv->npt < 200) {
							myrect.left = ix-1; myrect.right  = ix+2;
							myrect.top  = iy-1; myrect.bottom = iy+2;
						} else {
							myrect.left = ix; myrect.right  = ix+1;
							myrect.top  = iy; myrect.bottom = iy+1;
						}
						if (cv->pt_rgb != NULL) {
							if (cv->pt_rgb[i] != 0) {
								HPEN open;
								HBRUSH hbrush;
								
								ipen = cv->pt_rgb[i];
								hpen = CreatePen(PS_SOLID, 1, ipen);
								open = SelectObject(hdc, hpen);
								hbrush = CreateSolidBrush(ipen);
								FillRect(hdc, &myrect, hbrush);
								SelectObject(hdc,open);
								DeleteObject(hpen);
								DeleteObject(hbrush);
							} else {
								FillRect(hdc, &myrect, hbrush);
							}
						} else {
							FillRect(hdc, &myrect, hbrush);
						}
						if (cv->s != NULL && graph->mode == GR_LINEAR) {
							idy = (int) (cv->s[i]/(ymax-ymin)*graph->cyClient+0.5);
							if (idy > 0) {
								MoveToEx(hdc, ix, iy+idy, (LPPOINT) NULL);
								LineTo(hdc, ix, iy-idy);
							}
						}
					}
					/* If we used unique colors, go back to default now */
					if (cv->pt_rgb != NULL) {
						ipen = (cv->rgb != 0) ? cv->rgb : dflt_colors[ig] ;
						hpen = CreatePen(PS_SOLID, 1, ipen);
						SelectObject(hdc, hpen);
						hbrush = CreateSolidBrush(ipen);
					}
				}

				if (cv->draw_x_axis && ymin*ymax <= 0.0) {
					iy = get_iy(0, ymin, ymax, graph);
					MoveToEx(hdc, get_ix(0.0, 0.0,1.0, graph), iy, (LPPOINT) NULL);
					LineTo(hdc,   get_ix(1.0, 0.0,1.0, graph), iy);
				}
				if (cv->draw_y_axis && xmin*xmax <= 0.0) {
					ix = get_ix(0, xmin, xmax, graph);
					MoveToEx(hdc, ix, get_iy(0.0, 0.0,1.0, graph), (LPPOINT) NULL);
					LineTo(hdc,   ix, get_iy(1.0, 0.0,1.0, graph));
				}
				SelectObject(hdc, open);
				DeleteObject(hpen);
				DeleteObject(hbrush);
			}

			/* ====================================== */
			/* And finally the triangles (which may be on top of the points)
			/* ====================================== */
			for (ig=0; ig<graph->nmeshes; ig++) {
				mesh = graph->meshes[ig];
				if (! mesh->visible) continue;
				switch (graph->mode) {
					case GR_LINEAR:
						xmin = xgmin; xmax = xgmax; ymin = ygmin; ymax = ygmax;
						if (mesh->force_scale_x) { xmin = mesh->xmin; xmax = mesh->xmax; }
						if (mesh->force_scale_y) { ymin = mesh->ymin; ymax = mesh->ymax; }
						if (mesh->autoscale_x) PlotAutoScaleZ(mesh->xmin, mesh->xmax, &xmin, &xmax, NULL, NULL, NULL, graph->zforce_x);
						if (mesh->autoscale_y) PlotAutoScaleZ(mesh->ymin, mesh->ymax, &ymin, &ymax, NULL, NULL, NULL, graph->zforce_y);
						break;
					case GR_LOGLIN:
						xmin = xgmin; xmax = xgmax; ymin = ygmin; ymax = ygmax;
						if (mesh->force_scale_x) { xmin = mesh->xmin; xmax = mesh->xmax; }
						if (mesh->force_scale_y) { ymin = mesh->logymin; ymax = mesh->logymax; }
						break;
					case GR_LOGLOG:
						xmin = xgmin; xmax = xgmax; ymin = ygmin; ymax = ygmax;
						if (mesh->force_scale_y) { xmin = mesh->logxmin; ymax = mesh->logxmax; }
						if (mesh->force_scale_y) { ymin = mesh->logymin; ymax = mesh->logymax; }
						break;
				}

				/* Independent of whether there are independent point colors, set one default now to get current values */
				ipen = (mesh->rgb != 0) ? mesh->rgb : dflt_colors[ig] ;
				hpen = CreatePen(PS_SOLID, 1, ipen);
				open = SelectObject(hdc, hpen);
				hbrush = CreateSolidBrush(ipen);

				for (i=0; i<mesh->npt; i++) {
					double x0,y0, x1,y1, x2,y2;
					int ix0,iy0, ix1,iy1, ix2, iy2;

					x0 = mesh->t[i].x0;	y0 = mesh->t[i].y0;
					x1 = mesh->t[i].x1;	y1 = mesh->t[i].y1;
					x2 = mesh->t[i].x2;	y2 = mesh->t[i].y2;
					
					if (x0 == 0 && graph->mode == GR_LOGLOG) x0 = 1E-12;
					if (y0 == 0 && graph->mode != GR_LINEAR) y0 = 1E-12;
					if (x1 == 0 && graph->mode == GR_LOGLOG) x1 = 1E-12;
					if (y1 == 0 && graph->mode != GR_LINEAR) y1 = 1E-12;
					if (x2 == 0 && graph->mode == GR_LOGLOG) x2 = 1E-12;
					if (y2 == 0 && graph->mode != GR_LINEAR) y2 = 1E-12;
					x0 = (graph->mode != GR_LOGLOG) ? x0 : log(fabs(x0));
					y0 = (graph->mode == GR_LINEAR) ? y0 : log(fabs(y0));
					x1 = (graph->mode != GR_LOGLOG) ? x1 : log(fabs(x1));
					y1 = (graph->mode == GR_LINEAR) ? y1 : log(fabs(y1));
					x2 = (graph->mode != GR_LOGLOG) ? x2 : log(fabs(x2));
					y2 = (graph->mode == GR_LINEAR) ? y2 : log(fabs(y2));
					if (OutOfRange(x0, y0, xmin, xmax, ymin, ymax)) continue;
					if (OutOfRange(x1, y1, xmin, xmax, ymin, ymax)) continue;
					if (OutOfRange(x2, y2, xmin, xmax, ymin, ymax)) continue;
					ix0 = get_ix(x0, xmin,xmax, graph);
					iy0 = get_iy(y0, ymin,ymax, graph);
					ix1 = get_ix(x1, xmin,xmax, graph);
					iy1 = get_iy(y1, ymin,ymax, graph);
					ix2 = get_ix(x2, xmin,xmax, graph);
					iy2 = get_iy(y2, ymin,ymax, graph);
					MoveToEx(hdc, ix0, iy0, (LPPOINT) NULL);
					LineTo(hdc, ix1, iy1);
					LineTo(hdc, ix2, iy2);
					LineTo(hdc, ix0, iy0);
				}

				if (mesh->draw_x_axis && ymin*ymax <= 0.0) {
					iy = get_iy(0, ymin, ymax, graph);
					MoveToEx(hdc, get_ix(0.0, 0.0,1.0, graph), iy, (LPPOINT) NULL);
					LineTo(hdc,   get_ix(1.0, 0.0,1.0, graph), iy);
				}
				if (mesh->draw_y_axis && xmin*xmax <= 0.0) {
					ix = get_ix(0, xmin, xmax, graph);
					MoveToEx(hdc, ix, get_iy(0.0, 0.0,1.0, graph), (LPPOINT) NULL);
					LineTo(hdc,   ix, get_iy(1.0, 0.0,1.0, graph));
				}
				SelectObject(hdc, open);
				DeleteObject(hpen);
				DeleteObject(hbrush);
			}

			/* If requested, inform routine with hdc to allow more painting */
			if (graph->paint_callback.hwnd != NULL) {
				SendMessage(graph->paint_callback.hwnd, graph->paint_callback.wID, (WPARAM) hdc, (LPARAM) graph);
			}

#ifdef USE_MEMORY_DC
			BitBlt(hdcWindow, 0, 0, cxClient, cyClient, hdc, 0, 0, SRCCOPY);		/* Copy bitmap over */
			SelectObject(hdc, hOld);															/* Unlink the memory bitmap */
			DeleteObject(hbmMem);																/* Free the memory bitmap */
			DeleteDC(hdc);																			/* Free the memory DC */
#endif
			EndPaint(hwnd, &paintstruct);					/* Release DC */
			rc = 0; break;

		case WMP_SET_SLAVE:									/* Set this as a slave graph ... don't release memory on close */
			graph->slave_process = (BOOL) wParam;
			rc = 0; break;
			
		case WMP_LOGMODE:
			graph->mode = (GRAPH_MODE) wParam;
			if (graph->mode != GR_LINEAR && graph->mode != GR_LOGLIN && graph->mode != GR_LOGLOG) graph->mode = GR_LINEAR;
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;
			
		case WMP_GET_NUM_CURVES:
			rc = graph->ncurves;
			break;

		case WMP_GET_NUM_FUNCTIONS:
			rc = graph->nfncs;
			break;
			
		case WMP_GET_CURVE:
			rc = -1;																/* Curve does not exist	 */
			if ((int) lParam != 0) *((void **) lParam) = NULL;		/* Default return value	 */
			i = (int) wParam;													/* What curve do we want */
			if (i > 0 && i <= graph->ncurves) {
				if ((int) lParam != 0) *((GRAPH_CURVE **) lParam) = graph->curve[i-1];
				rc = 0;															/* Curve does exist */
			}
			break;

		case WMP_GET_FUNCTION:
			rc = -1;																/* Curve does not exist	 */
			if ((int) lParam != 0) *((void **) lParam) = NULL;		/* Default return value	 */
			i = (int) wParam;													/* What curve do we want */
			if (i > 0 && i <= graph->nfncs) {
				if ((int) lParam != 0) *((GRAPH_FNC **) lParam) = graph->function[i];
				rc = 0;															/* Curve does exist */
			}
			break;

		case WMP_CLEAR:
			SendMessage(hwnd, WMP_CLEAR_FUNCTIONS, 0, 0);
			SendMessage(hwnd, WMP_CLEAR_MESHES, 0, 0);
			SendMessage(hwnd, WMP_CLEAR_CURVES, 0, 0);
			rc = 0; break;

		case WMP_SET_CURVE_VISIBILITY:				/* Change visibility flag */
			i = ((int) wParam) - 1;						/* Which do we want to delete */
			if (i >= 0 && i < graph->ncurves) {
				graph->curve[i]->visible = ( (int) lParam != 0);
				InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			}
			rc = 0; break;

		case WMP_SET_MESH_VISIBILITY:					/* Change visibility flag */
			i = ((int) wParam) - 1;						/* Which do we want to delete */
			if (i >= 0 && i < graph->nmeshes) {
				graph->meshes[i]->visible = ( (int) lParam != 0);
				InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			}
			rc = 0; break;

		case WMP_SET_FNC_VISIBILITY:					/* Change visibility flag */
			i = ((int) wParam) - 1;						/* Which do we want to delete */
			if (i >= 0 && i < graph->nfncs) {
				graph->function[i]->visible = ( (int) wParam != 0);
			}
			rc = 0; break;

		case WMP_CLEAR_CURVE_BY_INDEX:
			i = ((int) wParam) - 1;						/* Which do we want to delete */
			if (i >= 0 && i < graph->ncurves) {
				free_curve(graph->curve[i]);
				graph->ncurves--;
				for (; i<graph->ncurves; i++) graph->curve[i] = graph->curve[i+1];	/* Move curve */
				InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			}
			rc = 0; break;

		case WMP_CLEAR_CURVE_BY_POINTER:
			for (i=0; i<graph->ncurves; i++) {
				if (graph->curve[i] == (GRAPH_CURVE *) wParam) break;
			}
			if (i >= 0 && i < graph->ncurves) SendMessage(hwnd, WMP_CLEAR_CURVE_BY_INDEX, (WPARAM) i+1, 0);
			rc = 0; break;

		case WMP_CLEAR_CURVE_BY_ID:
			for (i=0; i<graph->ncurves; i++) {
				if (graph->curve[i]->ID == (int) wParam) break;
			}
			if (i >= 0 && i < graph->ncurves) SendMessage(hwnd, WMP_CLEAR_CURVE_BY_INDEX, (WPARAM) i+1, 0);
			rc = 0; break;

		case WMP_CLEAR_CURVES_KEEP_LAST:
			if (graph->ncurves > 1) {
				for (i=0; i<graph->ncurves-1; i++) free_curve(graph->curve[i]);
				graph->curve[0] = graph->curve[graph->ncurves-1];
				graph->ncurves = 1;
				InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			}
			rc = 0; break;

		case WMP_CLEAR_CURVES:
			for (i=0; i<graph->ncurves; i++) free_curve(graph->curve[i]);
			graph->ncurves = 0;
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		case WMP_CLEAR_MESHES:
			for (i=0; i<graph->nmeshes; i++) free_mesh(graph->meshes[i]);
			graph->nmeshes = 0;
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		case WMP_CLEAR_FUNCTIONS:
			for (i=0; i<graph->nfncs; i++) {
				if (graph->function[i]->free_on_clear) free(graph->function[i]);
			}
			graph->nfncs = 0;
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		/* wPARAM has GRAPH_CURVE pointer;  if lParam==0, will do update refresh */
		case WMP_ADD_CURVE:
			cv = (GRAPH_CURVE *) wParam;

			/* Update curve information as if modified (since new) */
			UpdateCurveMinMax(cv);
			cv->modified = FALSE;

			/* See if already in list, which is just update, or if I should add */
			for (i=0; i<graph->ncurves; i++) {								/* Make sure not already present */
				if (graph->curve[i] == cv) break;
			}
			if (i == graph->ncurves) {											/* Not in the list */
				if (graph->ncurves >= GRAPH_MAX_CURVES) graph->ncurves = GRAPH_MAX_CURVES-1;
				graph->curve[graph->ncurves++] = cv;
			}
			if (lParam == 0) InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		/* wPARAM has GRAPH_CURVE pointer;  if lParam==0, will do update refresh */
		case WMP_ADD_MESH:
			mesh = (GRAPH_MESH *) wParam;

			/* Update curve information as if modified (since new) */
			UpdateMeshMinMax(mesh);
			mesh->modified = FALSE;

			/* See if already in list, which is just update, or if I should add */
			for (i=0; i<graph->nmeshes; i++) {							/* Make sure not already present */
				if (graph->meshes[i] == mesh) break;
			}
			if (i == graph->nmeshes) {										/* Not in the list */
				if (graph->nmeshes >= GRAPH_MAX_MESHES) graph->nmeshes = GRAPH_MAX_MESHES-1;
				graph->meshes[graph->nmeshes++] = mesh;
			}
			if (lParam == 0) InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		case WMP_ADD_FUNCTION:
			fnc = (GRAPH_FNC *) wParam;
			for (i=0; i<graph->nfncs; i++) {								/* Make sure not already present */
				if (graph->function[i] == fnc) break;
			}
			if (i == graph->nfncs) {										/* Not in the list */
				graph->function[graph->nfncs] = fnc;
				if (graph->nfncs < GRAPH_MAX_FNCS) graph->nfncs++;
			}
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		case WMP_SET_NO_MARGINS:
			graph->no_margins = (wParam != 0);
			rc = 0; break;

		case WMP_SET_BACKGROUND_COLOR:
			graph->background_color = (int) wParam;
			rc = 0; break;

		case WMP_SET_X_TITLE:
			if ( ((char *) wParam) != NULL) {
				strcpy_m(graph->x_title, sizeof(graph->x_title), (char *) wParam);
				graph->x_title[sizeof(graph->x_title)-1] = '\0';
			} else {
				*graph->x_title = '\0';
			}
			graph->x_title[sizeof(graph->x_title)-1] = '\0';
			rc = 0; break;

		case WMP_SET_Y_TITLE:
			if ( ((char *) wParam) != NULL) {
				strcpy_m(graph->y_title, sizeof(graph->y_title), (char *) wParam);
				graph->y_title[sizeof(graph->y_title)-1] = '\0';
			} else {
				*graph->y_title = '\0';
			}
			graph->y_title[sizeof(graph->y_title)-1] = '\0';
			rc = 0; break;

		case WMP_SET_LABEL_VISIBILITY:
			graph->show_X_labels = ( ((int) wParam) & GRAPH_X_LABELS ) != 0;
			graph->show_Y_labels = ( ((int) wParam) & GRAPH_Y_LABELS ) != 0;
			rc = 0; break;

		case WMP_SET_TITLE_VISIBILITY:
			graph->show_X_title = ( ((int) wParam) & GRAPH_X_TITLE ) != 0;
			graph->show_Y_title = ( ((int) wParam) & GRAPH_Y_TITLE ) != 0;
			rc = 0; break;

		case WMP_SET_AXIS_PARMS:
			axis_parms = (GRAPH_AXIS_PARMS *) wParam;
			/* Turn all on for now */
			graph->suppress_x_grid  = graph->suppress_y_grid  = FALSE;
			graph->suppress_x_minor = graph->suppress_y_minor = FALSE;
			graph->suppress_x_major = graph->suppress_y_major = FALSE;

			if (axis_parms->suppress_grid)   graph->suppress_x_grid = graph->suppress_y_grid = TRUE;
			if (axis_parms->suppress_x_grid) graph->suppress_x_grid = TRUE;
			if (axis_parms->suppress_y_grid) graph->suppress_y_grid = TRUE;

			if (axis_parms->suppress_ticks) {
				graph->suppress_x_minor = graph->suppress_y_minor = TRUE;
				graph->suppress_x_major = graph->suppress_y_major = TRUE;
			}
			if (axis_parms->suppress_x_ticks) graph->suppress_x_major = graph->suppress_x_minor = TRUE;
			if (axis_parms->suppress_y_ticks) graph->suppress_y_major = graph->suppress_y_minor = TRUE;
			if (axis_parms->suppress_major_ticks) graph->suppress_x_major = graph->suppress_y_major = TRUE;
			if (axis_parms->suppress_minor_ticks) graph->suppress_x_minor = graph->suppress_y_minor = TRUE;
			if (axis_parms->suppress_x_major_ticks) graph->suppress_x_major = TRUE;
			if (axis_parms->suppress_y_major_ticks) graph->suppress_y_major = TRUE;
			if (axis_parms->suppress_x_minor_ticks) graph->suppress_x_minor = TRUE;
			if (axis_parms->suppress_y_minor_ticks) graph->suppress_y_minor = TRUE;
				
			rc = 0; break;

		case WMP_SET_SCALES:
			scales = (GRAPH_SCALES *) wParam;
			graph->autox = scales->autoscale_x; graph->forcex = scales->force_scale_x; graph->xmin = scales->xmin; graph->xmax = scales->xmax;
			graph->autoy = scales->autoscale_y; graph->forcey = scales->force_scale_y; graph->ymin = scales->ymin; graph->ymax = scales->ymax;
			graph->autoz = scales->autoscale_z; graph->forcez = scales->force_scale_z; graph->zmin = scales->zmin; graph->zmax = scales->zmax;
			rc = 0; break;

		case WMP_SET_ZFORCE:
			zforce = (GRAPH_ZFORCE *) wParam;
			graph->zforce_x = zforce->x_force;
			graph->zforce_y = zforce->y_force;
			rc = 0; break;

		/* Same as redraw, but update all curve statistics assuming changed */
		case WMP_FULL_REDRAW:
			for (i=0; i<graph->ncurves; i++) {
				cv = graph->curve[i];
				UpdateCurveMinMax(cv);
				cv->modified = FALSE;
			}
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		/* Just invalidate and redraw */
		case WMP_REDRAW:
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			rc = 0; break;

		case WM_SIZE:
			rc = 0; break;

		/* Set a callback procedure to notify if we get a CURSOR message */
		case WMP_CURSOR_CALLBACK:
			graph->cursor_callback.hwnd = (HWND) wParam;
			graph->cursor_callback.wID  = (int) lParam;
			rc = 0; break;

		case WMP_PAINT_CALLBACK:
			graph->paint_callback.hwnd = (HWND) wParam;
			graph->paint_callback.wID  = (int) lParam;
			rc = 0; break;

		case WMP_GRAPH_CONVERT_COORDS:			/* Returns 0 if valid, 1 if invalid */
		{
			GRAPH_CONVERT_COORDS *coords;
			coords = (GRAPH_CONVERT_COORDS *) wParam;
			switch (coords->mode) {
				case GRAPH_FRACTION_TO_SCREEN:	/* Will always return within the graph area */
					coords->ix = get_ix(coords->x, 0.0, 1.0, graph);
					coords->iy = get_iy(coords->y, 0.0, 1.0, graph);
					rc = 0; break;
				case GRAPH_AXES_TO_SCREEN:			/* Will always return within the graph area */
					coords->ix = get_ix(coords->x, graph->xgmin, graph->xgmax, graph);
					coords->iy = get_iy(coords->y, graph->ygmin, graph->ygmax, graph);
					rc = 0; break;
				case GRAPH_SCREEN_TO_FRACTION:	/* ix,iy forced to lie within window area */
					coords->x = (1.0*coords->ix-graph->x_left_margin)/max(1,graph->cxClient-graph->x_left_margin-graph->x_right_margin);
					coords->y = (1.0*(graph->cyClient-coords->iy)-graph->y_left_margin)/max(1,graph->cyClient-graph->y_left_margin-graph->y_right_margin);
					rc = 0; break;
				case GRAPH_SCREEN_TO_AXES:			/* ix,iy forced to lie within window area */
					coords->x = (1.0*coords->ix-graph->x_left_margin)/max(1,graph->cxClient-graph->x_left_margin-graph->x_right_margin);
					coords->y = (1.0*(graph->cyClient-coords->iy)-graph->y_left_margin)/max(1,graph->cyClient-graph->y_left_margin-graph->y_right_margin);
					coords->x = graph->xgmin + (graph->xgmax-graph->xgmin)*coords->x;
					coords->y = graph->ygmin + (graph->ygmax-graph->ygmin)*coords->y;
					rc = 0; break;
				default:									/* Invalid ... just ignore all parameters */
					rc = 1; break;
			}
			coords->within_graph = (coords->ix >= graph->x_left_margin) &&
										  (coords->ix <= graph->cxClient - graph->x_right_margin) &&
										  (coords->iy >= graph->y_right_margin) &&
										  (coords->iy <= graph->cyClient - graph->y_left_margin);
			break;
		}
			
		case WM_COMMAND:
			wID = LOWORD(wParam);								/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);						/* Type of notification		*/
			rc = 1;													/* Assume we don't process */

			/* Switch on events always allowed */
			switch (wID) {
				case IDB_CLEAR:
					if (BN_CLICKED == wNotifyCode) SendMessage(hwnd, WMP_CLEAR, 0, 0);
					rc = 0; break;
				default:
					return DefWindowProc(hwnd, msg, wParam, lParam);
			}

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return rc;
}

#if 0
/* ===========================================================================
-- Class procedure for drawing timeline (considered a MDI window)
--
-- Usage: CALLBACK TimelineWndProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam);
--
-- Inputs: hwnd - handle for the client window
--         umsg - message to be processed
--         wParam, lParam - parameters for message
--
-- Output: message dependent
--
-- Return: 0 if message processed
=========================================================================== */
LRESULT CALLBACK GraphWndProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam) {

	HDC hdc;

#if 0
	LINE_DATA *graph, *graph2;
	LINE_CURVE *cv;
	LINE_POINT *pt;
#endif

	PAINTSTRUCT paintstruct;
	HPEN	hpen, open;
	RECT rect;
	POINTL point;

	int wID, wNotifyCode;
	double x,y,y0,dy, xmin,xmax,xxmin,xxmax, ymin,ymax;
	int i,j, ix,iy, penup, space_need, iline, num_lines;
	BOOL rcode;

	/* For data saving */
#if 0
	static char dir[PATH_MAX]="";							/* Directory for open/save -- keep for multiple calls */
	char pathname[PATH_MAX];
	BOOL first, saw_data;
	int tabs;
	OPENFILENAME ofn;
	FILE *funit;

	static int dflt_colors[MAX_LINES] =
												  {	RGB(255,255,255), RGB(255,0,0),		RGB(0,255,0),	RGB(128,128,255),		/* 0-3 */
	RGB(255,255,0),	RGB(255,0,255),	RGB(0,255,255)	};						/* 4-6 */
#endif

/* Recover the pointer to the graph data (actually set by WM_CREATE if starting */
/*	graph = (LINE_DATA *) GetWindowLongPtr(hwnd, 0); */

	switch (umsg) {
		case WM_CREATE:
#if 0
			graph = calloc(sizeof(*graph),1);
			SetWindowLongPtr(hwnd, 0, (LONG_PTR) graph);
			graph->max_pts      = DFLT_MAX_PTS;			/* Default number of points */
			graph->autox        = TRUE;
			graph->xlength      = -1;
			graph->no_overlap   = TRUE;
			for (i=0; i<MAX_LINES; i++) {
				graph->ymin[i] = graph->ymax[i] = 0.0;
				graph->enable[i]    = TRUE;
				graph->autoy[i]     = TRUE;
				graph->deviation[i] = FALSE;
			}
#endif
			return 0;

		case WM_CLOSE:
			return 0;

		case WM_DESTROY:
#if 0
			if (graph != NULL) {
				for (i=0; i<MAX_LINES; i++) {
					if (graph->lines[i] != NULL) free(graph->lines[i]);
				}
				free(graph);
			}
			SetWindowLong(hwnd, 0, 0);
#endif
			return 0;

#if 0
		case WMP_REDRAW:
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			return 0;
#endif

		case WM_PAINT:
			hdc = BeginPaint(hwnd, &paintstruct);		/* Get DC */
			GetClientRect(hwnd, &rect);					/* left=top=0 right/bottom real */

#if 0
			/* Draw the X lines */
			hpen = CreatePen(PS_SOLID, 1, RGB(128,128,128));
			open = SelectObject(hdc, hpen);
			for (i=0; i<=10; i++) {							/* Draw the X lines */
				ix = (int) (i*rect.right/10.0+0.5);
				MoveToEx(hdc, ix, 0, (LPPOINT) NULL);
				LineTo(hdc, ix, rect.bottom);
			}
			for (i=0; i<=8; i++) {							/* Draw the X lines */
				iy = (int) (i*rect.bottom/8.0+0.5);
				MoveToEx(hdc, 0, iy, (LPPOINT) NULL);
				LineTo(hdc, rect.right, iy);
			}
			SelectObject(hdc, open);
			DeleteObject(hpen);

			/* First, find the full range of the X values since all on common scale */
			xmin = DBL_MAX/4.0; xmax = -DBL_MAX/4.0;
			num_lines = 0;
			for (i=0; i<MAX_LINES; i++) {
				if (! graph->enable[i]) continue;						/* Must be turned on */
				if ( (cv = graph->lines[i]) == NULL || cv->npt == 0) continue;
				ArrayXYMinMax(cv->pts, cv->npt, &xxmin, &xxmax);
				if (xxmin < xmin) xmin = xxmin;
				if (xxmax > xmax) xmax = xxmax;
				num_lines++;
			}
			if (graph->xlength <= 0) graph->autox = TRUE;			/* Infinite length must be auto */
			if (! graph->autox) {											/* Fixed window defeind by xlength */
				if (xmax-xmin > graph->xlength) {
					xmin = xmax-graph->xlength;
				} else {
					xmax = xmin+graph->xlength;
				}
			} else if (graph->xlength > 0 && xmax-xmin > graph->xlength) {
				xmin = xmax-graph->xlength;
			}
			if (xmax <= xmin) xmax = (xmin != 0) ? xmin+fabs(xmin) : 1.0;	/* Avoid divide by zero error below */

			/* Now, go through and autoscale on y and draw */
			iline = 0;
			for (i=0; i<MAX_LINES; i++) {
				if (! graph->enable[i]) continue;						/* Must be turned on */
				if ( (cv = graph->lines[i]) == NULL || cv->npt == 0) continue;
				ymin = graph->ymin[i]; ymax = graph->ymax[i];
				y0 = 0.0;														/* Assumption is no average */
				if (graph->deviation[i]) {									/* Plot only deviation from average? */
					for (j=0; j<cv->npt; j++) y0 += cv->pts[2*j+1];
					y0 /= cv->npt;
				}
				if (graph->autoy[i] || ymin == ymax) {					/* Need to autoscale */
					ArrayXYMinMax(cv->pts+1, cv->npt, &ymin, &ymax);
					ymin -= y0; ymax -= y0;
					if (ymin == ymax) { dy = (ymin==0) ? 1 : 0.1*ymin; ymin -= dy; ymax += dy; }
					PlotAutoScaleZ(ymin, ymax, &ymin, &ymax, NULL, NULL, NULL, graph->zforce_y);
				}

				hpen = CreatePen(PS_SOLID, 1, dflt_colors[i]);
				open = SelectObject(hdc, hpen);
				penup = TRUE;
				for (j=0; j<cv->npt; j++) {
					x = cv->pts[2*j]; y = cv->pts[2*j+1]-y0;
					x = (x-xmin)/(xmax-xmin); y = (ymax-y)/(ymax-ymin);
					if (x < 0.0 || x > 1.0) { penup = TRUE; continue; }
					if (y < 0.0 || x > 1.0) { penup = TRUE; continue; }
					if (graph->no_overlap) y = (1.0*iline+y)/num_lines;
					ix = (int) (x*rect.right + 0.5);
					iy = (int) (y*rect.bottom + 0.5);
					if (penup) {
						MoveToEx(hdc, ix, iy, (LPPOINT) NULL);
						penup = FALSE;
					} else {
						LineTo(hdc, ix, iy);
					}
				}
				iline++;
				SelectObject(hdc, open);
				DeleteObject(hpen);
			}
#endif
			EndPaint(hwnd, &paintstruct);					/* Release DC */
			return 0;

#if 0
		case WMP_CLEAR:
			for (i=0; i<MAX_LINES; i++) {
				if (graph->lines[i] != NULL) {
					free(graph->lines[i]); 
					graph->lines[i] = NULL;
				}
			}
			InvalidateRect(hwnd, NULL, ERASE_BACKGROUND_ON_INVALIDATE);
			return 0;

		case WMP_SETPARMS:
			graph2 = (LINE_DATA *) wParam;									/* Data */
			if (graph2 == NULL) return 0;
			graph->autox        = graph2->autox;
			graph->xlength      = graph2->xlength;
			graph->no_overlap   = graph2->no_overlap;
			for (i=0; i<MAX_LINES; i++) {
				graph->ymin[i]      = graph2->ymin[i];
				graph->ymax[i]      = graph2->ymax[i];
				graph->enable[i]    = graph2->enable[i];
				graph->autoy[i]     = graph2->autoy[i];
				graph->deviation[i] = graph2->deviation[i];
			}
			i = graph->max_pts == graph2->max_pts;
			graph->max_pts = graph2->max_pts;
			if (i) SendMessage(hwnd, WM_CLEAR, 0, 0);
			return 0;

		case WMP_ADD_POINT:
			pt = (LINE_POINT *) wParam;					/* Pointer to structure */
			if (pt->line < 0 || pt->line >= MAX_LINES) return 0;
			if (graph->lines[pt->line] == NULL) {
				space_need = sizeof(LINE_CURVE) + 2*sizeof(*cv->pts)*graph->max_pts;
				graph->lines[pt->line] = calloc(1, space_need);
			}
			if ( (cv = graph->lines[pt->line]) == NULL) return 0;
			if (cv->npt >= graph->max_pts) {					/* When to start scrolling data */
				memmove((void *) cv->pts, (void *) (cv->pts+2), (graph->max_pts-1)*2*sizeof(*cv->pts));
				i = graph->max_pts-1; 
				cv->npt = graph->max_pts;
			} else {
				i = cv->npt++;
			}
			cv->pts[2*i] = pt->x;	cv->pts[2*i+1] = pt->y;
			return 0;
#endif

		case WM_SIZE:
			break;

		case WM_COMMAND:
			wID = LOWORD(wParam);								/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);						/* Type of notification		*/
			rcode = FALSE;											/* Assume we don't process */

#if 0
			/* Switch on events always allowed */
			switch (wID) {
				case IDB_RESET:
					PostMessage(hwnd, WMP_CLEAR, 0, 0);
					break;
				case IDB_SAVE:
					memset(%ofn, 0, sizeof(ofn));						/* Not static, must be set to zeros */
					ofn.lStructSize       = sizeof(OPENFILENAME);
					ofn.hwndOwner         = hwnd;
					ofn.lpstrTitle        = "Save Timeline data";
					ofn.lpstrFilter       = "Text Files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
					ofn.lpstrCustomFilter = NULL;
					ofn.nMaxCustFilter    = 0;
					ofn.nFilterIndex      = 1;
					ofn.lpstrFile         = pathname;				/* Full path */
					ofn.nMaxFile          = sizeof(pathname);
					ofn.lpstrFileTitle    = NULL;						/* Partial path */
					ofn.nMaxFileTitle     = 0;
					ofn.lpstrDefExt       = "txt";
					ofn.lpstrInitialDir   = (*dir=='\0' ? NULL : dir);
					ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
					if (GetSaveFileName(&ofn)) {
						strcpy(dir, pathname);
						dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */
						if ( (funit = fopen(pathname, "w")) == NULL) {
							MessageBox(HWND_DESKTOP, "File open error", "Unable to open file for saving\ntime data. Sorry\n", MB_ICONERROR | MB_OK);
						} else {
							for (i=0; i<graph->max_pts; i++) {
								first = TRUE;
								saw_data = FALSE;
								tabs = 0;
								for (j=0; j<MAX_LINES; j++) {
									if ( (cv = graph->lines[j]) == NULL) continue;
									if (! first) tabs++;
									if (i >= cv->npt) {
										tabs++;									/* Everyone gets a tab for missed data */
									} else {
										for (; tabs>0; tabs--) fputc('\t', funit);
										fprintf(funit, "%f\t%f", cv->pts[2*i],cv->pts[2*i+1]);
										saw_data = TRUE;
									}
									first = FALSE;
								}
								if (! saw_data) break;								/* No one had anymore data so end now */
								fputc('\n', funit);
							}
							fclose(funit);
						}
					}
					rcode = TRUE; break;

				case IDB_PROPERTIES:
					i = graph->max_pts;							/* Automatic reset if you change the number of points */
					DialogBoxParam(MyInstance, "IDD_TIMELINE_PROPERTIES", hwnd, TimelinePropertiesDlgProc, (LPARAM) graph);
					if (graph->max_pts != i) SendMessage(hwnd, WMP_CLEAR, 0, 0);
					break;
				default:
					break;
			}

#endif
		default:
			return DefWindowProc(hwnd, umsg, wParam, lParam);
	}
	return DefWindowProc(hwnd, umsg, wParam, lParam);
}

#endif


/* ===========================================================================
-- Helper routine to scan through data in a curve and update the min/max in
-- case of data changes (or number of points)
=========================================================================== */
static void UpdateCurveMinMax(GRAPH_CURVE *cv) {

	int i;

	/* If not forced, determine array extent now for axis scaling */
	if (! cv->force_scale_x) {
		ArrayMinMax(cv->x, cv->npt, &cv->xmin, &cv->xmax);
		ArrayLogMinMax(cv->x, cv->npt, &cv->logxmin, &cv->logxmax);
	}
	if (! cv->force_scale_y) {
		if (cv->s == NULL) {
			ArrayMinMax(cv->y, cv->npt, &cv->ymin, &cv->ymax);
		} else if (cv->y == NULL) {
			cv->ymin = 0; cv->ymax = 1.0;
		} else {
			cv->ymin = DBL_MAX; cv->ymax = -DBL_MAX;
			for (i=0; i<cv->npt; i++) {
				if (cv->y[i]+fabs(cv->s[i]) > cv->ymax) cv->ymax = cv->y[i]+fabs(cv->s[i]);
				if (cv->y[i]-fabs(cv->s[i]) < cv->ymin) cv->ymin = cv->y[i]-fabs(cv->s[i]);
			}
		}
		ArrayLogMinMax(cv->y, cv->npt, &cv->logymin, &cv->logymax);
	}
	return;
}

/* ===========================================================================
-- Helper routine to scan through data in a mesh and update the min/max in
-- case of data changes (or number of points)
=========================================================================== */
static void	UpdateMeshMinMax(GRAPH_MESH *mesh) {

	int i;
	double x,y, xmin,xmax, ymin,ymax;
	
	/* Don't do anything if the structure is invalid */
	if (mesh->t == NULL) return;
	
	/* If not forced, determine array extent now for axis scaling */
	if (! mesh->force_scale_x) {
		if (mesh->npt <= 0 || mesh->t == NULL) {
			mesh->xmin = mesh->xmax = 0;
			mesh->logxmin = mesh->logxmax = 0.0;
		} else {
			xmin = xmax = mesh->t[0].x0;
			for (i=0; i<mesh->npt; i++) {
				x = mesh->t[i].x0;
				if (x < xmin) xmin = x;
				if (x > xmax) xmax = x;
				x = mesh->t[i].x1;
				if (x < xmin) xmin = x;
				if (x > xmax) xmax = x;
				x = mesh->t[i].x2;
				if (x < xmin) xmin = x;
				if (x > xmax) xmax = x;
			}
			mesh->xmin = xmin;	mesh->xmax = xmax;

			xmin = 100; xmax = -100;
			for (i=0; i<mesh->npt; i++) {
				x = mesh->t[i].x0;
				x = (x == 0) ? -12 : log(fabs(x));
				if (x < xmin) xmin = x;
				if (x > xmax) xmax = x;
				x = mesh->t[i].x1;
				x = (x == 0) ? -12 : log(fabs(x));
				if (x < xmin) xmin = x;
				if (x > xmax) xmax = x;
				x = mesh->t[i].x2;
				x = (x == 0) ? -12 : log(fabs(x));
				if (x < xmin) xmin = x;
				if (x > xmax) xmax = x;
			}
			mesh->logxmin = xmin;	mesh->logxmax = xmax;
		}
	}

	if (! mesh->force_scale_y) {
		if (mesh->npt <= 0 || mesh->t == NULL) {
			mesh->ymin = mesh->ymax = 0;
			mesh->logymin = mesh->logymax = 0.0;
		} else {
			ymin = ymax = mesh->t[0].y0;
			for (i=0; i<mesh->npt; i++) {
				y = mesh->t[i].y0;
				if (y < ymin) ymin = y;
				if (y > ymax) ymax = y;
				y = mesh->t[i].y1;
				if (y < ymin) ymin = y;
				if (y > ymax) ymax = y;
				y = mesh->t[i].y2;
				if (y < ymin) ymin = y;
				if (y > ymax) ymax = y;
			}
			mesh->ymin = ymin;	mesh->ymax = ymax;

			ymin = 100; ymax = -100;
			for (i=0; i<mesh->npt; i++) {
				y = mesh->t[i].y0;
				y = (y == 0) ? -12 : log(fabs(y));
				if (y < ymin) ymin = y;
				if (y > ymax) ymax = y;
				y = mesh->t[i].y1;
				y = (y == 0) ? -12 : log(fabs(y));
				if (y < ymin) ymin = y;
				if (y > ymax) ymax = y;
				y = mesh->t[i].y2;
				y = (y == 0) ? -12 : log(fabs(y));
				if (y < ymin) ymin = y;
				if (y > ymax) ymax = y;
			}
			mesh->logymin = ymin;	mesh->logymax = ymax;
		}
	}

	return;
}

/* ============================================================================
-- Routine to determine if point is out of range of the current graph window
--
-- Usage: BOOL OutOfRange(double x, double y, double xmin, double xmax, double ymin, double ymax);
--
-- Inputs: obvious
--
-- Return: TRUE if point should be excluded
=========================================================================== */
static BOOL OutOfRange(double x, double y, double xmin, double xmax, double ymin, double ymax) {
	double xtmp;

	if (xmax < xmin) { xtmp = xmin; xmin = xmax; xmax = xtmp; }
	if (ymax < ymin) { xtmp = ymin; ymin = ymax; ymax = xtmp; }
	xtmp = 0.02*(xmax-xmin);							/* Allow a 2% window outside graph area */
	xmin -= xtmp; xmax += xtmp;
	xtmp = 0.02*(ymax-ymin);
	ymin -= xtmp; ymax += xtmp;

	return (x < xmin) || (x > xmax) || (y < ymin) || (y > ymax);
}

/* ============================================================================
--     PlotAutoScale - Autoscaling routine for axis ranges.
--
--     A useful routine to automatically scale data range to a "nice" range
--     for plotting.  Takes given min and max values and modifies them to
--     even ranges of a suggested major tick length and associated minor tick
--     length.  For example: 1.37 to 18.33 returns
--                           0.00 to 20.00    dx = 5.0    dx2 = 1.0
--     The lower limit will be forced to 0 if FMIN < FMAX*ZFORCE (zero force)
--     The variable lies in common with default value of 0.3 .
--
--     Usage:     PlotAutoScale(FMIN,FMAX,SMIN,SMAX,DX,DX2,M)
--
--     Inputs:    FMIN  -  Current minimum of data to be plotted
--                FMAX  -  Current maximum of data to be plotted
--                Common array ZFORCE for zero forcing.
--
--     Output:    SMIN  -  Corrected minimum
--                SMAX  -  Corrected maximum
--                DX    -  Recommended major tick mark spacing
--                DX2   -  Recommended minor tick mark spacing
--                M     -  Necessary labelling mode (integer or float, etc.)
--                      <0 => Integer mode
--                      >0 => Float mode with M digits past decimal
--
--     NOTES:     If FMIN = FMAX, an error condition detected.  Output leaves
--                FMIN unchanged and default type values to FMAX. Programs
--                then continues as if these values were given.
============================================================================= */
/* Logic: If normalized range df >= dxlim[i], use the dx,dx2 values */

static double dxlim[]  = { 7.001,3.501, 1.401, 0.99999 };		/* Limits	*/
static double dxuse[]  = { 2.0,  1.0,   0.5,   0.2 };				/* DX			*/
static double dx2use[] = { 0.5,  0.2,   0.1,	  0.05 };			/* DX2		*/

static void PlotAutoScaleZ(double fmin, double fmax, double *SMIN, double *SMAX, double *DX, double *DX2, int *M, double zforce) {

	double	smin, smax, dx, dx2;					/* Local values of return values */
	double	smin_old, smax_old;					/* For iteration until stable		*/
	int		iter=4;									/* Allow 4 iterations to solve */
	int   	m;

	double	temp, df, factor;						/* Local variables*/
	BOOL  	negate  = FALSE;						/* Coordinates not switched	 */
	BOOL		reverse = FALSE;						/* No reverse of SMIN and SMAX */
	int		i;
	long int	i4;

/* Initialize parameters */
	smin = fmin;										/* Output = Input for now */
	smax = fmax;

/* ... Check for bad parameters and modify as necessary */
	if (smin == smax) {						/* Default conditions */
		if (smin < 0)							/* If negative, even interval about 0 */
			smax = -smin;
		else if (smin > 0)					/* Positive, double interval */
			smax = 2*smin;
		else										/* 0, go from [0,1] */
			smax = 1;
	} else if (smin > smax) {				/* Always keep SMIN < SMAX */
		temp = smax;
		smax = smin;
		smin = temp;
		reverse = TRUE;						/* Remember to reverse at end again */
	}

	if (smax < 0) {							/* Also always work with positive #'s */
		temp = smax;							/* Negate and switch the pairs */
		smax = -smin;
		smin = -temp;
		negate = TRUE;							/* Remember at the end */
	}

/* ... Start an iterative loop until we arrive at a stable choice */
	do {
		smin_old = smin;						/* Save current smin/smax	*/
		smax_old = smax;						/* Use to compare at end	*/

/* ... Reset SMIN to 0 if is positive and < ZFORCE*SMAX.  Keeps scales "nice"  */
		if ( (smin > 0) && (smin < zforce*smax)) smin = 0;

/* ... We want to get MAX-MIN between 1 and 10 for the rest of the routines. */
		df = smax - smin;							/* Length of interval 			*/
		factor = 1;									/* No divisors						*/
		while (df < 1)	  {						/* Get it to range of [1,10]; */
			df = 10*df;
			factor /= 10;
		}
		while (df > 10.01) {
			df = df/10;
			factor *= 10;
		}

/* ... Choose the major and minor tick spacing */
		df = floor(df*1000.0+0.49) / 1000.0;	/* Round off to thousandth */
		for (i=0; i<4; i++) {						/* Do the remaining tests (nbd) */
			if (df >= dxlim[i]) {
				dx  = dxuse[i]  * factor;
				dx2 = dx2use[i] * factor;
				break;
			}
		}

/* Now, take the given DX and move the endpoints slightly to make axis
   start and end on a division.  Allow a little play for roundoff. */

		i4 = (long int) (smax/dx + 0.99);	/* New end, now at an even boundary */
		smax = dx*i4;
		i4 = (long int) (smin/dx + 0.01);	/* 1% overrun at this end also */
		temp = dx*i4;
		smin = (smin<0) ? temp-dx : temp ;	/* Correct if we are negative */

	} while ( (fabs(smin_old-smin)+fabs(smax_old-smax))/fabs(smax-smin) > DBL_EPSILON && --iter);

/* And undo and changes from beginning */
	if (negate) {								/* Did we negate to keep positive */
		temp = smax;
		smax = -smin;
		smin = -temp;
	}
	if (reverse) {								/* Had we interchanged coordinates */
		temp = smax;
		smax = smin;
		smin = temp;
	}

/* Determine labelling mode to be suggested */
	if (dx >= 1) {								/* Set integer mode if DX > 1.0 */
		m = -1;
	} else if (dx>=1.0e-9 && dx<1.0e10) { /* # of digits needed to see dx (safe) */
		m = (int) (-log10(dx)+0.99);	/* 0.99 takes care of 0.1 0.01 etc. */
	} else {										/* Otherwise will be scientific */
		m = 0;
	}

	if (SMIN != NULL) *SMIN = smin;		/* And return local values */
	if (SMAX != NULL) *SMAX = smax;
	if (DX   != NULL) *DX   = dx;
	if (DX2  != NULL) *DX2  = dx2;
	if (M    != NULL) *M    = m;
	return;
}


/* =============================================================================
--     MINMAX  - Return minimum and maximum values in an array sent by user.
--     MINMAX2 - Return minimum and maximum values excluding NAN and \infty
--
--     Usage: CALL MINMAX (X,NPT,XMIN,XMAX)
--            CALL MINMAX2(X,NPT,XMIN,XMAX)
--
--     Inputs: X - Array of point
--             NPT - Number of data points
--
--     Output: XMIN - Minimum value in the array
--             XMAX - Maximum value in the array
============================================================================= */
static void ArrayMinMax(double *x, int npt, double *xmin, double *xmax) {

	if (npt <= 0 || x == NULL) {
		*xmax = *xmin = 0.0;
	} else {
		*xmax = *xmin = *x;
		while (npt--) {
			if (*x > *xmax) *xmax = *x;
			if (*x < *xmin) *xmin = *x;
			x++;
		}
	}
	return;
}

static void ArrayLogMinMax(double *x, int npt, double *pxmin, double *pxmax) {
	double xtmp, xmin, xmax;
	int i;

	if (npt <= 0 || x == NULL) {
		xmin = xmax = 0.0;
	} else {
		xmin = 100; xmax = -100;
		for (i=0; i<npt; i++) {
			xtmp = (x[i] == 0) ? -12 : log(fabs(x[i])) ;
			if (xtmp < xmin) xmin = xtmp;
			if (xtmp > xmax) xmax = xtmp;
		}
	}

	/* Return values */
	if (pxmin != NULL) *pxmin = xmin;
	if (pxmax != NULL) *pxmax = xmax;
	return;
}

/* =============================================================================
-- Routine to set up for properly labeling axes ... # of decimals, scientific, etc.
--
-- Usage: int FormatLabels(LABEL_FORMAT *lab, double rmin, double rmax, double dx);
--
-- Inputs: rmin,rmax - range of the data
--         dx        - increment between labels
--         lab       - pointer to a LABEL_FORMAT structure that will be filled in
--
-- Output: lab - fills in the parameters appropriately
--           lab->sci  - TRUE if going to scientific notation
--           lab->iexp - exponent if in scientific notation
--
-- Return: 0
--
-- Notes: (1) Assumes the value of dx is appropriate for rmin/rmax
--        (2) Use EncodeLabel() function to get appropriate string
============================================================================= */
static int FormatLabels(LABEL_FORMAT *lab, double rmin, double rmax, double dx) {
	double tmp;

	tmp = max( fabs(rmin), fabs(rmax) );							/* What is biggest number to be labelled */

	/* Determine if we need to go to scientific notation */
	lab->sci = tmp < 0.9999E-3 || tmp > 0.99999E4;				/* Go to scientific if too many digits */
	if (lab->sci) {			
		lab->iexp  = ( (int) (log10(tmp) + 100) ) - 100;		/* Essentially ceil function */
		lab->scale = pow(10.0, lab->iexp);							/* And numerical scaling factor */
	} else {
		lab->iexp = 0;
		lab->scale = 1.0;
	}

	/* And determine number of decimals required / creating format statement */
	if (dx/lab->scale > 1.0) {										/* Does each interval correspond to integer changes? */
		lab->mx = -1;
		strcpy_m(lab->format, sizeof(lab->format), "%i");
	} else {
		lab->mx = (int) (-log10(dx/lab->scale)+0.999);		/* Number of digits required to see dx */
		sprintf_s(lab->format, sizeof(lab->format), "%%.%if", lab->mx);
	}

	return 0;
}

/* =============================================================================
-- Routine to encode a label value as string for graph
--
-- Usage: char *EncodeLabel(double x, LABEL_FORMAT *lab, char *result, size_t len);
--
-- Inputs: x      - value to be encoded
--         lab    - pointer to a filled in LABEL_FORMAT structure
--         result - pointer to a string large enough to receive formatted 
--                  value (20 should be sufficiently big) or NULL; if NULL,
--                  function will return a static string containing result.
--         len    - dimensioned length of result (for checking)
--
-- Output: Fills in *result if not NULL
--
-- Return: Pointer to result, or pointer to static string if result==NULL
============================================================================= */
static char *EncodeLabel(double x, LABEL_FORMAT *lab, char *result, size_t len) {
	static char szTmp[20];
	if (result == NULL) result = szTmp;

	if (lab->mx < 0) {
		sprintf_s(result, len, lab->format, nint(x/lab->scale));
	} else {
		sprintf_s(result, len, lab->format, x/lab->scale);
	}

	return result;
}
