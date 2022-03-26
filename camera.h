#ifndef CAMERA_LOADED

#define	CAMERA_LOADED

/* Note: The structures here are also used by the client/server */
typedef enum _CAMERA_TYPE {CAMERA_UNKNOWN=0, CAMERA_DCX=1, CAMERA_TL=2} CAMERA_TYPE;

#pragma pack(4)
typedef struct _IMAGE_INFO {			/* NOTE: interpretation of values depends on camera type */ 
	CAMERA_TYPE type;						/* 0=unknown, 1=DCX, 2=TL					*/
	uint32_t frame;						/* Which frame within the ring buffer	*/
	time_t timestamp;						/* Standard UNIX time of image capture	*/
	double camera_time;					/* Time of capture from camera clock - units are seconds but epoch undefined */
	uint32_t width, height;				/* Image width and height					*/
	uint32_t memory_pitch;				/* Bytes between each rows (allocate memory pitch*height) */
	double exposure;						/* Current exposure (ms)					*/
	double gamma;							/* Gamma value									*/
	double master_gain;					/* Gains in non-linear range [0,100]	*/
	double red_gain, green_gain, blue_gain;
	uint32_t color_correct_mode;		/* Camera dependent							*/
												/* For DCX, 0,1,2,4,8 corresponding to disable, enable, BG40, HQ, IR Auto */
	double color_correct_strength;	/* Camera dependent							*/
} IMAGE_INFO;
#pragma pack()

/* Structure used by ZooCam_client to query camera characteristics */
#pragma pack(4)
typedef struct _CAMERA_INFO {
	CAMERA_TYPE type;						/* 0=unknown, 1=DCX, 2=TL		*/
	char name[32];							/* "name" of specific camera	*/
	char model[32];						/* Manufacturer's camera model */
	char manufacturer[32];				/* Sensor manufacturer */
	char serial[32];						/* Sensor serial number */
	char version[32];						/* Sensor version */
	char date[32];							/* Sensor firmware date */
	uint32_t width, height;				/* Maximum image size */
	BOOL bColor;							/* Is camera color? */
	double x_pixel_um, y_pixel_um;	/* Pixel size in um */
} CAMERA_INFO;
#pragma pack()

/* Values for the enumeration must match order of radio buttons */
#define	NUM_TRIGGER_MODES		(5)
typedef enum _TRIGGER_MODE     { TRIG_FREERUN=0, TRIG_SOFTWARE=1, TRIG_EXTERNAL=2, TRIG_SS=3, TRIG_BURST=4 } TRIGGER_MODE;
typedef enum _TRIGGER_POLARIY  { TRIG_EXT_NOCHANGE=0, TRIG_EXT_POS=1, TRIG_EXT_NEG=2, TRIG_EXT_UNSUPPORTED=3} TRIGGER_POLARITY ;

/* Which triggers can be enabled for a camera (set by the Open_Camera routines) */
typedef struct _TRIGGER_CAPABILITIES {
	BOOL bFreerun:1;
	BOOL bSoftware:1;
	BOOL bExternal:1;
	BOOL bSingleShot:1;
	BOOL bBurst:1;
	BOOL bArmDisarm:1;
	BOOL bForceExtTrigger:1;
	BOOL bMultipleFramesPerTrigger:1;
	BOOL bExtTrigSlope:1;
} TRIGGER_CAPABILITIES;

/* Only used in Camera_ArmDisarm() functions */
typedef enum _TRIG_ARM_ACTION { TRIG_ARM_QUERY=0, TRIG_ARM=1, TRIG_DISARM=2, TRIG_ARM_UNKNOWN=3} TRIG_ARM_ACTION;

/* Structure for details about trigging (mostly external trigger parms) */
#pragma pack(4)
typedef struct _TRIGGER_INFO {
	TRIGGER_MODE mode;						/* Current triggering mode (query only) */
	TRIGGER_POLARITY ext_slope;			/* External trigger polarity  */
	TRIGGER_CAPABILITIES capabilities;	/* Trigger capabilities (read only) */
	BOOL bArmed;								/* If true, trigger is armed (read only) */
	int frames_per_trigger;					/* Frames per trigger (in SOFTWARE / HARDWARE modes) */
	int msWait;									/* ms to wait for previous trig to complete before switch	*/
	int nBurst;									/* number of images to capture on software/external trigger */
} TRIGGER_INFO;
#pragma pack()

