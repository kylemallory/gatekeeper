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

#include <stdio.h>
#include <unistd.h>

#include <pthread.h>
#include "Timer.h"

static void *timerHandler(void *vpTimer) {
    Timer *timer = (Timer *)vpTimer;
    for (;;) {
        timer->update();
    }
    return NULL;
}

Timer::Timer(void)
{
    //threadStatus = 1; // must set this first, or timerHandler might terminate early
    // this->eventMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&eventMutex, NULL);
    pthread_create(&threadId, NULL, &timerHandler, this);
}

Timer::~Timer() {
    // pthread_kill(&this->threadId, 0);
    pthread_mutex_destroy(&this->eventMutex);
}

void Timer::shutdown(void) {
    //this->threadStatus = 0;
}

int8_t Timer::every(const struct timespec period, void (*callback)(const struct timespec), int repeatCount)
{
    int8_t i = findFreeEventIndex();
    if (i == -1) return -1;

    pthread_mutex_lock(&this->eventMutex);

    _events[i].eventType = EVENT_EVERY;
    _events[i].period.tv_sec = period.tv_sec;
    _events[i].period.tv_nsec = period.tv_nsec;
    _events[i].repeatCount = repeatCount;
    _events[i].callback = callback;
    _events[i].pattern = NULL;
    clock_gettime(CLOCK_MONOTONIC, &(_events[i].lastEventTime));
    _events[i].count = 0;

    pthread_mutex_unlock(&this->eventMutex);

    return i;
}

int8_t Timer::every(mstimer_t period, void (*callback)(const struct timespec), int repeatCount)
{
    return every(tsFromMicrosec(period), callback, repeatCount);
}

int8_t Timer::every(const struct timespec period, void (*callback)(const struct timespec))
{
    return every(period, callback, -1); // - means forever
}

int8_t Timer::every(mstimer_t period, void (*callback)(const struct timespec))
{
    return every(tsFromMicrosec(period), callback, -1); // - means forever
}

int8_t Timer::after(const struct timespec period, void (*callback)(const struct timespec))
{
    return every(period, callback, 1);
}

int8_t Timer::after(mstimer_t period, void (*callback)(const struct timespec))
{
    return every(tsFromMicrosec(period), callback, 1);
}

int8_t Timer::latch(uint8_t pin, const struct timespec period, uint8_t value)
{
    int8_t i = findFreeEventIndex();
    if (i == -1) return -1;

    pthread_mutex_lock(&this->eventMutex);

    _events[i].eventType = EVENT_OSCILLATE;
    _events[i].pin = pin;
    _events[i].period.tv_sec = period.tv_sec;
    _events[i].period.tv_nsec = period.tv_nsec;
    _events[i].pattern = NULL;
    _events[i].callback = NULL;
    _events[i].pinState = value;
    digitalWrite(pin, value);
    _events[i].repeatCount = 1;
    clock_gettime(CLOCK_MONOTONIC, &(_events[i].lastEventTime));
    _events[i].count = 0;

    pthread_mutex_unlock(&this->eventMutex);

    return i;
}

int8_t Timer::latch(uint8_t pin, mstimer_t period, uint8_t value)
{
    return latch(pin, tsFromMicrosec(period), value);
}

int8_t Timer::oscillate(uint8_t pin, const struct timespec period, uint8_t startingValue, int repeatCount)
{
    int8_t i = findFreeEventIndex();
    if (i == -1) return -1;

    pthread_mutex_lock(&this->eventMutex);

    _events[i].eventType = EVENT_OSCILLATE;
    _events[i].pin = pin;
    _events[i].period.tv_sec = period.tv_sec;
    _events[i].period.tv_nsec = period.tv_nsec;
    _events[i].pattern = NULL;
    _events[i].callback = NULL;
    _events[i].pinState = startingValue;
    digitalWrite(pin, startingValue);
    _events[i].repeatCount = repeatCount * 2; // full cycles not transitions
    clock_gettime(CLOCK_MONOTONIC, &(_events[i].lastEventTime));
    _events[i].count = 0;

    pthread_mutex_unlock(&this->eventMutex);

    return i;
}

int8_t Timer::oscillate(uint8_t pin, mstimer_t period, uint8_t startingValue, int repeatCount)
{
    return oscillate(pin, tsFromMicrosec(period), startingValue, repeatCount);
}

int8_t Timer::oscillate(uint8_t pin, const struct timespec period, uint8_t startingValue)
{
    return oscillate(pin, period, startingValue, -1); // forever
}

int8_t Timer::oscillate(uint8_t pin, mstimer_t period, uint8_t startingValue)
{
    return oscillate(pin, tsFromMicrosec(period), startingValue, -1); // forever
}

int8_t Timer::pulse(uint8_t pin, const struct timespec period, uint8_t startingValue)
{
    return oscillate(pin, period, startingValue, 1); // once
}

int8_t Timer::pulse(uint8_t pin, mstimer_t period, uint8_t startingValue)
{
    return oscillate(pin, tsFromMicrosec(period), startingValue, 1); // once
}

int8_t Timer::pattern(uint8_t pin, const mstimer_t pattern[], uint8_t startingValue, int repeatCount)
{
    int8_t i = findFreeEventIndex();
    if (i == -1) return -1;

    pthread_mutex_lock(&this->eventMutex);

    _events[i].eventType = EVENT_PATTERN;
    _events[i].pin = pin;
    _events[i].period = tsFromMicrosec(pattern[0]);
    _events[i].pattern = pattern;
    _events[i].patternPos = 0;
    _events[i].callback = NULL;
    _events[i].pinState = startingValue;
    digitalWrite(pin, startingValue);
    _events[i].repeatCount = repeatCount;
    clock_gettime(CLOCK_MONOTONIC, &(_events[i].lastEventTime));
    _events[i].count = 0;

    pthread_mutex_unlock(&this->eventMutex);

    return i;
}

void Timer::stop(int8_t id)
{
    _events[id].eventType = EVENT_NONE;
}

struct timespec sleepyTime;
void Timer::update(void)
{
    sleepyTime.tv_nsec = 1L;
    sleepyTime.tv_sec = 0L;
    for (int8_t i = 0; i < MAX_NUMBER_OF_EVENTS; i++)
    {
        if (_events[i].eventType != EVENT_NONE)
        {
            pthread_mutex_lock(&this->eventMutex);
            _events[i].update();
            pthread_mutex_unlock(&this->eventMutex);
        }
    }
    nanosleep(&sleepyTime, NULL);
}

int8_t Timer::findFreeEventIndex(void)
{
    for (int8_t i = 0; i < MAX_NUMBER_OF_EVENTS; i++)
    {
        if (_events[i].eventType == EVENT_NONE)
        {
            return i;
        }
    }
    return -1;
}

