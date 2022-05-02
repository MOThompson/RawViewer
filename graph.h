#ifndef _GRAPH_LOADED
	#define	_GRAPH_LOADED

	int Graph_StartUp(HINSTANCE hThisInst);
	void ArrayMinMax(double *x, int npt, double *xmin, double *xmax);

	#define	IDB_CLEAR						(40001)				/* From popup menu */

	#define	WMP_REDRAW						(WM_APP+1)			/* Redraw, updating only curves marked modified */
	#define	WMP_FULL_REDRAW				(WM_APP+2)			/* Redraw assuming all curves may have changed data */
	#define	WMP_ADD_CURVE					(WM_APP+3)			/* Add WPARAM curve to list of curves */
	#define	WMP_ADD_MESH					(WM_APP+4)			/* Add a list of triangles to draw */
	#define	WMP_ADD_POINT					(WM_APP+5)
	#define	WMP_ADD_FUNCTION				(WM_APP+6)			/* Add WPARAM function to list of functions */
	#define	WMP_GET_NUM_CURVES			(WM_APP+7)			/* Return value is # of curves stored */
	#define	WMP_GET_CURVE					(WM_APP+8)			/* WPARAM is curve # (1 based) and LPARAM must be pointer to variable to get GRAPH_CURVE *cv value */
	#define	WMP_GET_NUM_FUNCTIONS		(WM_APP+9)			/* Return value is # of curves stored */
	#define	WMP_GET_FUNCTION				(WM_APP+10)			/* WPARAM is function # (1 based) and LPARAM must be pointer to variable to get GRAPH_FNC *fnc value */
	#define	WMP_CLEAR						(WM_APP+11)			/* Clear all graphs and functions */
	#define	WMP_CLEAR_FUNCTIONS			(WM_APP+12)
	#define	WMP_CLEAR_MESHES				(WM_APP+13)
	#define	WMP_CLEAR_CURVES				(WM_APP+14)
	#define	WMP_CLEAR_CURVE_BY_POINTER	(WM_APP+15)			/* Delete curve with GRAPH_CURVE *cv in WPARAM */
	#define	WMP_CLEAR_CURVE_BY_INDEX	(WM_APP+16)			/* Delete curve i (i=1 = first) */
	#define	WMP_CLEAR_CURVE_BY_ID		(WM_APP+17)			/* Delete first curve with specified ID */
	#define	WMP_CLEAR_CURVES_KEEP_LAST	(WM_APP+18)			/* Delete all but last curve */
	#define	WMP_SET_CURVE_VISIBILITY	(WM_APP+19)
	#define	WMP_SET_FNC_VISIBILITY		(WM_APP+20)
	#define	WMP_SET_NO_MARGINS			(WM_APP+21)			/* If wParam != 0, sets minimal (1 pixel) margins around graph area */
	#define	WMP_SET_BACKGROUND_COLOR	(WM_APP+22)			/* wParam has the RGB value of the desired background */
	#define	WMP_SET_SCALES					(WM_APP+23)
	#define	WMP_SET_ZFORCE					(WM_APP+24)
	#define	WMP_SET_AXIS_PARMS			(WM_APP+25)
	#define	WMP_SET_MESH_VISIBILITY		(WM_APP+26)
	#define	WMP_LOGMODE						(WM_APP+27)
	#define	WMP_SET_X_TITLE				(WM_APP+28)
	#define	WMP_SET_Y_TITLE				(WM_APP+29)
	#define	WMP_SET_LABEL_VISIBILITY	(WM_APP+30)
	#define	  GRAPH_X_LABELS				(0x01)
	#define	  GRAPH_Y_LABELS				(0x02)
	#define	WMP_SET_TITLE_VISIBILITY	(WM_APP+31)
	#define	  GRAPH_X_TITLE				(0x01)
	#define	  GRAPH_Y_TITLE				(0x02)
	#define	WMP_CURSOR_CALLBACK			(WM_APP+32)			/* Registers routine to get messages on cursor presses */
	#define	WMP_PAINT_CALLBACK			(WM_APP+33)			/* Registers routine to get messages after drawing WM_PAING messages */
	#define	WMP_GRAPH_CONVERT_COORDS	(WM_APP+34)			/* Returns ix,iy <==> x,y value ... see GRAPH_CONVERT_COORDS structure */
	#define	WMP_SET_SLAVE					(WM_APP+35)			/* Mark this as a slave ... careful on close with memory release */

	#define	GRAPH_MAX_FNCS					(10)
	#define	GRAPH_MAX_CURVES				(10)
	#define	GRAPH_MAX_MESHES				(10)
		
	/* Used by WMP_GET_SCREEN_COORDS to translate give x,y into ix,iy coordinates */
	typedef struct _GRAPH_CONVERT_COORDS {
		enum {GRAPH_FRACTION_TO_SCREEN=0, GRAPH_AXES_TO_SCREEN=1, GRAPH_SCREEN_TO_FRACTION=2, GRAPH_SCREEN_TO_AXES=3} mode;
		double x,y;
		int ix,iy;
		BOOL within_graph;
	} GRAPH_CONVERT_COORDS;

	typedef struct _GRAPH_TRIANGLE {			/* Define a triangle as a collection of 3 points */
		double x0,y0, x1,y1, x2,y2;
	} GRAPH_TRIANGLE;

	typedef struct _GRAPH_ZFORCE {			/* Value when to force lower limit to zero on axis */
		double x_force, y_force;				/* Typical value is 0.3 for default GENPLOT */
	} GRAPH_ZFORCE;

	typedef struct _GRAPH_SCALES {
		BOOL autoscale_x,   autoscale_y,   autoscale_z;		/* Autoscale based on the data */
		BOOL force_scale_x, force_scale_y, force_scale_z;	/* If TRUE, use xmin,xmax or ymin,ymax	*/
		double xmin, xmax;											/* specific min/max if not auto */
		double ymin, ymax;											/* specific min/max if not auto */
		double zmin, zmax;											/* specific min/max if not auto */
	} GRAPH_SCALES;
	
	typedef struct _GRAPH_AXIS_PARMS {
		BOOL suppress_grid;									/* If TRUE, suppress both grids (ignore suppress_x_grid, suppress_y_grid) */
		BOOL suppress_x_grid;								/* If ! suppress_grid, controls the X grid independently */
		BOOL suppress_y_grid;								/* If ! suppress_grid, controls the Y grid independently */
		BOOL suppress_ticks;									/* Suppress all tick marks if TRUE */
		BOOL suppress_x_ticks;
		BOOL suppress_y_ticks;
		BOOL suppress_major_ticks;							/* Suppress X and Y major tick marks if TRUE */
		BOOL suppress_x_major_ticks;
		BOOL suppress_y_major_ticks;
		BOOL suppress_minor_ticks;							/* Suppress X and Y minor tick marks if TRUE */
		BOOL suppress_x_minor_ticks;
		BOOL suppress_y_minor_ticks;
	} GRAPH_AXIS_PARMS;
	
	typedef struct _GRAPH_CURVE {
		int ID;													/* ID (can be used to selectively delete	*/
		char legend[64];										/* Legend text										*/
		BOOL visible;											/* Should curve be drawn?						*/
		BOOL free_on_clear;									/* If TRUE, x,y and s and this structure released when cleared */
		BOOL modified;											/* Data has been modified ... internal data structures should be updated */
		BOOL draw_x_axis, draw_y_axis;					/* If TRUE, draw solid line at x=0 or y=0 */
		BOOL autoscale_x, force_scale_x;					/* auto ==> scan data for range,  force ==> don't make nice */
		BOOL autoscale_y, force_scale_y;					/* auto ==> scan data for range,  force ==> don't make nice */
		BOOL autoscale_z, force_scale_z;					/* auto ==> scan data for range,  force ==> don't make nice */
		/* Values below are either manual when not autoscaling, or automatically updated with data if autoscaling */
		double xmin,xmax, logxmin, logxmax;				/* Values used as scales */
		double ymin,ymax, logymin, logymax;				/* Values used as scales */
		double zmin,zmax, logzmin, logzmax;				/* Values used as scales */
		/* End of optional values */
		double *x, *y, *z;									/* Potential x,y,z values */
		double *s;												/* Potential uncertainty in y */
		int *pt_rgb;											/* If !NULL, individual point colors */
		int npt;													/* Number of valid points in x,y,z arrays */
		int nptmax;												/* Unused by code ... caller may use to track allocated size */
		int rgb;
		int isize;												/* If 0, use default.  If !0, draws square +-n pixels */
		BOOL master;											/* If TRUE, will be used to define graph xmin/xmax and axes */
		int flags;												/* Set various flags (see below) */
	} GRAPH_CURVE;
