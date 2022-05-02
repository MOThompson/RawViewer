#ifndef tl_loaded

#define tl_loaded

/* Maximum number of threads that can request a signal when new frame available */
#define	TL_MAX_SIGNALS				(10)
#define	TL_MAX_RING_SIZE			(1000)
#define	TL_IMAGE_ACCESS_TIMEOUT	(200)			/* Never delay for more than 200 ms (ensure 5 fps) for access to image buffers */

#define	TL_CAMERA_MAGIC	0x8A46

FILE *tl_debug;										/* If !NULL, pointer to some logfile for debugging */

#pragma pack(4)
typedef struct _TL_IMAGE {
	int index;											/* Index of this buffer (frame)	*/
	BOOL valid;											/* Is data in buffer valid			*/
	int locks;											/* # buffer locks; 0 => availble	*/
	struct _TL_CAMERA *tl;							/* If we need other info			*/

	int imageID;										/* Unique ID of image (# since start) */
	unsigned short *raw;								/* Buffer with the raw data		*/
	double camera_time;								/* Camera pixel clock timestamp	*/
	__time64_t timestamp;							/* time() value						*/
	SYSTEMTIME system_time;							/* Include millisecond time		*/
	double dB_gain;									/* Master gain in dB					*/
	double ms_expose;									/* ms exposure (also us_expose)	*/
} TL_IMAGE;
#pragma pack()

#pragma pack(4)
#define	TL_RAW_FILE_MAGIC		(0x4A7B92CF)
typedef struct _TL_RAW_FILE_HEADER {
	int magic;									/* ID indicating this is my file (check endien)			*/
	int header_size;							/* Size in bytes of this header (n-8 more)				*/
	int major_version, minor_version;	/* Header version (currently 1.0)							*/
	double ms_expose;							/* Exposure time in ms											*/
	double dB_gain;							/* Gain in dB for camera (RGB don't matter)				*/
	__time64_t timestamp;					/* time() of image capture (relative Jan 1, 1970)		*/
	double camera_time;						/* Image time based on pixel clock (arbitrary zero)	*/
	int year, month, day;					/* Date of capture (human readable, 2024.02.29)			*/
	int hour, min, sec, ms;					/* Time of capture (human readable, 18:00:59.372)		*/
	char camera_model[16];					/* Camera model													*/
	char camera_serial[16];					/* Serial number of camera										*/
	int sensor_type;							/* enum (TL_CAMERA_SENSOR_TYPE) of sensor type			*/
	int color_filter;							/* enum (TL_COLOR_FILTER_ARRAY_PHASE) of color filter	*/
	int width, height;						/* height and width of image									*/
	int bit_depth;								/* Bits resolution in each pixel								*/
	int pixel_bytes, image_bytes;			/* Bytes per pixel and bytes total in image				*/
	double pixel_width, pixel_height;	/* Physical dimensions of pixel (in um)					*/
} TL_RAW_FILE_HEADER;
#pragma pack()


/* Structure use for communicating ring size information in client/server */
#ifndef INCLUDE_TL_DETAIL_INFO
	typedef struct _TL_CAMERA TL_CAMERA;
