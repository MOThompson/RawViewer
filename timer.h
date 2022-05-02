#define	HIRESTIMER_MAGIC	(0x862AB7)
typedef struct _HIRES_TIMER {
	int magic;								/* Random number indicating this has not been released */
	LARGE_INTEGER freq, base;			/* Frequency and initial value (base) on call */
	double time_at_reset;				/* Declared value when timer created / reset */
} HIRES_TIMER;

HIRES_TIMER *HiResTimerCreate(void);
void HiResTimerDestroy(HIRES_TIMER *timer);
HIRES_TIMER *HiResTimerReset(HIRES_TIMER *timer, double time_now);
double HiResTimerDelta(HIRES_TIMER *timer);

LONGLONG InitIntervalTimer(void);
double IntervalTime(LONGLONG start);
double HighResIntervalTimer(BOOL reset);

