/******************************************************************************
	win32ex.c - add-ons to standard win32 API
******************************************************************************/

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE_					/* Always require POSIX standard */
#ifndef	_CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS	/* Turn off for this routine (intentional use of strncpy) */
#endif

/***** Standard Include Files *****/
/* from standard C library */
#include <stddef.h>				  /* for defining several useful types and macros */
#include <stdio.h>				  /* for performing input and output */
#include <stdlib.h>				  /* for performing a variety of operations */
#include <string.h>				  /* for manipulating several kinds of strings */
#include <float.h>
#include <errno.h>
#include <math.h>
#include <time.h>

/* from standard Windows library */
#define STRICT						  /* define before including windows.h for stricter type checking */
#include <windows.h>				  /* master include file for Windows applications */
#include <richedit.h>
#undef _POSIX_
	#include <process.h>				  /* for process control fuctions (e.g. threads)        */
#define _POSIX_

/***** Local Include Files *****/
#include "win32ex.h"				  /* Extensions to Win32 API */

/***** Constants *****/

/***** Macros *****/

/***** Typedefs *****/

/***** Imported Functions *****/

/***** Exported Functions *****/

/***** Private Functions *****/

/***** Imported Variables *****/

/***** Exported Variables *****/

/***** Private Variables *****/



/* ===========================================================================
-- Secure version of string copy that will not overflow the buffer.
-- Unlike strcpy_s, this version simply truncates src if necessary to
-- fit and guarentees that dest will be null terminated as long as
-- the size is greater than 0
--
-- Usage: int strcpy_m(char *dest, size_t count, const char *src);
--
-- Inputs: dest  - pointer to destination string location
--         count - size of dest
--         src   - pointer to source string
--
-- Output: Copies up to count-1 characters from src to dest and terminates
--         dest with a null character (guarenteed)
--
-- Return: 0 if successful, !=0 otherwise
--           EINVAL if dest or src are NULL
--           ERANGE if count is <= 0
=========================================================================== */
errno_t strcpy_m(char *dest, size_t count, const char *src) {

	/* Even on error, null terminate destination string if possible */
	if (dest != NULL && count > 0) *dest = '\0';

	/* Validate parameters */
	if (dest == NULL || src == NULL) return EINVAL;
	if (count <= 0) return ERANGE;

	/* Copy but no more than count-1 characters */
	while (--count > 0 && *src) *(dest++) = *(src++);
	*dest = '\0';
	return 0;
}

/* ===========================================================================
-- Secure version of string concatenate that will not overflow the buffer.
-- Unlike strcat_s, this version simply truncates src if necessary to
-- fit and guarentees that dest will be null terminated as long as
-- the size is greater than 0 and something is copied into the space
--
-- Usage: int strcat_m(char *dest, size_t count, const char *src);
--
-- Inputs: dest  - pointer to destination string location
--         count - size of dest
--         src   - pointer to source string
--
-- Output: Appends dest to src, but only up to a total of count-1 chars;
--         terminates dest with a null character unless error
--
-- Return: 0 if successful, !=0 otherwise
--         EINVAL if dest or src are NULL
--         ERANGE if count is <= 0
=========================================================================== */
errno_t strcat_m(char *dest, size_t count, const char *src) {

	/* Validate parameters */
	if (dest == NULL || src == NULL) return EINVAL;
	if (count < 1) return ERANGE;

	/* Point to the end of dest, but never further than count */
	while (*dest != '\0' && count > 0) { dest++; count--; }
	if (count == 0) return ERANGE;

	/* We are now pointed at the end of the string, so just copy new string */
	while (--count > 0 && *src) *(dest++) = *(src++);
	*dest = '\0';
	return 0;
}


/* ===========================================================================
-- Potentially useful messages from very early code
=========================================================================== */
#if 0

/**************************************************************************
 *
 *  Name       : DlgSetSysMenu(hDlg)
 *
 *  Description: Sets only the Move and Close items of the system menu
 *
 *  Concepts:  Any dialog box is free to call this routine, to edit
 *             which menu items will appear on its System Menu pulldown.
 *
 *  API's      :  WinWindowFromID
 *                SendMessage
 *
 *  Parameters :  hDlg     = window handle of the dialog
 *
 *  Return     :  [none]
 *
 *************************************************************************/
VOID DlgSetSysMenu(HWND hDlg) {

#if 0
	HWND     hSysMenu;
	MENUITEM Mi;
	UINT     Pos;
	MRESULT  Id;
	SHORT    cItems;

	/******************************************************************/
	/*  We only want Move and Close in the system menu.               */
	/******************************************************************/

	hSysMenu = WinWindowFromID(hDlg, FID_SYSMENU);
	SendMessage( hSysMenu, MM_QUERYITEM, MPFROM2SHORT(SC_SYSMENU, FALSE), MPFROMP((PCH) & Mi));
	Pos = 0L;
	cItems = (SHORT)SendMessage( Mi.hwndSubMenu, MM_QUERYITEMCOUNT, 0, 0);
	while (cItems--) {
		Id = SendMessage( Mi.hwndSubMenu, MM_ITEMIDFROMPOSITION, MPFROMLONG(Pos), 0);
		switch (SHORT1FROMMR(Id)) {
			case SC_MOVE:
			case SC_CLOSE:
				Pos++;  /* Don't delete that one. */
				break;
			default:
				SendMessage( Mi.hwndSubMenu, MM_DELETEITEM, MPFROM2SHORT((USHORT)Id, TRUE), 0);
		}
	}
#endif

	return;
}

#endif


