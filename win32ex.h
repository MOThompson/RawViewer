#ifndef _WIN32EX_H_LOADED
#define _WIN32EX_H_LOADED

/******************************************************************************
	win32ex.h - API for add-ons to standard win32 API
******************************************************************************/

/* Extensions to standard libraries */
errno_t strcpy_m(char *dest, size_t dest_size, const char *src);
errno_t strcat_m(char *dest, size_t dest_size, const char *src);

/* Center a dialog window within its parent */
#define	DlgCenterWindow(hdlg)	(DlgCenterWindowEx((hdlg), NULL))
void DlgCenterWindowEx(HWND hdlg, HWND parent);

/* Show msgs from a dialog box call */
char *GetMsgInfo(char *routine, UINT msg, WPARAM wparam, LPARAM lparam);
void ShowMsgInfo(char *routine, UINT msg, WPARAM wparam, LPARAM lparam);

void DisableCloseButton(HWND hwnd);

/* Write RTF text to a window (can be big and colorful */
int SetDlgRTFText(HWND hdlg, int control, char *msg, int fSize, int colorindex);

/******************************************************************************
   Macros
******************************************************************************/
#define GetDlgItemCheck( hwnd, ID )			( SendDlgItemMessage( hwnd, ID, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
#define SetDlgItemCheck( hwnd, ID, mark )	( SendDlgItemMessage( hwnd, ID, BM_SETCHECK, ( mark ? BST_CHECKED : BST_UNCHECKED ), 0 ) )
#define ClickDlgItem( hwnd, ID )				( SendMessage( hdlg, WM_COMMAND, MAKEWPARAM( ID, BN_CLICKED ), 0 ) )
#define EnableDlgItem(hdlg, wID, flag)		( EnableWindow(GetDlgItem(hdlg, wID), flag) )
#define ShowDlgItem(hdlg, wID, flag)		( ShowWindow(GetDlgItem(hdlg, wID), flag) )

/* **************************************************************************
   Routines to determine which radio button is checked
************************************************************************** */
BOOL SetRadioButton(HWND hdlg, int nID_first, int nID_last, int nID_want);
int  GetRadioButton(HWND hdlg, int nID_first, int nID_last);
BOOL SetRadioButtonIndex(HWND hdlg, int nID_first, int nID_last, int index);
int  GetRadioButtonIndex(HWND hdlg, int nID_first, int nID_last);

/******************************************************************************
	Routines to mimic strtol() and strtod() but return with *endptr pointing to
   first non-whitespace after number token
******************************************************************************/
long strtol_nw( const char *nptr, char **endptr, int base );
double strtod_nw( const char *nptr, char **endptr );

/* Routines to read/write the ini files */
BOOL WritePrivateProfileStr(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPCTSTR lpString, LPCTSTR lpFileName);
BOOL WritePrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int ival, LPCTSTR lpFileName);
BOOL WritePrivateProfileDouble(LPCTSTR lpAppName, LPCTSTR lpKeyName, double rval, LPCTSTR lpFileName);

BOOL ReadPrivateProfileStr(LPCTSTR lpAppName, LPCTSTR lpKeyName, char *str, size_t len, LPCTSTR lpFileName);
BOOL ReadPrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int *ival, LPCTSTR lpFileName);
BOOL ReadPrivateProfileDouble(LPCTSTR lpAppName, LPCTSTR lpKeyName, double *rval, LPCTSTR lpFileName);


/* ===========================================================================
-- Routines and definitions for working with Combo Boxes.  Allows
-- storing an int or pointer with the value and setting or recovering
-- those values.
=========================================================================== */
typedef struct _CB_INT_LIST {
	char *id;
	int value;
} CB_INT_LIST;

typedef struct _CB_PTR_LIST {
	char *id;
	void *value;
} CB_PTR_LIST;

#define	CB_COUNT(list)		(sizeof(list)/sizeof(list[0]))

int ComboBoxClearList(HWND hdlg, int wID);
int ComboBoxClearSelection(HWND hdlg, int wID);

int ComboBoxGetIndex(HWND hdlg, int wID);
int ComboBoxSetByIndex(HWND hdlg, int wID, int index);

int ComboBoxFillIntList(HWND hdlg, int wID, CB_INT_LIST *list, int n);
int ComboBoxAddIntItem(HWND hdlg, int wID, char *text, int value);
int ComboBoxGetIntValue(HWND hdlg, int wID);
int ComboBoxSetByIntValue(HWND hdlg, int wID, int ival);

int ComboBoxFillPtrList(HWND hdlg, int wID, CB_PTR_LIST *list, int n);
int ComboBoxAddPtrItem(HWND hdlg, int wID, char *text, void *value);
void *ComboBoxGetPtrValue(HWND hdlg, int wID);
int ComboBoxSetByPtrValue(HWND hdlg, int wID, void *ival);


/******************************************************************************
	Routines for getting and putting numbers into dialog boxes.

	Hides the (embarassing) fact that Windows dialogs only pass strings.
******************************************************************************/

