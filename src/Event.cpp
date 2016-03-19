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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 Code by Simon Monk
 http://www.simonmonk.org
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "Event.h"

Event::Event(void)
{
    eventType = EVENT_NONE;
    pattern = NULL;
    patternPos = 0;
    patternLen = 0;
    pin = 0;
    pinState = 0;
    callback = NULL;
    period = tsFromMicrosec(0L);
    count = 0;
    lastEventTime = tsFromMicrosec(0L);
}

void Event::update(void)
{
    mstimer_t p;
    struct timespec now, diff;
    clock_gettime(CLOCK_MONOTONIC, &now);
    diff = tsSubtract(now, lastEventTime);
    if ((diff.tv_nsec == 0) && (diff.tv_sec == 0)) {
        syslog(LOG_ERR, "Timer Error.\n");
        lastEventTime = now;
    }

    if (tsCompare(diff, period) >= 0)
    {
        diff = tsSubtract(diff, period);
        switch (eventType)
        {
        case EVENT_EVERY:
            (*callback)(diff);
            break;

        case EVENT_PATTERN:
            patternPos++;
            p = pattern[patternPos];
            if (p == 0L) {
                count++;
                if (repeatCount > -1 && count >= repeatCount) {
                    period = tsFromMicrosec(0L);
                    break;
                }
                patternPos = 0;
                p = pattern[patternPos];
            }
            period = tsFromMicrosec(p);
            pinState = ! pinState;
            digitalWrite(pin, pinState);
            break;
        case EVENT_OSCILLATE:
            pinState = ! pinState;
            digitalWrite(pin, pinState);
            break;
        }
        lastEventTime.tv_nsec = now.tv_nsec;
        lastEventTime.tv_sec = now.tv_sec;
        if (eventType != EVENT_PATTERN)
            count++;
    }
    if (repeatCount > -1 && count >= repeatCount)
    {
        eventType = EVENT_NONE;
        pattern = NULL;
        patternPos = 0;
        patternLen = 0;
        pin = 0;
        pinState = 0;
        callback = NULL;
        period = tsFromMicrosec(0L);
        count = 0;
        lastEventTime = tsFromMicrosec(0L);
    }
}