/* ===========================================================================
-- Routine to center a dialog window in either its parent or to a specified window 
--
-- Usage: void DlgCenterWindow(HWND hdlg);
--        void DlgCenterWindowEx(HWND hdlg, HWND parent);
--
-- Inputs: hdlg - window to center
--         parent - if !NULL, window to center hdlg within
--                  if NULL or ! IsWindow(hdlg), tries to use GetParent(hdlg), otherwise
--                  GetDesktopWindow()
--
-- Output: Centers the specified window within the specified parent window
--
-- Return: none
--
-- Notes: While called "parent", can be any window that can be enumerated
=========================================================================== */
void DlgCenterWindowEx(HWND hdlg, HWND parent) {

	HWND hwndOwner; 
	RECT rc, rcDlg, rcOwner; 

	if (parent == HWND_DESKTOP) {
		hwndOwner = GetDesktopWindow();
	} else if (parent != NULL && IsWindow(parent)) {
		hwndOwner = parent;
	} else if ((hwndOwner = GetParent(hdlg)) == NULL) {
		hwndOwner = GetDesktopWindow();
	}
	GetWindowRect(hwndOwner, &rcOwner); 
	GetWindowRect(hdlg, &rcDlg); 
	CopyRect(&rc, &rcOwner); 

/* Offset owner and dialog box rectangles so that right/bottom represent the
 * width and height, and then offset the owner again to discard space taken
 * up by the dialog box.
 */
	OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
	OffsetRect(&rc, -rc.left, -rc.top); 
	OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

/* The new position is the sum of half the remaining space and the owner's
 * original position.
 */
	SetWindowPos(hdlg, HWND_TOP, rcOwner.left+rc.right/2, rcOwner.top+rc.bottom/2, 0, 0, SWP_NOSIZE); 
	return;
}


/* ============================================================================
	Routine to complement WritePrivateProfileString - really stupid its not there
============================================================================ */
// Win API function (could rewrite for myself if ever needed)
// BOOL WINAPI WritePrivateProfileString(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPCTSTR lpString, LPCTSTR lpFileName);

BOOL WritePrivateProfileStr(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPCTSTR lpString, LPCTSTR lpFileName) {
	return WritePrivateProfileString(lpAppName, lpKeyName, lpString, lpFileName);
}

BOOL WritePrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int ival, LPCTSTR lpFileName) {
	char buf[80];
	sprintf_s(buf, sizeof(buf), "%d", ival);
	return WritePrivateProfileString(lpAppName, lpKeyName, buf, lpFileName);
}

BOOL WritePrivateProfileDouble(LPCTSTR lpAppName, LPCTSTR lpKeyName, double rval, LPCTSTR lpFileName) {
	char buf[80];
	sprintf_s(buf, sizeof(buf), "%g", rval);
	return WritePrivateProfileString(lpAppName, lpKeyName, buf, lpFileName);
}

/* ============================================================================
	Routine to complement GetPrivateProfileInt - really stupid its not there
============================================================================ */
// Win API function (could rewrite for myself if ever needed)
// DWORD WINAPI GetPrivateProfileString(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPCTSTR lpDefault, LPTSTR lpReturnedString, DWORD nSize, LPCTSTR lpFileName);

BOOL ReadPrivateProfileStr(LPCTSTR lpAppName, LPCTSTR lpKeyName, char *str, size_t len, LPCTSTR lpFileName) {
	char szBuf[256];
	if (GetPrivateProfileString(lpAppName, lpKeyName, NULL, szBuf, sizeof(szBuf), lpFileName) <= 0) return FALSE;
	strcpy_m(str, len, szBuf);
	return TRUE;
}

BOOL ReadPrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int *ival, LPCTSTR lpFileName) {
	char szBuf[256];
	if (GetPrivateProfileString(lpAppName, lpKeyName, NULL, szBuf, sizeof(szBuf), lpFileName) <= 0) return FALSE;
	*ival = atol(szBuf);
	return TRUE;
}

BOOL ReadPrivateProfileDouble(LPCTSTR lpAppName, LPCTSTR lpKeyName, double *rval, LPCTSTR lpFileName) {
	char szBuf[256];
	if (GetPrivateProfileString(lpAppName, lpKeyName, NULL, szBuf, sizeof(szBuf), lpFileName) <= 0) return FALSE;
	*rval = atof(szBuf);
	return TRUE;
}

/* ===========================================================================
-- Routines to handle working with radio buttons
--
-- Usage: BOOL SetRadioButton(HWND hdlg, int nID_first, int nID_last, int nID_want);
--        BOOL SetRadioButtonIndex(HWND hdlg, int nID_first, int nID_last, int index);
--        int  GetRadioButton(HWND hdlg, int nID_first, int nID_last)
--        int  GetRadioButtonIndex(HWND hdlg, int nID_first, int nID_last);
--
-- Input: hdlg      - pointer to a valid dialog box handle
--        nID_first - ID of first button in the group
--        nID_last  - ID of last button in the group
--        nID_want  - button that we want to be checked
--        index     - Index to set (relative to nID_first)
--
-- Output: SetRadioButton sends a BST_UNCHECKED to all controls except nID_want
--         GetRadioButton goes through the list returning first that is checked
--
-- Return: SetRadioButton() returns TRUE as long as nID_first <= nID_want <= nID_last
--                          and nID_first <= nID_last.  If not returns FALSE.  If
--                          nID_first <= nID_last, still clears and sets nID_first.
--                          Otherwise does nothing
--         GetRadioButton returns first ID that is checked.  If none are checked,
--                        returns nID_first
--
-- Notes: SetRadioButton() should be equivalent to CheckRadioButton()
=========================================================================== */
BOOL SetRadioButton(HWND hdlg, int nID_first, int nID_last, int nID_want) {
	int i, rc;

	if (nID_last < nID_first) return FALSE;
	
	if ( ! (rc = nID_want >= nID_first && nID_want <= nID_last) ) nID_want = nID_first;
	for (i=nID_first; i<=nID_last; i++) SendDlgItemMessage(hdlg, i, BM_SETCHECK, i==nID_want ? BST_CHECKED : BST_UNCHECKED, 0);

	return rc;
}

int GetRadioButton(HWND hdlg, int nID_first, int nID_last) {
	int i;
	for (i=nID_first; i<=nID_last; i++) {
		if (SendDlgItemMessage(hdlg, i, BM_GETCHECK, 0, 0)) break;
	}
	if (i > nID_last) i = nID_first;
	return i;
}

/* These could have been done as macros, but safer to avoid and side-effect issues */
BOOL SetRadioButtonIndex(HWND hdlg, int nID_first, int nID_last, int index) {
	return SetRadioButton(hdlg, nID_first, nID_last, nID_first + index);
}
int GetRadioButtonIndex(HWND hdlg, int nID_first, int nID_last) {
	return GetRadioButton(hdlg, nID_first, nID_last) - nID_first;
}