#define	CURVE_FLAG_POINTS					(0x01)		/* Do only points (also default if flags == 0) */
#define	CURVE_FLAG_LINES					(0x02)		/* Do solid connected lines rather than points */
#define	CURVE_FLAG_LINES_AND_POINTS	(0x03)		/* Do solid connected lines and points */
	
	typedef struct _GRAPH_MESH {
		int ID;													/* ID (can be used to selectively delete	*/
		char legend[64];										/* Legend text										*/
		BOOL visible;											/* Should curve be drawn?						*/
		BOOL free_on_clear;									/* If TRUE, triangle points and this structure released when cleared */
		BOOL modified;											/* Data has been modified ... internal data structures should be updated */
		BOOL draw_x_axis, draw_y_axis;					/* If TRUE, draw solid line at x=0 or y=0 */
		BOOL force_scale_x, force_scale_y;				/* If TRUE, use xmin,xmax or ymin,ymax	*/
		BOOL autoscale_x, autoscale_y;					/* Autoscale based on the data */
		double xmin,xmax, ymin,ymax;						/* If override, values used as scales	(note - may be changed if not forced) */
		double zmin,zmax;										/* Range for Z if defined */
		double logxmin, logxmax, logymin, logymax;	/* In case going to log mode */
		GRAPH_TRIANGLE *t;									/* List of triangles (3x x,y pairs) */
		int npt;
		int rgb;
	} GRAPH_MESH;
	
	typedef struct _GRAPH_FNC {
		int ID;													/* ID (can be used to selectively delete	*/
		char legend[64];										/* Legend text										*/
		BOOL visible;											/* Should curve be drawn?						*/
		BOOL free_on_clear;									/* If TRUE, free structure on clear			*/
		BOOL draw_x_axis, draw_y_axis;					/* If TRUE, draw solid line at x=0 or y=0 */
		BOOL force_scale_x, force_scale_y;				/* If TRUE, use xmin,xmax or ymin,ymax		*/
		double xmin,xmax, ymin,ymax;						/* If override, values used as scales		*/
		int npt;
		double rmin, rmax;									/* Range for function additions				*/
		void *args;												/* Arguments for function call				*/
		double (*fnc)(double x, void *args);
		int rgb;													/* If 0, default colors will be used		*/
	} GRAPH_FNC;
	
	typedef enum _GRAPH_MODE {GR_LINEAR=0, GR_LOGLIN=1, GR_LOGLOG=2} GRAPH_MODE;
	
	typedef struct _GRAPH_DATA {
		HWND hwnd;													/* Handle for messages */
		BOOL slave_process;										/* This is a slave structure ... no autofree() */
		GRAPH_MODE mode;
		BOOL autox, autoy, autoz;								/* Autoscale the axes based on data */
		BOOL forcex, forcey, forcez;							/* Force scales and don't choose nice values for axes */
		double xmin,xmax, ymin,ymax, zmin, zmax;			/* Values of range when not autox and autoy */
		double zforce_x, zforce_y, zforce_z;				/* Zero-forcing range */
		BOOL suppress_x_grid, suppress_y_grid;				/* Suppress the internal grid on the graph */
		BOOL suppress_x_minor, suppress_x_major;			/* Suppress x axis tick marks */
		BOOL suppress_y_minor, suppress_y_major;			/* Suppress x axis tick marks */
		BOOL show_X_labels, show_Y_labels;					/* Should we draw the labels on X and Y axes */
		BOOL show_X_title, show_Y_title;						/* Should we draw the title on X and Y axes */
		BOOL no_margins;											/* Show only minimal margins */
		int  background_color;									/* Background color */
		char x_title[64], y_title[64];						/* Are there titles on the X and Y axes */
		
		/* Data from the last WM_PAINT */
		double xgmin,xgmax, ygmin,ygmax;						/* min/max for the labelled graph */
		int x_left_margin, x_right_margin;					/* Pixels to left/right of graph for labels */
		int y_left_margin, y_right_margin;					/* Pixels below/above of graph for labels */
		int cxClient, cyClient;									/* Size of the client area for graphs */

		GRAPH_FNC *function[GRAPH_MAX_FNCS];
		int nfncs;
		
		GRAPH_CURVE *curve[GRAPH_MAX_CURVES];
		int ncurves;
		
		GRAPH_MESH *meshes[GRAPH_MAX_MESHES];
		int nmeshes;

		/* Structures for cursor and paint callback routines */
		struct {				
			HWND hwnd;		
			int wID;
		} cursor_callback,										/* on any cursor presses */
		  paint_callback;											/* called after drawing complete with HDC */

	} GRAPH_DATA;

	/* Information returned as the wParam in the cursor callback function */
	typedef struct _GRAPH_CURSOR_INFO {
		HWND hwnd;						/* Handle of calling window */
		int msg;							/* Message (WM_LBUTTONDOWN, ...) */
		POINT point;					/* Actual point of cursor (screen coord's) */
		int cxClient, cyClient;		/* Size of the client window */
		int xpixel, ypixel;			/* x,y pixel relative to lower left of window */
		double xfrac,yfrac;			/* Fractional position within the graph area */
		double x,y;						/* Actual x,y based on first graph scales */
		BOOL ingraph;					/* Is the position within the actual graph? */
	} GRAPH_CURSOR_INFO;

#endif		/* #ifndef _GRAPH_LOADED */