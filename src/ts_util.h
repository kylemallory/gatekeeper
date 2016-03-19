/* $Id: ts_util.h,v 1.11 2009/09/09 22:38:13 alex Exp $ */
/*******************************************************************************

    ts_util.h

    "timespec" Manipulation Definitions.

*******************************************************************************/

#ifndef  TS_UTIL_H		/* Has the file been INCLUDE'd already? */
#define  TS_UTIL_H  yes

#ifdef __cplusplus		/* If this is a C++ compiler, use C linkage */
extern  "C"  {
#endif

//#include  "pragmatics.h"		/* Compiler, OS, logging definitions. */
//#include  "tv_util.h"			/* "timeval" manipulation functions. */

/*******************************************************************************
    Public functions.
*******************************************************************************/

/**
* The number of microseconds per second.
*/
#define MICROS_PER_SECOND 1000000

/**
 * The number of nanoseconds per microsecond.
 */
#define NANOS_PER_MICRO 1000

/**
 * Convert a microsecond value to a struct timespec.
 *
 * @param microseconds The microsecond value to convert.
 * @return A timespec that represents the given number of microseconds.
 */
static inline
struct timespec tsFromMicrosec(unsigned long long microseconds)
{
    struct timespec retval;

    retval.tv_sec =  (int)(microseconds / MICROS_PER_SECOND);
    retval.tv_nsec = (int)(microseconds % MICROS_PER_SECOND) * NANOS_PER_MICRO;
    return retval;
}

/**
 * Convert a timespec to a microsecond value.  @e NOTE: There is a loss of
 * precision in the conversion.
 *
 * @param time_spec The timespec to convert.
 * @return The number of microseconds specified by the timespec.
 */
static inline
unsigned long long tsToMicrosec(struct timespec *time_spec)
{
    unsigned long long retval;

    retval = time_spec->tv_sec * MICROS_PER_SECOND;
    retval += time_spec->tv_nsec / NANOS_PER_MICRO;
    return retval;
}

extern void sleep_ms(int milliseconds);

extern struct timespec tsAdd( struct timespec time1, struct timespec time2 ) ;

extern int tsCompare( struct timespec time1, struct timespec time2 ) ;

extern struct timespec tsCreate( long seconds, long nanoseconds ) ;

extern struct timespec tsCreateF( double fSeconds ) ;

extern double tsFloat( struct timespec time ) ;

extern const char *tsShow( struct timespec binaryTime, int inLocal, const char *format ) ;

extern struct timespec tsSubtract( struct timespec time1, struct timespec time2 ) ;

#ifdef __cplusplus		/* If this is a C++ compiler, use C linkage */
}
#endif

#endif				/* If this file was not INCLUDE'd previously. */