#else
	// #include "tl_camera_sdk.h"					/* Somehow, this is already loaded */
	#include "tl_camera_sdk_load.h"
	#include "tl_mono_to_color_processing.h"
	#include "tl_color_enum.h"
	#include "tl_mono_to_color_processing_load.h"

	typedef struct _TL_CAMERA {
		int magic;												/* Magic value to indicate valid */
		char ID[32];											/* String ID for the camera		*/
		char name[32];											/* Registered name of camera		*/
		char model[16];										/* Model									*/
		char serial[16];										/* Serial number						*/
		char firmware[1024];									/* Firmware revisions				*/
		FILE *handle;											/* Handle to the camera itself	*/
		enum TL_CAMERA_SENSOR_TYPE sensor_type;		/* Sensor type (must be BAYER)	*/
		enum TL_COLOR_FILTER_ARRAY_PHASE color_filter;	/* Color filter array phase		*/
		float color_correction[9];							/* Color correction matrix			*/
		float white_balance[9];								/* Default white balance matrix	*/

		int bit_depth;											/* Bit depth							*/
		int width, height;									/* Image size							*/
		double pixel_height_um, pixel_width_um;		/* Pixel size in um					*/
		
		BOOL bGainControl;									/* Has master gain control?		*/
		double db_min, db_max;								/* Min/max gain in dB */
		double db_dflt;										/* Default (original) master gain */
		double red_dflt, green_dflt, blue_dflt;		/* Default (original) RGB gains	*/
		
		long long us_expose_min, us_expose_max;		/* Min and max exposure times		*/
		long long us_expose;									/* us exposure time (ms_expose)	*/
		
		BOOL bFrameRateControl;								/* Is framerate control possible	*/
		double fps_min, fps_max;							/* Min and max frame rate			*/
		
		int clock_Hz;											/* Camera clock frequency (or 0)	*/
		
		int pixel_bytes;										/* Number of bytes per pixel		*/
		size_t image_bytes;									/* Number of bytes in an image	*/
		void *color_processor;
		BOOL IsSensorColor;									/* TRUE color, FALSE monochrome	*/
		
		HANDLE image_mutex;									/* Access to modify/use data		*/
		
		TRIGGER_INFO trigger;								/* Trigger details					*/

		/* Current capture conditions */
		double dB_gain;										/* Master gain in dB					*/
		double ms_expose;										/* ms exposure (also us_expose)	*/
		
		/* Ring information */
		int frame_count;										/* Total number of frames read	*/
		int nBuffers,											/* Number of frames in the ring	*/
			 nValid,												/* # frames valid in ring			*/
			 iLast,												/* index of last camera frame		*/
			 iShow;												/* index of frame in rgb/seps		*/

		/* Image memory buffers */
		TL_IMAGE *images;										/* Image raw data and metadata	*/
		int npixels;											/* Number of pixels in a frame	*/
		int nbytes_raw;										/* Number of bytes in each frame */

		int rgb24_imageID;									/* ImageID of current rgb24 data	*/
		int rgb24_nbytes;										/* Number of bytes in rgb24 data	*/
		unsigned char *rgb24;								/* rgb32 image RGBQUAD (4xh*2)	*/
		
		int separations_imageID;							/* ImageID of current separation data	*/
		int nbytes_red, nbytes_green, nbytes_blue;	/* Number of bytes in sub-chans	*/
		unsigned short *red, *green, *blue;				/* Inidividual channels */
		
		HANDLE new_image_signals[TL_MAX_SIGNALS];		/* Handles to event semaphores	*/
		
} TL_CAMERA;
#endif		/* #ifdef INCLUDE_MINIMAL_TL */


/* Global status variables */
BOOL TL_is_initialized;

/* Maximum number of cameras that can be simultaneously opened */
/* While this is public, access to list is not intended to be direct */
/* NULL entries in this list indicate no camera in that slot */
#define	TL_MAX_CAMERAS	(20)
TL_CAMERA *tl_camera_list[TL_MAX_CAMERAS];
int tl_camera_count;

/* Functions */
int TL_Initialize(void);
int TL_SetDebug(BOOL debug);
int TL_Shutdown(void);

/* Find  initializes structures with minimal resources  */
/* Open  opens the handle and initializes buffers */
/* Close releases buffers associated witht he camera */
/* Free  deletes the camera from the list */
TL_CAMERA *TL_FindCamera(char *ID, int *rcode);
int TL_ForgetCamera(TL_CAMERA *tl);
int TL_OpenCamera(TL_CAMERA *tl, int nBuf);
int TL_CloseCamera(TL_CAMERA *tl);

int TL_GetCameraName(TL_CAMERA *tl, char *name, size_t length);
int TL_SetCameraName(TL_CAMERA *tl, char *name);

