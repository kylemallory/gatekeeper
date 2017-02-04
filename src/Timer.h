/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*  * * * * * * * * * * * * * * * * * * * * * * * * * * *
 Code by Simon Monk
 http://www.simonmonk.org
* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef Timer_h
#define Timer_h

#include <inttypes.h>
#include <pthread.h>
#include <wiringPi.h>

#include "Event.h"
#include "ts_util.h"

#define MAX_NUMBER_OF_EVENTS 10

typedef unsigned long long mstimer_t;

class Timer
{

public:
    Timer(const char *threadName);
    ~Timer();

    int8_t every(const struct timespec period, void (*callback)(const struct timespec drift));
    int8_t every(mstimer_t period, void (*callback)(const struct timespec drift));
    int8_t every(const struct timespec period, void (*callback)(const struct timespec drift), int repeatCount);
    int8_t every(mstimer_t period, void (*callback)(const struct timespec drift), int repeatCount);
    int8_t after(const struct timespec period, void (*callback)(const struct timespec drift));
    int8_t after(mstimer_t period, void (*callback)(const struct timespec drift));
    int8_t latch(uint8_t pin, const struct timespec period, uint8_t value);
    int8_t latch(uint8_t pin, mstimer_t period, uint8_t value);
    int8_t oscillate(uint8_t pin, const struct timespec period, uint8_t startingValue);
    int8_t oscillate(uint8_t pin, mstimer_t period, uint8_t startingValue);
    int8_t oscillate(uint8_t pin, const struct timespec period, uint8_t startingValue, int repeatCount);
    int8_t oscillate(uint8_t pin, mstimer_t period, uint8_t startingValue, int repeatCount);
    int8_t pulse(uint8_t pin, const struct timespec period, uint8_t startingValue);
    int8_t pulse(uint8_t pin, mstimer_t period, uint8_t startingValue);
    int8_t pattern(uint8_t pin, const mstimer_t *pattern, uint8_t startingValue, int repeatCount);
    void stop(int8_t id);
    void shutdown();
    void update(void);

protected:
    Event _events[MAX_NUMBER_OF_EVENTS];
    int8_t findFreeEventIndex(void);
    pthread_t threadId;
    pthread_mutex_t eventMutex;
    struct timespec sleepDelay;
};

#endif