/* ===========================================================================
-- Routines to automate the filling in of combo boxes with a text (id)
-- and a value (either integer or pointer).  Returns the data
-- associated with the selected item, as well as choosing an item based
-- on the value rather than index in the list.
--
-- Usage: int ComboBoxClearList(HWND hdlg, int wID);
--			 int ComboBoxClearSelection(HWND hdlg, int wID);
--
--			 int ComboBoxGetIndex(HWND hdlg, int wID);
--			 int ComboBoxSetByIndex(HWND hdlg, int wID, int index);
--
--        int ComboBoxFillIntList(HWND hdlg, int wID, CB_INT_LIST *list, int n);
--        int ComboBoxAddIntItem(HWND hdlg, int wID, char *text, int value);
--        int ComboBoxGetIntValue(HWND hdlg, int wID);
--        int ComboBoxSetByIntValue(HWND hdlg, int wID, int ival);
--
--        int ComboBoxFillPtrList(HWND hdlg, int wID, CB_PTR_LIST *list, int n);
--			 int ComboBoxAddPtrItem(HWND hdlg, int wID, char *text, void *value);
--        void *ComboBoxGetPtrValue(HWND hdlg, int wID);
--        int ComboBoxSetByPtrValue(HWND hdlg, int wID, void *ival);
--
-- Inputs: hdlg  - dialog box handle for the combo box
--         wID   - specific control
--         list  - either an integer value or pointer value list of id/value
--         value - either an integer value or pointer value list of id/value
--         n     - number of elements in list (use CB_COUNT macro)
--
-- Output: FillIn     - creates the list on screen.
--         SetByValue - selects an entry based on value (takes entry 0 if not found)
--    
-- Return: 0 if successful, 1 if any sort of error.  Normally this is that the 
--         value requested in a SetByValue does not exist.
--
-- Notes:
--   (1) The SetByIntValue or SetByPtrValue functions will choose the first
--       element if the specified int or string does not exist.  In this
--       case, the function returns 1; if item exists, it returns 0;
--   (2) Use ComboBoxClearSelection() to clear all selections
=========================================================================== */
int ComboBoxClearList(HWND hdlg, int wID) {
	SendDlgItemMessage(hdlg, wID, CB_RESETCONTENT, (WPARAM) 0, (LPARAM) 0);
	return 0;
}

int ComboBoxClearSelection(HWND hdlg, int wID) {
	SendDlgItemMessage(hdlg, wID, CB_SETCURSEL, (WPARAM) -1, (LPARAM) 0);
	return 0;
}