#define	NUM_COLOR_CORRECT_MODES	(5)
typedef enum _COLOR_CORRECT { COLOR_DISABLE=0, COLOR_ENABLE=1, COLOR_BG40=2, COLOR_HQ=3, COLOR_AUTO_IR=4 } COLOR_CORRECT;

/* Flags identifying allowed save formats for a given camera */
#define	FL_BMP			(0x01)			/* Bitwise flags for identifying availability for any given camera */
#define	FL_RAW			(0x02)
#define	FL_JPG			(0x04)
#define	FL_PNG			(0x08)

/* Enum for file type in saves */
typedef enum _FILE_FORMAT { FILE_DFLT = 0, FILE_BMP=1, FILE_RAW=2, FILE_JPG=3, FILE_PNG=4 } FILE_FORMAT;

/* Structure use for communicating ring size information in client/server */
#pragma pack(4)
typedef struct _RING_INFO {
	int nBuffers;								/* Number of buffers in the ring */
	int nValid;									/* Number of frames valid since last reset */
	int iLast;									/* index of last buffer used (from events) */
	int iShow;									/* index of currently displayed frame */
} RING_INFO;
#pragma pack()

/* ===========================================================================
==============================================================================
-- Prototypes below are hidden for zoocam_client code.
==============================================================================
=========================================================================== */

#ifndef ZOOM_CLIENT							/* Don't load if the client code */

typedef struct _WND_INFO WND_INFO;

int Camera_GetCameraInfo(WND_INFO *wnd, CAMERA_INFO *info);

int    Camera_GetExposureParms(WND_INFO *wnd, double *ms_low, double *ms_high, double *ms_incr);
double Camera_SetExposure(WND_INFO *wnd, double ms_expose);
double Camera_GetExposure(WND_INFO *wnd);

double Camera_SetGamma(WND_INFO *wnd, double  gamma);
double Camera_GetGamma(WND_INFO *wnd);

typedef enum {M_CHAN=0, R_CHAN=1, G_CHAN=2, B_CHAN=3} GAIN_CHANNEL;
typedef enum {IS_SLIDER, IS_VALUE} ENTRY_TYPE;
int Camera_SetGains(WND_INFO *wnd, GAIN_CHANNEL channel, ENTRY_TYPE entry, double value);
int Camera_GetGains(WND_INFO *wnd, double values[4], double slider[4]);
int Camera_ResetGains(WND_INFO *wnd);

double Camera_GetFPSActual(WND_INFO *wnd);
double Camera_SetFPSControl(WND_INFO *wnd, double  rate);
double Camera_GetFPSControl(WND_INFO *wnd);

COLOR_CORRECT Camera_GetColorCorrection(WND_INFO *wnd, double *rval);
COLOR_CORRECT Camera_SetColorCorrection(WND_INFO *wnd, COLOR_CORRECT mode, double rval);

TRIG_ARM_ACTION Camera_Arm(WND_INFO *wnd, TRIG_ARM_ACTION action);
int Camera_Trigger(WND_INFO *wnd);
int Camera_GetFramesPerTrigger(WND_INFO *wnd);
int Camera_SetFramesPerTrigger(WND_INFO *wnd, int frames);
TRIGGER_MODE Camera_SetTriggerMode(WND_INFO *wnd, TRIGGER_MODE mode, TRIGGER_INFO *info);
TRIGGER_MODE Camera_GetTriggerMode(WND_INFO *wnd, TRIGGER_INFO *info);

int Camera_GetRingInfo(WND_INFO *wnd, RING_INFO *info);
int Camera_SetRingBufferSize(WND_INFO *wnd, int nBuf);
int Camera_ResetRingCounters(WND_INFO *wnd);

int Camera_GetImageData(WND_INFO *wnd, int frame, void **image_data, int *length);
int Camera_GetImageInfo(WND_INFO *wnd, int frame, IMAGE_INFO *info);

int Camera_GetPreferredImageFormat(WND_INFO *wnd);
int Camera_GetSaveFormatFlag(WND_INFO *wnd);
int Camera_RenderFrame(WND_INFO *wnd, int frame, HWND hwnd);

int Camera_SaveImage(WND_INFO *wnd, int frame, char *path, FILE_FORMAT format);
int Camera_SaveAll(WND_INFO *wnd, char *pattern, FILE_FORMAT format);

#endif			/* #ifndef ZOOM_CLIENT */

#endif			/* #ifndef CAMERA_LOADED */