/* ============================================================================
   Routine to encode and put an integer value into a dialog box by user format.
  
   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg
           fmt  - char string formatting information.  If NULL, will use %g
           value - actual value to encode
  
   Output: Sets text of specified dialog control
  
   Return: nothing
============================================================================ */
void SetDlgItemFmtInt(HWND hdlg, int wID, char *fmt, int value);
void PutInt(HWND hdlg, int wID, char *fmt, int value);


/* ============================================================================
   Routine to encode and put a real value into a dialog box by user format.
  
   Usage: void PutDouble(HWND hdlg, int wID, char *fmt, double value)
  
   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg
           fmt  - char string formatting information.  If NULL, will use %g
           value - actual value to encode
  
   Output: Sets text of specified dialog control
  
   Return: nothing
============================================================================ */
void PutDouble(HWND hdlg, int wID, char *fmt, double value);
void SetDlgItemDouble(HWND hdlg, int wID, char *fmt, double value);


/* Old formats */
int GetInt(HWND hdlg, int wID);							/* Identical with GetDlgItemIntEx() */
double GetDouble(HWND hdlg, int wID);					/* Identical with GetDlgItemDouble() */


/* New format */
int GetDlgItemIntEx(HWND hdlg, int wID);
double GetDlgItemDouble(HWND hdlg, int wID);
double GetDoubleNC(HWND hdlg, int wID);

/* ============================================================================
   Routine to query the text in a dialog control and interpret as a float.
   Checks validity of the text, potentially returning invalid codes

   Usage: double = GetDoubleEx(HWND hdlg, int wID, BOOL *invalid, int *BadID)

   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg
  
   Output: *invalid - set TRUE if !NULL and control has invalid # text
           *BadID   - set to wID if !NULL and control has invalid # text
                      otherwise, both are left unchanged
  
   Return: Best guess of value in the control
  
   Notes: This function only modifies *invalid and *BadID if there was a
          problem.  Usage in this form allows a string of GetDoubleEx() to
          be called with only a single error check at the end.  If there
          were any errors, the last bad dialog box will be identified.
============================================================================ */
double GetDoubleEx(HWND hdlg, int wID, BOOL *invalid, int *BadID);


/* ============================================================================
	For use with GetDoubleEx after check on invalid
============================================================================ */
void AskForDoubleAgain( HWND hdlg, int id );


/* ============================================================================
   Routine to 'read' a number from a dialog control, constrain it, and then
   rewrite it with specified format.
  
   Usage: double GetConstrainedDouble(HWND hdlg, int wID, BOOL positive, char *fmt,
                                      double low, double high, double dflt)
   Usage: int GetConstrainedInt(HWND hdlg, int wID, BOOL positive, char *fmt,
                                int low, int high, int dflt)
  
   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg
           Positive - TRUE if number must be positive
           fmt  - format to use to re-encode the number into the dialog control
           low  - lower limit value permitted
           high - upper limit value permitted
           dflt - default value if completely invalid
  
   Output: Always rewrites the control text with the specified format
  
   Returns: dflt if out of range, else best guess of value in the control
============================================================================ */
double GetConstrainedDouble(HWND hdlg, int wID, BOOL positive, char *fmt, double low, double high, double dflt);
int GetConstrainedInt(HWND hdlg, int wID, BOOL positive, char *fmt, int low, int high, int dflt);


/* ============================================================================
   Like GetConstrainedDouble(), only if value is above high, it "sticks" to
   high, and if value is less than low, it "sticks" to low
  
   Usage: double GetDoubleInRange(HWND hdlg, int wID, char *fmt,
                                  double low, double high)
  
   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg
           fmt  - format to use to re-encode the number into the dialog control
           low  - lower limit value permitted
           high - upper limit value permitted
  
   Output: Always rewrites the control text with the specified format
  
   Returns: best guess of value in control, but _always_ some number within
            the permitted range [low,high]
============================================================================ */
double GetDoubleInRange(HWND hdlg, int wID, char *fmt, double low, double high);


/* take input from dialog and snap to an integer within given range [low,high] */
int GetIntInRangeEx( HWND hdlg, int wID, char *fmt, int low, int high );
#define GetIntInRange( hdlg, wID, low, high ) ( GetIntInRangeEx( (hdlg), (wID), "%d", (low), (high) ) )



/******************************************************************************
	Routines to mimic MessageBox() but spawn the dialog as a separate thread
	so routines can post error messages and not wait for a response
******************************************************************************/
int SpawnMessageBox(  HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType );



/******************************************************************************
	Error handling routines
******************************************************************************/

/* Freeze thread, put up a message box, and then exit the process */
void AbortOnFatalError( char *rname, char *msg );

double UpNice(double dx);
double DownNice(double dx);

/* Convert SYSTEMTIME structure to standard UNIX time */
__time64_t TimeFromSystemTime(const SYSTEMTIME *pTime);

#endif		/* #ifndef _WIN32EX_H_LOADED */
