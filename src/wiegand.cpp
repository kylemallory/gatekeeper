#include "wiegand.h"

static unsigned char __wiegandData[WIEGANDMAXDATA];	// can capture upto 32 bytes of data -- FIXME: Make this dynamically allocated in init?
static unsigned long __wiegandBitCount;			// number of bits currently captured
static struct timespec __wiegandBitTime;		// timestamp of the last bit received (used for timeouts)

void data0Pulse(void)
{
    if (__wiegandBitCount / 8 < WIEGANDMAXDATA)
    {
        __wiegandData[__wiegandBitCount / 8] <<= 1;
        __wiegandBitCount++;
    }
    clock_gettime(CLOCK_MONOTONIC, &__wiegandBitTime);
}

void data1Pulse(void)
{
    if (__wiegandBitCount / 8 < WIEGANDMAXDATA)
    {
        __wiegandData[__wiegandBitCount / 8] <<= 1;
        __wiegandData[__wiegandBitCount / 8] |= 1;
        __wiegandBitCount++;
    }
    clock_gettime(CLOCK_MONOTONIC, &__wiegandBitTime);
}

int wiegandInit(int d0pin, int d1pin)
{
		int err = 0;
    // Setup wiringPi
    syslog(LOG_NOTICE, "Configuring Wiegand RFID/Keypad...\n");
    if ( wiringPiSetup() != 0) {
        syslog(LOG_ERR, "Error initializing RasbPi GPIO framework.\n" );
        return 1;
    }
    pinMode(d0pin, INPUT);
    pinMode(d1pin, INPUT);

		syslog(LOG_NOTICE, "Registering hardware interrupts...\n");
		err |= (wiringPiISR(d0pin, INT_EDGE_FALLING, data0Pulse) != 0);
		err |= (wiringPiISR(d1pin, INT_EDGE_FALLING, data1Pulse) != 0);

    if (err) {
        syslog(LOG_ERR, "Error initializing RasbPi GPIO ISR framework.\n" );
        return 1;
    }

    return 0;
}

void wiegandReset()
{
    memset((void *)__wiegandData, 0, WIEGANDMAXDATA);
    __wiegandBitCount = 0;
}

int wiegandGetPendingBitCount()
{
    struct timespec now, delta;
    clock_gettime(CLOCK_MONOTONIC, &now);
    delta.tv_sec = now.tv_sec - __wiegandBitTime.tv_sec;
    delta.tv_nsec = now.tv_nsec - __wiegandBitTime.tv_nsec;

    if ((delta.tv_sec > 1) || (delta.tv_nsec > WIEGANDTIMEOUT))
        return __wiegandBitCount;

    return 0;
}

/*
 * wiegandReadData is a simple, non-blocking method to retrieve the last code
 * processed by the API.
 * data : is a pointer to a block of memory where the decoded data will be stored.
 * dataMaxLen : is the maximum number of -bytes- that can be read and stored in data.
 * Result : returns the number of -bits- in the current message, 0 if there is no
 * data available to be read, or -1 if there was an error.
 * Notes : this function clears the read data when called. On subsequent calls,
 * without subsequent data, this will return 0.
 */
int wiegandReadData(void* data, int dataMaxLen)
{
    if (wiegandGetPendingBitCount() > 0)
    {
        int bitCount = __wiegandBitCount;
        int byteCount = (__wiegandBitCount / 8) + 1;
        if (data)
        {
            memset(data, 0, dataMaxLen);
            memcpy(data, (void *)__wiegandData, ((byteCount > dataMaxLen) ? dataMaxLen : byteCount));
        }

        wiegandReset();
        return bitCount;
    }
    return 0;
}


int doWiegandAccess()
{
    return 0;
}