int TL_GetRingInfo(TL_CAMERA *tl, int *nBuffers, int *nValid, int *iLast, int *iShow);
int TL_SetRingBufferSize(TL_CAMERA *tl, int nBuf);
int TL_ResetRingCounters(TL_CAMERA *tl);

int TL_FindAllCameras(TL_CAMERA **list[]);
int TL_CloseAllCameras(void);
int TL_ForgetAllCameras(void);

TL_CAMERA *TL_FindCameraByIndex(int index);
TL_CAMERA *TL_FindCameraByID(char *ID);
TL_CAMERA *TL_FindCameraByHandle(void *handle);

int TL_EnumCameraList(int *pcount, TL_CAMERA **pinfo[]);

BOOL TL_IsValidCamera(TL_CAMERA *tl);

int TL_GetCameraInfo(TL_CAMERA *tl, CAMERA_INFO *info);

int TL_AddImageSignal(TL_CAMERA *tl, HANDLE signal);
int TL_RemoveImageSignal(TL_CAMERA *tl, HANDLE signal);

int TL_ProcessRGB(TL_CAMERA *tl, int frame);				/* No ties to TL_ProcessRawSeparation */
int TL_ProcessRawSeparation(TL_CAMERA *tl, int frame);	/* No ties to TL_ProcessRGB */

BITMAPINFOHEADER *TL_CreateDIB(TL_CAMERA *tl, int frame, int *rc);

int TL_GetImageInfo(TL_CAMERA *tl, int frame, IMAGE_INFO *info);
int TL_GetImageData(TL_CAMERA *tl, int frame, void **image_data, size_t *length);

int TL_GetSaveFormatFlag(TL_CAMERA *tl);
int TL_GetSaveName(char *path, size_t length, FILE_FORMAT *format);
int TL_SaveImage(TL_CAMERA *tl, char *path, int frame, FILE_FORMAT format);
int TL_SaveBMPImage(TL_CAMERA *tl, char *path, int frame);
int TL_SaveRawImage(TL_CAMERA *tl, char *path, int frame);
int TL_SaveBurstImages(TL_CAMERA *tl, char *pattern, FILE_FORMAT format);

int TL_RenderFrame(TL_CAMERA *tl, int frame, HWND hwnd);

int    TL_GetExposureParms(TL_CAMERA *tl, double *ms_min, double *ms_max);
double TL_SetExposure(TL_CAMERA *tl, double ms_expose);
double TL_GetExposure(TL_CAMERA *tl, BOOL bForceQuery);

double TL_SetFPSControl(TL_CAMERA *tl, double fps);
double TL_GetFPSControl(TL_CAMERA *tl);
double TL_GetFPSActual(TL_CAMERA *tl);

int TL_GetMasterGainInfo(TL_CAMERA *tl, BOOL *bGain, double *db_dflt, double *db_min, double *db_max);
int TL_SetMasterGain(TL_CAMERA *tl, double dB_gain);
int TL_GetMasterGain(TL_CAMERA *tl, double *dB_gain);

#define	TL_IGNORE_GAIN		(-999)
int TL_GetRGBGains(TL_CAMERA *tl, double *red, double *green, double *blue);
int TL_SetRGBGains(TL_CAMERA *tl, double  red, double  green, double  blue);
int TL_GetDfltRGBGains(TL_CAMERA *tl, double *red, double *green, double *blue);

/* Triggering controls (freerun especially) */
TRIG_ARM_ACTION TL_Arm(TL_CAMERA *tl, TRIG_ARM_ACTION action);
int TL_Trigger(TL_CAMERA *tl);
int TL_SetFramesPerTrigger(TL_CAMERA *tl, int frames);
int TL_GetFramesPerTrigger(TL_CAMERA *tl);
TRIGGER_MODE TL_SetTriggerMode(TL_CAMERA *tl, TRIGGER_MODE mode, TRIGGER_INFO *info);
TRIGGER_MODE TL_GetTriggerMode(TL_CAMERA *tl, TRIGGER_INFO *info);

#endif			/* tl_loaded */
