/* ***************************************************************************
WARNING: Recommendation now is to *not* typecast the FileTime structure
to an _int64 as may causes misalignment with 64-bit OS
*************************************************************************** */

/* Timer.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE                  /* Always require POSIX standard */
/* #include "preload.h" */					/* Nice, but probably not required */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <windows.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "timer.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define  TRUE           1
	#define  FALSE          0
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ============================================================================
-- Subroutine to return current start time - initializes interval timer
--
-- Usage:   LONGLONG = InitTime(void)
--
-- Inputs:  none
--
-- Output:  none
--
-- Returns: Current time as a 64-bit integer.  Actual format is undefined to
--          user, but should be returned unmodified to the IntervalTime function
--          to determine the time between calls.
============================================================================ */
LONGLONG InitIntervalTimer(void) {
	
	LONGLONG result;

	GetSystemTimeAsFileTime((FILETIME *) &result);
	return(result);
}

/* ============================================================================
-- Subroutine to return current start time - initializes interval timer
--
-- Usage:   double = IntervalTime(LONGLONG start)
--
-- Inputs:  start - origin of time, as returned by InitIntervalTimer
--
-- Output:  none
--
-- Returns: Time since call to InitIntervalTimer in seconds (double precision)
--          Best possible quantization is 100 ns, but actually only 50 ms (based
--          apparently on the 20 Hz interrupt clock in the system).
============================================================================ */
double IntervalTime(LONGLONG start) {

	LONGLONG result;
	GetSystemTimeAsFileTime((FILETIME *) &result);
	result -= start;
	return (result*100.0E-9);			/* 100 ns to seconds */
}

/* ===========================================================================
	-- Routine returning high precision time interval counter.  The return
	-- is in seconds with the highest precision that is reasonable for the system.
	-- This routine is guarenteed to be monotonic but not necessarily absolute.
	--
	-- Usage: double IntervalTimer(int reset)
	--
	-- Inputs: reset - (BOOL) if TRUE, reset start of interval timer
	--
	-- Output: none
	--
	-- Return: Time in seconds since first call, or last reset, of the counter
	--
	-- Note: Implicit resolution of the LARGE_INTEGER is larger than double. But
	--       maximum resolution seems to be 100 ns which means no loss of precision 
	--       for a timer running 10+ days.  Good enough for all currently
	--       envisioned purposes and avoids complications in use code.
	=========================================================================== */

/* ===========================================================================
-- Routine to work with multiple high-precision timers.  These are based on a
-- high frequency counter with ~100 ns resolution (in 2020).  Return values
-- are in seconds since either initialization or reset (to specified value).
--
-- Usage: HIRES_TIMER *HiResTimerCreate(void);
--        void HiResTimerDestroy(HIRES_TIMER *timer);
--        HIRES_TIMER *HiResTimerReset(HIRES_TIMER *timer, double time_now);
--        double HiResTimerDelta(HIRES_TIMER *timer);
--
-- Inputs: timer    - pointer structure returned by HiResTimerCreate or HiResTimerReset
--         time_now - value that HiResTimerDelta should return at instant of reset 
--                    (this is typically zero, but can be any value)
--
-- Output: HiResTimerCreate and HiResTimerReset initializes HIRES_TIMER structures.
--
-- Return: HiResTimerCreate()  - pointer to initialized HIRES_TIMER structure 
--                               Time zero is at moment of initialization
--         HiResTimerDestroy() - none, but timer is no longer valid
--         HiResTimerReset()   - pointer to initialized HIRES_TIMER structure
--                               Time zero is value specified in the call
--                               If timer is NULL, will allocate/return HIRES_TIMER structure
--                               Returns NULL if timer is invalid (maybe been closed) 
--         HiResTimerDelta()   - time (in secs) since create or reset (with specified offset)
--
-- Notes:
--   (1) HiResTimerCreate() is effectively HiResTimerReset(NULL, 0.0)
--   (2) The resolution of the counter is ~100 ns (10 MHz - as of 2020)
--   (3) The "double" return value has 52 bits of mantissa; this is sufficient
--       to ensure that time() values are represented exactly for essentially
--       eternity.  However, 100 ns precision will be lost after 28.56 years.
--   (3) While the counter and values are monotonic, they are not guarenteed to 
--       be absolute (more or less than 86,400 seconds in a day for example).
=========================================================================== */
HIRES_TIMER *HiResTimerCreate(void) {
	return HiResTimerReset(NULL, 0.0);
}

void HiResTimerDestroy(HIRES_TIMER *timer) {
	if (timer->magic == HIRESTIMER_MAGIC) {
		timer->magic = 0;
		free(timer);
	}
	return;
}

HIRES_TIMER *HiResTimerReset(HIRES_TIMER *timer, double time_now) {

	/* Verify the timer is a valid structure, return NULL if not */
	if (timer != NULL && timer->magic != HIRESTIMER_MAGIC) return NULL;

	/* Possibly allocate in one step */
	if (timer == NULL) {
		timer = calloc(1, sizeof(*timer));
		timer->magic = HIRESTIMER_MAGIC;
	}

	/* Get parameters and reset */
	QueryPerformanceFrequency(&timer->freq);
	QueryPerformanceCounter(&timer->base);
	timer->time_at_reset = time_now;

	return timer;
};

double HiResTimerDelta(HIRES_TIMER *timer) {
	LARGE_INTEGER counts;
	long double delta;

	if (timer == NULL || timer->magic != HIRESTIMER_MAGIC || timer->freq.QuadPart == 0) {
		delta = 0.0;
	} else { 
		QueryPerformanceCounter(&counts);
		delta  = (1.0 * (counts.QuadPart-timer->base.QuadPart)) / timer->freq.QuadPart;
		delta += timer->time_at_reset;
	}
	return delta;
}


/* ===========================================================================
-- This routine is retained for compatibility, but recommend use of the
-- HiResTimer() routines above for future applications.
--
-- Routine returning high precision time interval counter.  The return
-- is in seconds with the highest precision that is reasonable for the system.
-- This routine is guarenteed to be monotonic but not necessarily absolute.
--
-- Usage: double IntervalTimer(int reset)
--
-- Inputs: reset - (BOOL) if TRUE, reset start of interval timer
--
-- Output: none
--
-- Return: Time in seconds since first call, or last reset, of the counter
--
-- Note: Implicit resolution of the LARGE_INTEGER is larger than double. But
--       maximum resolution seems to be 100 ns which means no loss of precision 
--       for a timer running 28+ years.  Good enough for all currently
--       envisioned purposes and avoids complications in use code.
=========================================================================== */
double HighResIntervalTimer(BOOL reset) {
	static BOOL init = FALSE;

	static LARGE_INTEGER freq, count0;
	LARGE_INTEGER counts;

	if (! init || reset) {
		init = TRUE;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&count0);
		return 0.0;
	}
	QueryPerformanceCounter(&counts);
	return (double) ( (freq.QuadPart == 0) ? 0.0 : (1.0*(counts.QuadPart-count0.QuadPart))/freq.QuadPart );
}


/* ===========================================================================
=========================================================================== */
#ifdef TEST
int main(int argc, char *argv[]) {

	double intervals[80];
	LONGLONG start;
	int i;
	
	start = InitIntervalTimer();
	intervals[0] = IntervalTime(start);
	for (i=1; i<80; i++) { 
		while (TRUE) {
			intervals[i] = IntervalTime(start);
			if (intervals[i] != intervals[i-1]) break;
		}
	}
	for (i=0; i<80; i++) printf("%g %g\n", intervals[i], (i>0)?intervals[i]-intervals[i-1]:0.0);

	return(0);
}
#endif