int ComboBoxGetIndex(HWND hdlg, int wID) {
	int item;
	item = (int) SendDlgItemMessage(hdlg, wID, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
	if (item == CB_ERR) item = 0;
	return item;
}

int ComboBoxSetByIndex(HWND hdlg, int wID, int index) {
	return (int) SendDlgItemMessage(hdlg, wID, CB_SETCURSEL, index, (LPARAM) 0);
}

int ComboBoxAddIntItem(HWND hdlg, int wID, char *text, int value) {
	int item;
	item = (int) SendDlgItemMessage(hdlg, wID, CB_ADDSTRING, 0, (LPARAM) text);
	return (SendDlgItemMessage(hdlg, wID, CB_SETITEMDATA, (WPARAM) item, (LPARAM) value) == CB_ERR) ? 1 : 0 ;
}

int ComboBoxFillIntList(HWND hdlg, int wID, CB_INT_LIST *list, int n) {
	int i, item;
	for (i=0; i<n; i++) {
		item = (int) SendDlgItemMessage(hdlg, wID, CB_ADDSTRING, 0, (LPARAM) list[i].id);
		SendDlgItemMessage(hdlg, wID, CB_SETITEMDATA, (WPARAM) item, (LPARAM) list[i].value);
	}
	return 0;
}

int ComboBoxGetIntValue(HWND hdlg, int wID) {
	int item;
	item = (int) SendDlgItemMessage(hdlg, wID, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
	if (item == CB_ERR) item = 0;
	return (int) SendDlgItemMessage(hdlg, wID, CB_GETITEMDATA, (WPARAM) item, (LPARAM) 0);
}

int ComboBoxSetByIntValue(HWND hdlg, int wID, int ival) {
	int count, item, rc=0;
	count = (int) SendDlgItemMessage(hdlg, wID, CB_GETCOUNT, (WPARAM) 0, (LPARAM) 0);
	for (item=0; item<count; item++) {
		if (SendDlgItemMessage(hdlg, wID, CB_GETITEMDATA, (WPARAM) item, (LPARAM) 0) == ival) break;
	}
	if (item >= count) { rc = 1; item = 0; }
	SendDlgItemMessage(hdlg, wID, CB_SETCURSEL, item, (LPARAM) 0);
	return rc;
}

int ComboBoxAddPtrItem(HWND hdlg, int wID, char *text, void *value) {
	int item;
	item = (int) SendDlgItemMessage(hdlg, wID, CB_ADDSTRING, 0, (LPARAM) text);
	return (SendDlgItemMessage(hdlg, wID, CB_SETITEMDATA, (WPARAM) item, (LPARAM) value) == CB_ERR) ? 1 : 0 ;
}

int ComboBoxFillPtrList(HWND hdlg, int wID, CB_PTR_LIST *list, int n) {
	int i, item;
	for (i=0; i<n; i++) {
		item = (int) SendDlgItemMessage(hdlg, wID, CB_ADDSTRING, 0, (LPARAM) list[i].id);
		SendDlgItemMessage(hdlg, wID, CB_SETITEMDATA, (WPARAM) item, (LPARAM) list[i].value);
	}
	return 0;
}

void *ComboBoxGetPtrValue(HWND hdlg, int wID) {
	int item;
	item = (int) SendDlgItemMessage(hdlg, wID, CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
	if (item == CB_ERR) item = 0;
	return (void *) SendDlgItemMessage(hdlg, wID, CB_GETITEMDATA, (WPARAM) item, (LPARAM) 0);
}

int ComboBoxSetByPtrValue(HWND hdlg, int wID, void *ival) {
	int count, item, rc=0;
	count = (int) SendDlgItemMessage(hdlg, wID, CB_GETCOUNT, (WPARAM) 0, (LPARAM) 0);
	for (item=0; item<count; item++) {
		if ((void *) SendDlgItemMessage(hdlg, wID, CB_GETITEMDATA, (WPARAM) item, (LPARAM) 0) == ival) break;
	}
	if (item >= count) { rc = 1; item = 0; }
	SendDlgItemMessage(hdlg, wID, CB_SETCURSEL, item, (LPARAM) 0);
	return rc;
}

/* ===========================================================================
-- Print out the message passed and parameters in human readable format
--
-- Usage: ShowMsgInfo(char *routine, int msg, WPARAM wparam, LPARAM lparam);
--
-- Inputs: routine - name of dialog box or window handler (usually)
--         msg, lParam, wParam - message information
--
-- Output: Prints message to stdout
--
-- Return: none
=========================================================================== */
char *GetMsgInfo(char *routine, UINT msg, WPARAM wparam, LPARAM lparam) {
	int i;
	static char szBuf[256];
	static struct {
		UINT msg;
		char *text;
	} msg_list[] = {
		{0x0000,	"WM_NULL"},
		{0x0001,	"WM_CREATE"},
		{0x0002,	"WM_DESTROY"},
		{0x0003,	"WM_MOVE"},
		{0x0005,	"WM_SIZE"},
		{0x0006,	"WM_ACTIVATE"},
		{0x0007,	"WM_SETFOCUS"},
		{0x0008,	"WM_KILLFOCUS"},
		{0x000A,	"WM_ENABLE"},
		{0x000B,	"WM_SETREDRAW"},
		{0x000C,	"WM_SETTEXT"},
		{0x000D,	"WM_GETTEXT"},
		{0x000E,	"WM_GETTEXTLENGTH"},
		{0x000F,	"WM_PAINT"},
		{0x0010,	"WM_CLOSE"},
		{0x0011,	"WM_QUERYENDSESSION"},
		{0x0013,	"WM_QUERYOPEN"},
		{0x0016,	"WM_ENDSESSION"},
		{0x0012,	"WM_QUIT"},
		{0x0014,	"WM_ERASEBKGND"},
		{0x0015,	"WM_SYSCOLORCHANGE"},
		{0x0018,	"WM_SHOWWINDOW"},
		{0x001A,	"WM_WININICHANGE"},
		{0x001B,	"WM_DEVMODECHANGE"},
		{0x001C,	"WM_ACTIVATEAPP"},
		{0x001D,	"WM_FONTCHANGE"},
		{0x001E,	"WM_TIMECHANGE"},
		{0x001F,	"WM_CANCELMODE"},
		{0x0020,	"WM_SETCURSOR"},
		{0x0021,	"WM_MOUSEACTIVATE"},
		{0x0022,	"WM_CHILDACTIVATE"},
		{0x0023,	"WM_QUEUESYNC"},
		{0x0024,	"WM_GETMINMAXINFO"},
		{0x0026,	"WM_PAINTICON"},
		{0x0027,	"WM_ICONERASEBKGND"},
		{0x0028,	"WM_NEXTDLGCTL"},
		{0x002A,	"WM_SPOOLERSTATUS"},
		{0x002B,	"WM_DRAWITEM"},
		{0x002C,	"WM_MEASUREITEM"},
		{0x002D,	"WM_DELETEITEM"},
		{0x002E,	"WM_VKEYTOITEM"},
		{0x002F,	"WM_CHARTOITEM"},
		{0x0030,	"WM_SETFONT"},
		{0x0031,	"WM_GETFONT"},
		{0x0032,	"WM_SETHOTKEY"},
		{0x0033,	"WM_GETHOTKEY"},
		{0x0037,	"WM_QUERYDRAGICON"},
		{0x0039,	"WM_COMPAREITEM"},
		{0x003D,	"WM_GETOBJECT"},
		{0x0041,	"WM_COMPACTING"},
		{0x0044,	"WM_COMMNOTIFY"},
		{0x0046,	"WM_WINDOWPOSCHANGING"},
		{0x0047,	"WM_WINDOWPOSCHANGED"},
		{0x0048,	"WM_POWER"},
		{0x004A,	"WM_COPYDATA"},
		{0x004B,	"WM_CANCELJOURNAL"},
		{0x004E,	"WM_NOTIFY"},
		{0x0050,	"WM_INPUTLANGCHANGEREQUEST"},
		{0x0051,	"WM_INPUTLANGCHANGE"},
		{0x0052,	"WM_TCARD"},
		{0x0053,	"WM_HELP"},
		{0x0054,	"WM_USERCHANGED"},
		{0x0055,	"WM_NOTIFYFORMAT"},
		{0x007B,	"WM_CONTEXTMENU"},
		{0x007C,	"WM_STYLECHANGING"},
		{0x007D,	"WM_STYLECHANGED"},
		{0x007E,	"WM_DISPLAYCHANGE"},
		{0x007F,	"WM_GETICON"},
		{0x0080,	"WM_SETICON"},
		{0x0081,	"WM_NCCREATE"},
		{0x0082,	"WM_NCDESTROY"},
		{0x0083,	"WM_NCCALCSIZE"},
		{0x0084,	"WM_NCHITTEST"},
		{0x0085,	"WM_NCPAINT"},
		{0x0086,	"WM_NCACTIVATE"},
		{0x0087,	"WM_GETDLGCODE"},
		{0x0088,	"WM_SYNCPAINT"},
		{0x00A0,	"WM_NCMOUSEMOVE"},
		{0x00A1,	"WM_NCLBUTTONDOWN"},
		{0x00A2,	"WM_NCLBUTTONUP"},
		{0x00A3,	"WM_NCLBUTTONDBLCLK"},
		{0x00A4,	"WM_NCRBUTTONDOWN"},
		{0x00A5,	"WM_NCRBUTTONUP"},
		{0x00A6,	"WM_NCRBUTTONDBLCLK"},
		{0x00A7,	"WM_NCMBUTTONDOWN"},
		{0x00A8,	"WM_NCMBUTTONUP"},
		{0x00A9,	"WM_NCMBUTTONDBLCLK"},
		{0x00AB,	"WM_NCXBUTTONDOWN"},
		{0x00AC,	"WM_NCXBUTTONUP"},
		{0x00AD,	"WM_NCXBUTTONDBLCLK"},
		{0x00FE,	"WM_INPUT_DEVICE_CHANGE"},
		{0x00FF,	"WM_INPUT"},
		{0x0100,	"WM_KEYFIRST"},
		{0x0100,	"WM_KEYDOWN"},
		{0x0101,	"WM_KEYUP"},
		{0x0102,	"WM_CHAR"},
		{0x0103,	"WM_DEADCHAR"},
		{0x0104,	"WM_SYSKEYDOWN"},
		{0x0105,	"WM_SYSKEYUP"},
		{0x0106,	"WM_SYSCHAR"},
		{0x0107,	"WM_SYSDEADCHAR"},
		{0x0109,	"WM_UNICHAR"},
		{0x0109,	"WM_KEYLAST"},
		{0xFFFF,	"UNICODE_NOCHAR"},
		{0x0108,	"WM_KEYLAST"},
		{0x010D,	"WM_IME_STARTCOMPOSITION"},
		{0x010E,	"WM_IME_ENDCOMPOSITION"},
		{0x010F,	"WM_IME_COMPOSITION"},
		{0x010F,	"WM_IME_KEYLAST"},
		{0x0110,	"WM_INITDIALOG"},
		{0x0111,	"WM_COMMAND"},
		{0x0112,	"WM_SYSCOMMAND"},
		{0x0113,	"WM_TIMER"},
		{0x0114,	"WM_HSCROLL"},
		{0x0115,	"WM_VSCROLL"},
		{0x0116,	"WM_INITMENU"},
		{0x0117,	"WM_INITMENUPOPUP"},
		{0x0119,	"WM_GESTURE"},
		{0x011A,	"WM_GESTURENOTIFY"},
		{0x011F,	"WM_MENUSELECT"},
		{0x0120,	"WM_MENUCHAR"},
		{0x0121,	"WM_ENTERIDLE"},
		{0x0122,	"WM_MENURBUTTONUP"},
		{0x0123,	"WM_MENUDRAG"},
		{0x0124,	"WM_MENUGETOBJECT"},
		{0x0125,	"WM_UNINITMENUPOPUP"},
		{0x0126,	"WM_MENUCOMMAND"},
		{0x0127,	"WM_CHANGEUISTATE"},
		{0x0128,	"WM_UPDATEUISTATE"},
		{0x0129,	"WM_QUERYUISTATE"},
		{0x0132,	"WM_CTLCOLORMSGBOX"},
		{0x0133,	"WM_CTLCOLOREDIT"},
		{0x0134,	"WM_CTLCOLORLISTBOX"},
		{0x0135,	"WM_CTLCOLORBTN"},
		{0x0136,	"WM_CTLCOLORDLG"},
		{0x0137,	"WM_CTLCOLORSCROLLBAR"},
		{0x0138,	"WM_CTLCOLORSTATIC"},
		{0x01E1,	"MN_GETHMENU"},
		{0x0200,	"WM_MOUSEFIRST"},
		{0x0200,	"WM_MOUSEMOVE"},
		{0x0201,	"WM_LBUTTONDOWN"},
		{0x0202,	"WM_LBUTTONUP"},
		{0x0203,	"WM_LBUTTONDBLCLK"},
		{0x0204,	"WM_RBUTTONDOWN"},
		{0x0205,	"WM_RBUTTONUP"},
		{0x0206,	"WM_RBUTTONDBLCLK"},
		{0x0207,	"WM_MBUTTONDOWN"},
		{0x0208,	"WM_MBUTTONUP"},
		{0x0209,	"WM_MBUTTONDBLCLK"},
		{0x020A,	"WM_MOUSEWHEEL"},
		{0x020B,	"WM_XBUTTONDOWN"},
		{0x020C,	"WM_XBUTTONUP"},
		{0x020D,	"WM_XBUTTONDBLCLK"},
		{0x020E,	"WM_MOUSEHWHEEL"},
		{0x020E,	"WM_MOUSELAST"},
		{0x020D,	"WM_MOUSELAST"},
		{0x020A,	"WM_MOUSELAST"},
		{0x0209,	"WM_MOUSELAST"},
		{0x0210,	"WM_PARENTNOTIFY"},
		{0x0211,	"WM_ENTERMENULOOP"},
		{0x0212,	"WM_EXITMENULOOP"},
		{0x0213,	"WM_NEXTMENU"},
		{0x0214,	"WM_SIZING"},
		{0x0215,	"WM_CAPTURECHANGED"},
		{0x0216,	"WM_MOVING"},
		{0x0218,	"WM_POWERBROADCAST"},
		{0x0219,	"WM_DEVICECHANGE"},
		{0x0220,	"WM_MDICREATE"},
		{0x0221,	"WM_MDIDESTROY"},
		{0x0222,	"WM_MDIACTIVATE"},
		{0x0223,	"WM_MDIRESTORE"},
		{0x0224,	"WM_MDINEXT"},
		{0x0225,	"WM_MDIMAXIMIZE"},
		{0x0226,	"WM_MDITILE"},
		{0x0227,	"WM_MDICASCADE"},
		{0x0228,	"WM_MDIICONARRANGE"},
		{0x0229,	"WM_MDIGETACTIVE"},
		{0x0230,	"WM_MDISETMENU"},
		{0x0231,	"WM_ENTERSIZEMOVE"},
		{0x0232,	"WM_EXITSIZEMOVE"},
		{0x0233,	"WM_DROPFILES"},
		{0x0234,	"WM_MDIREFRESHMENU"},
		{0x0240,	"WM_TOUCH"},
		{0x0281,	"WM_IME_SETCONTEXT"},
		{0x0282,	"WM_IME_NOTIFY"},
		{0x0283,	"WM_IME_CONTROL"},
		{0x0284,	"WM_IME_COMPOSITIONFULL"},
		{0x0285,	"WM_IME_SELECT"},
		{0x0286,	"WM_IME_CHAR"},
		{0x0288,	"WM_IME_REQUEST"},
		{0x0290,	"WM_IME_KEYDOWN"},
		{0x0291,	"WM_IME_KEYUP"},
		{0x02A1,	"WM_MOUSEHOVER"},
		{0x02A3,	"WM_MOUSELEAVE"},
		{0x02A0,	"WM_NCMOUSEHOVER"},
		{0x02A2,	"WM_NCMOUSELEAVE"},
		{0x02B1,	"WM_WTSSESSION_CHANGE"},
		{0x02c0,	"WM_TABLET_FIRST"},
		{0x02df,	"WM_TABLET_LAST"},
		{0x0300,	"WM_CUT"},
		{0x0301,	"WM_COPY"},
		{0x0302,	"WM_PASTE"},
		{0x0303,	"WM_CLEAR"},
		{0x0304,	"WM_UNDO"},
		{0x0305,	"WM_RENDERFORMAT"},
		{0x0306,	"WM_RENDERALLFORMATS"},
		{0x0307,	"WM_DESTROYCLIPBOARD"},
		{0x0308,	"WM_DRAWCLIPBOARD"},
		{0x0309,	"WM_PAINTCLIPBOARD"},
		{0x030A,	"WM_VSCROLLCLIPBOARD"},
		{0x030B,	"WM_SIZECLIPBOARD"},
		{0x030C,	"WM_ASKCBFORMATNAME"},
		{0x030D,	"WM_CHANGECBCHAIN"},
		{0x030E,	"WM_HSCROLLCLIPBOARD"},
		{0x030F,	"WM_QUERYNEWPALETTE"},
		{0x0310,	"WM_PALETTEISCHANGING"},
		{0x0311,	"WM_PALETTECHANGED"},
		{0x0312,	"WM_HOTKEY"},
		{0x0317,	"WM_PRINT"},
		{0x0318,	"WM_PRINTCLIENT"},
		{0x0319,	"WM_APPCOMMAND"},
		{0x031A,	"WM_THEMECHANGED"},
		{0x031D,	"WM_CLIPBOARDUPDATE"},
		{0x031E,	"WM_DWMCOMPOSITIONCHANGED"},
		{0x031F,	"WM_DWMNCRENDERINGCHANGED"},
		{0x0320,	"WM_DWMCOLORIZATIONCOLORCHANGED"},
		{0x0321,	"WM_DWMWINDOWMAXIMIZEDCHANGE"},
		{0x0323,	"WM_DWMSENDICONICTHUMBNAIL"},
		{0x0326,	"WM_DWMSENDICONICLIVEPREVIEWBITMAP"},
		{0x033F,	"WM_GETTITLEBARINFOEX"},
		{0x0358,	"WM_HANDHELDFIRST"},
		{0x035F,	"WM_HANDHELDLAST"},
		{0x0360,	"WM_AFXFIRST"},
		{0x037F,	"WM_AFXLAST"},
		{0x0380,	"WM_PENWINFIRST"},
		{0x038F,	"WM_PENWINLAST"},
		{0x8000,	"WM_APP"}
	};

	*szBuf = '\0';
	for (i=0; i<sizeof(msg_list)/sizeof(msg_list[0]); i++) {
		if (msg_list[i].msg == msg) {
			sprintf_s(szBuf, sizeof(szBuf), "%s(): %-24s\twParam: 0x%8.8x\tlParam: 0x%8.8x\n", routine, msg_list[i].text, (int) wparam, (int) lparam);
			break;
		}
	}
	if (*szBuf == '\0') sprintf_s(szBuf, sizeof(szBuf), "%s(): id=0x%4.4x\twParam: 0x%8.8x\tlParam: 0x%8.8x\n", routine, msg, (int) wparam, (int) lparam);
	return szBuf;
}

void ShowMsgInfo(char *routine, UINT msg, WPARAM wparam, LPARAM lparam) {

	printf("%s", GetMsgInfo(routine, msg, wparam, lparam));
	fflush(stdout);
	return;
}




/******************************************************************************
	Routines for getting and putting numbers into dialog boxes.

	Hides the (embarassing) fact that Windows dialogs only pass strings.
******************************************************************************/

/* ===========================================================================
-- Disables the close button in the system menu of a child window
--
-- Usage: DisableCloseButton(HWND hwnd)
--
-- Inputs: hwnd - pointer to handle of window to change
--
-- Output: Finds the system menu and disables the SC_CLOSE
--
-- Return: nothing
=========================================================================== */
void DisableCloseButton(HWND hwnd) {
	HMENU hMenu;

	if ( (hMenu = GetSystemMenu(hwnd, FALSE)) != NULL) EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND|MF_DISABLED);
	return;
}


/* ============================================================================
   Routine to encode and put an integer value into a dialog box by user format.
  
   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg
           fmt  - char string formatting information.  If NULL, will use %g
           value - actual value to encode
  
   Output: Sets text of specified dialog control
  
   Return: nothing
============================================================================ */
void SetDlgItemFmtInt(HWND hdlg, int wID, char *fmt, int value) {
	char szBuf[256];

	if ( fmt == NULL ) fmt = "%d";
	sprintf_s(szBuf, sizeof(szBuf), fmt, value );
	SetDlgItemText( hdlg, wID, szBuf );

	return;
}
	
void PutInt( HWND hdlg, int wID, char *fmt, int value ) {
	SetDlgItemFmtInt(hdlg, wID, fmt, value);
	return;
}

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
void SetDlgItemDouble(HWND hdlg, int wID, char *fmt, double value ) {
	char szBuf[80];

	if ( fmt == NULL ) fmt = "%g";
	sprintf_s(szBuf, sizeof(szBuf), fmt, value );
	SetDlgItemText( hdlg, wID, szBuf );

	return;
}

void PutDouble( HWND hdlg, int wID, char *fmt, double value ) {
	char szBuf[80];

	if ( fmt == NULL ) fmt = "%g";
	sprintf_s(szBuf, sizeof(szBuf), fmt, value );
	SetDlgItemText( hdlg, wID, szBuf );

	return;
}

/* ============================================================================
   Routine to query the text in a dialog control and interpret as an integer.
   Returns the best guess with no error checking.  If the text is invalid,
   the window text will be reset to that part that is valid - ie. the
   string will be terminated at the first invalid character.

   Usage: int GetInt(HWND hdlg, int wID)

   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg

   Output: none

   Return: best guess of value in control window
============================================================================ */
int GetInt(HWND hdlg, int wID) {
	return GetDlgItemIntEx(hdlg, wID);
}

int GetDlgItemIntEx(HWND hdlg, int wID) {
	char szBuf[80], *endptr, *aptr;
	int ival;

	/* get number from string */
	GetDlgItemText( hdlg, wID, szBuf, sizeof(szBuf) );
	ival = strtol(szBuf, &endptr, 10);

	/* search for and eliminate garbage after last valid point */
	aptr = endptr;
	while (isspace(*endptr)) endptr++;
	if ( *aptr != '\0' ) {
		*endptr = '\0';
		SetDlgItemText( hdlg, wID, szBuf );
	}

	/* return the number */
	return (ival);
}


/* ============================================================================
   Routine to query the text in a dialog control and interpret as a float.
   Returns the best guess with no error checking.  If the text is invalid,
   the window text will be reset to that part that is valid - ie. the
   string will be terminated at the first invalid character.
  
   Usage: double GetDouble(HWND hdlg, int wID);
          double GetDoubleEx(HWND hdlg, int wID, BOOL modify);
			 double GetDlgItemDouble(HWND hdlg, int wID);
  
   Inputs: hdlg - dialog box handle
           wID  - ID of control within hdlg
           modify - rewrite text eliminating any text past valid interpret
  
   Output: none
  
   Return: best guess of value in control window
============================================================================ */
double GetDlgItemDouble(HWND hdlg, int wID) {
	char szBuf[80], *endptr, *aptr;
	double dval;

	/* get number from string */
	GetDlgItemText(hdlg, wID, szBuf, sizeof(szBuf));
	dval = strtod(szBuf, &endptr);

	/* search for and eliminate garbage after last valid point */
	aptr = endptr;
	while (isspace(*endptr)) endptr++;
	if (*aptr != '\0') {
		*endptr = '\0';
		SetDlgItemText(hdlg, wID, szBuf);
	}

	/* return the number */
	return (dval);
}

double GetDouble( HWND hdlg, int wID ) {
	return GetDlgItemDouble(hdlg, wID);
}

double GetDoubleNC(HWND hdlg, int wID) {
	char szBuf[80];

	/* get number from string */
	GetDlgItemText(hdlg, wID, szBuf, sizeof(szBuf));
	return atof(szBuf);
}


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
double GetDoubleEx( HWND hdlg, int wID, BOOL *invalid, int *BadID ) {
	char szBuf[80], *endptr;
	double dval;
	
	/* get number from string */
	GetDlgItemText( hdlg, wID, szBuf, sizeof(szBuf) );
	dval = strtod( szBuf, &endptr );

	/* search for and identify garbage after last valid point */
	while (isspace(*endptr)) endptr++;
	if ( *endptr != '\0' ) {
		if ( invalid != NULL ) *invalid = TRUE;
		if ( BadID   != NULL ) *BadID   = wID;
	}

	/* return the number */
	return (dval);
}


/* ============================================================================
	For use with GetDoubleEx after check on invalid
============================================================================ */
void AskForDoubleAgain( HWND hdlg, int id ) {
  MessageBox( HWND_DESKTOP, "Invalid floating point number in text box.  Please correct.", "Nice Try", MB_ICONERROR | MB_OK );
  SetFocus( GetDlgItem( hdlg, id ) );
  return;
}


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
           dflt - default value if completely invalid  (not used anymore
  
   Output: Always rewrites the control text with the specified format
  
   Returns: Limits to specified range, taking max or min as appropriate
============================================================================ */
double GetConstrainedDouble( HWND hdlg, int wID, BOOL positive, char *fmt, double low, double high, double dflt ) {
	char szBuf[80], *aptr, *endptr;
	double value;

	/* get number from string */
	GetDlgItemText( hdlg, wID, szBuf, sizeof(szBuf) );
	aptr = szBuf;
	while (isspace(*aptr)) aptr++;

	value = strtod(aptr, &endptr);
	if (*aptr == '\0' || endptr == aptr) {
		value = dflt;
	} else {
		if (positive) value = fabs(value);				/* constrain value */
		if (value < low) value = low;
		if (value > high) value = high;
	}

	/* report contrained value */
	sprintf_s(szBuf, sizeof(szBuf), fmt, value);
	SetDlgItemText(hdlg, wID, szBuf);
	return value;
}

int GetConstrainedInt( HWND hdlg, int wID, BOOL positive, char *fmt, int low, int high, int dflt ) {
	char szBuf[80], *aptr;
	int value;

	/* get number from string */
	GetDlgItemText( hdlg, wID, szBuf, sizeof(szBuf) );
	aptr = szBuf;
	while (isspace(*aptr)) aptr++;
	value = (*aptr == '\0') ? dflt : atoi(aptr);

	/* constrain value */
	if (positive) value = abs(value);
	if (value < low) value = low;
	if (value > high) value = high;

	/* report contrained value */
	sprintf_s(szBuf, sizeof(szBuf), fmt, value );
	SetDlgItemText( hdlg, wID, szBuf );
	return (value);
}


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
double GetDoubleInRange( HWND hdlg, int wID, char *fmt, double low, double high ) {
	char szBuf[80];
	double value;
	
	/* Make sure low < high, if not swappem */
	if ( high < low ) {
	  double swap;
	  swap = low;
	  low = high;
	  high = swap;
	}

	/* Get number from dialog */
	GetDlgItemText( hdlg, wID, szBuf, sizeof(szBuf) );
	value = atof(szBuf);

	/* Constrain value */
	if ( value < low ) value = low;
	else if ( high < value ) value = high;

	/* Report constrained value */
	sprintf_s(szBuf, sizeof(szBuf), fmt, value );
	SetDlgItemText( hdlg, wID, szBuf );
	return (value);
}

/* take input from dialog and snap to an integer within given range [low,high] */
int GetDlgIntInRangeEx( HWND hdlg, int wID, char *fmt, int low, int high ) {
	char szBuf[80];
	int value;
	
	/* Make sure low < high, if not swappem */
	if (high < low) {
	  int swap;
	  swap = low;
	  low = high;
	  high = swap;
	}

	/* Get number from dialog */
	GetDlgItemText( hdlg, wID, szBuf, sizeof(szBuf) );
	value = atoi(szBuf);

	/* Constrain value */
	if ( value < low ) value = low;
	else if ( high < value ) value = high;

	/* Report constrained value */
	sprintf_s(szBuf, sizeof(szBuf), fmt, value );
	SetDlgItemText( hdlg, wID, szBuf );
	return(value);
}

/******************************************************************************
	Routines to mimic MessageBox() but spawn the dialog as a separate thread
	so routines can post error messages and not wait for a response
******************************************************************************/
typedef struct _MSG_BOX_ARGLIST {
  HWND hwnd;
  char *text;
  char *caption;
  UINT utype;
} MSG_BOX_ARGLIST, *LP_MSG_BOX_ARGLIST;


static void MessageBoxThread(void *arglist) {
	MSG_BOX_ARGLIST *mbargs;

	mbargs = (MSG_BOX_ARGLIST *) arglist;
	MessageBox(mbargs->hwnd, mbargs->text, mbargs->caption, mbargs->utype);
	free(mbargs->text); 
	free(mbargs->caption);
	free(mbargs);
	return;
}


int SpawnMessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType) {

  MSG_BOX_ARGLIST *MsgBoxArgList;
  uintptr_t thread_id;

  MsgBoxArgList = calloc(sizeof(*MsgBoxArgList), 1);
  MsgBoxArgList->hwnd  = hWnd;
  MsgBoxArgList->utype = uType;
  MsgBoxArgList->text = _strdup(lpText);
  MsgBoxArgList->caption = _strdup(lpCaption);

  thread_id = _beginthread(MessageBoxThread, 0, MsgBoxArgList);
  Sleep(200);  /* give dialog a chance to copy strings to screen */
  return (int) thread_id;
}


/******************************************************************************
	Error handling routines
******************************************************************************/

/* Freeze thread, put up a message box, and then exit the process */
void AbortOnFatalError( char *rname, char *msg ) {
  char sz_caption[256], sz_text[1024];
  sprintf_s(sz_caption, sizeof(sz_caption), "%s Error", rname );
  sprintf_s(sz_text, sizeof(sz_text), "Error Code %d\n\n %s", GetLastError(), msg );
  MessageBox( NULL, sz_text, sz_caption, MB_OK | MB_ICONERROR | MB_TASKMODAL | MB_TOPMOST );
  exit(EXIT_FAILURE);
}


/* ===========================================================================
-- Routine to write RTF text into a dialog box edit box
--
-- Usage: SetMyDlgRTFText(int control, char *msg, int fSize, int colorindex);
--
-- Input: control - ID Of dialog box to write text to
--        msg     - pointer to string to display in the text region
--        fSize   - font size (points)
--        colorindex - which of defined colors to paint with
--          [0..4] ==> black, red, green, blue, white
--
-- Output: none
--
-- Return: 0 if successful.  Otherwise failure of start dialog box.
=========================================================================== */
static DWORD CALLBACK RTF_TextFromStream(DWORD_PTR dwCookie, BYTE *buf, LONG cb, LONG *pcb) {
	FILE *funit;

	funit = (FILE *) dwCookie;					/* This is the file */
	*pcb = (LONG) fread(buf, sizeof(*buf), cb, funit);
	if (feof(funit) || ferror(funit)) fclose(funit);
	return(0);
}

typedef struct _RTF_STR {
	char *str;
	int posn;
} RTF_STR;


static DWORD CALLBACK RTF_TextFromString(DWORD_PTR dwCookie, BYTE *buf, LONG cb, LONG *pcb) {

	RTF_STR *ptr;
	char *str;
	int len;

	ptr = (RTF_STR *) dwCookie;				/* Pointer to text info		*/
	str = ptr->str + ptr->posn;				/* Current position			*/
	len = (int) strlen(str);
	len = min(len, cb);							/* No more than length		*/
	*pcb = len;										/* And tell caller			*/
	memcpy(buf, str, len);						/* Copy over amount wanted	*/
	ptr->posn += len;								/* New position				*/

	return(0);
}

int SetDlgRTFText(HWND hdlg, int control, char *msg, int fSize, int colorindex) {

#define	NUM_COLORS	(5)
	static char szTmp[256];
	static char full[] = "{\\rtf1 \\ansi \\deff0 {\\fonttbl {\\f0 Times New Roman;}}\n"
								"{\\colortbl;\\red0\\green0\\blue0;\\red192\\green0\\blue0;\\red0\\green176\\blue40;\\red0\\green0\\blue192;\\red255\\green255\\blue255;}\n"
								"\\f0 \\fs%d \\cf%d %s\\cf0}\n";

	char *aptr;
	static RTF_STR rtf_text;						/* For using RTF in text areas */
	EDITSTREAM stream;

/* To do multiple lines, just use <text>\\line <text> */
/* To get bold, \\b.  To get rid of bold, \\b0 */
/* To get italics, \\i.  To get rid of italics, \\i0 */
/* To set background color (not in edit box), \\cb1 */
	if (colorindex < 0) colorindex = 0;
	if (colorindex >= NUM_COLORS) colorindex = NUM_COLORS-1;
	if ( (aptr = strchr(msg, '\n')) != NULL) *aptr = '\0';		/* One line only */
	sprintf(szTmp, full, fSize, colorindex+1, msg);
	rtf_text.str  = szTmp;
	rtf_text.posn = 0;
	stream.dwCookie = (DWORD_PTR) &rtf_text;
	stream.dwError  = 0;
	stream.pfnCallback = RTF_TextFromString;
	SendDlgItemMessage(hdlg, control, EM_STREAMIN, (WPARAM) SF_RTF, (LPARAM) &stream);

	return(0);
}

/* ===========================================================================
-- Convert from SYSTEMTIME structure to UNIX time (time_t)
=========================================================================== */
__time64_t TimeFromSystemTime(const SYSTEMTIME *pTime) {
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	tm.tm_year = pTime->wYear - 1900;	/* Indexed from 1900 as per C standard */
	tm.tm_mon  = pTime->wMonth - 1;		/* January = 0 */
	tm.tm_mday = pTime->wDay;

	tm.tm_hour = pTime->wHour;
	tm.tm_min  = pTime->wMinute;
	tm.tm_sec  = pTime->wSecond;
	tm.tm_isdst = -1;							/* Let system determine if DST */

	return _mktime64(&tm);
}
