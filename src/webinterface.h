#ifndef __WEBINTERFACE_H__
#define __WEBINTERFACE_H__

/*
 * Wiegand API Raspberry Pi
 * By Kyle Mallory
 * 12/01/2013
 * Based on previous code by Daniel Smith (www.pagemac.com) and Ben Kent (www.pidoorman.com)
 * Depends on the wiringPi library by Gordon Henterson: https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 * The Wiegand interface has two data lines, DATA0 and DATA1.  These lines are normall held
 * high at 5V.  When a 0 is sent, DATA0 drops to 0V for a few us.  When a 1 is sent, DATA1 drops
 * to 0V for a few us. There are a few ms between the pulses.
 *   *************
 *   * IMPORTANT *
 *   *************
 *   The Raspberry Pi GPIO pins are 3.3V, NOT 5V. Please take appropriate precautions to bring the
 *   5V Data 0 and Data 1 voltges down. I used a 330 ohm resistor and 3V3 Zenner diode for each
 *   connection. FAILURE TO DO THIS WILL PROBABLY BLOW UP THE RASPBERRY PI!
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <onion/log.h>
#include <onion/onion.h>
#include <onion/dict.h>
#include <onion/handler.h>
#include <onion/request.h>
#include <onion/response.h>
#include <onion/shortcuts.h>
#include <onion/types.h>
#include <onion/extras/png.h>
#include <onion/block.h>
#include <onion/static.h>
#include <onion/exportlocal.h>

#include <database.h>
#include <graphics.h>

#ifdef CAMERA
extern CCamera* g_cam;
#endif

int initWebInterface(void);
// int sendMMSemail(char *fromAddr, char *toAddr[], char *body);
int sendMMSemail(const char *fromAddr, const char *toAddr[], const char **lines, const char **attachments);
onion_connection_status sendEmail_handler(void *none, onion_request *req, onion_response *res);

#endif
