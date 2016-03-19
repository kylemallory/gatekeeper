#ifdef __cplusplus
#include <cstdlib>
#include <string>
#include <iostream>
#else
#include <stdlib.h>
#endif

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <execinfo.h>
#include <ucontext.h>
#include <sys/stat.h>
//#include <syslog.h>
#include <bsd/libutil.h>

#include <wiringPi.h>

#include <graphics.h>
#include <wiegand.h>
#include <database.h>
#include <Timer.h>
#include <webinterface.h>

#define KEYPAD_DATA0_PIN    0
#define KEYPAD_DATA1_PIN    1
#define KEYPAD_LED_PIN      2
#define KEYPAD_BUZZER_PIN   3
#define DOOR_STRIKE_PIN     4
#define CAMERA_FLASH_PIN    5
#define DISPLAY_POWER_PIN   6

#define ACCESS_RFID 0
#define ACCESS_PIN  1

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1



#define FROM     "<keymaster@toyshedstudios.com>"
#define TO       "<8018310780@mms.att.net>"
#define CC       "<info@example.com>"

static const char *fromAddr = FROM;
static const char *toAddr[] = { TO , NULL };


/* additional make libraries

-lopencv_gpu -lopencv_contrib -lopencv_legacy
-lopencv_calib3d
-lopencv_features2d -lopencv_video
-lopencv_highgui -lopencv_ml
-lopencv_flann
*/

bool g_done = false;
struct pidfh *pfh = NULL;
pthread_t keypadThread_id;
Timer *timer = NULL;
time_t tzOffset;
bool camera_enabled = false;

// patterns are a list of durations (ms), first high, second low, third high, fourth low, etc.
// patterns are terminated with a -1 duration
const mstimer_t pattern_F[] = { 100000L, 100000L, 0L };
const mstimer_t pattern_S[] = { 500000L, 500000L, 0L };
const mstimer_t pattern_L[] = { 1000000L, 1000000L, 0L };
const mstimer_t pattern_FF[] = { 100000L, 100000L, 100000L, 100000L, 0L };
const mstimer_t pattern_SS[] = { 500000L, 500000L, 500000L, 500000L, 0L };
const mstimer_t pattern_FFPFF[] = { 100000L, 100000L, 100000L, 500000L, 100000L, 100000L, 100000L,  0L };
const mstimer_t pattern_FFS[] = { 100000L, 100000L, 100000L, 100000L, 500000L, 500000L, 0L };
const mstimer_t pattern_FSFFS[] = { 100000L, 100000L, 500000L, 500000L, 100000L, 100000L, 100000L, 100000L, 500000L, 500000L, 0 };

const char *msgAccessGranted = "Access Granted";
const char *msgAccessDenied = "Access Denied";

void sig_handler(int signum) {
    if ((signum == SIGTERM) || (signum == SIGINT)) {
        syslog(LOG_NOTICE, "Received SIGNAL %d, exiting...\n", signum);
        g_done = true;
    } else if (signum == SIGKILL){
        syslog(LOG_NOTICE, "Received SIG %d, exiting...\n", signum);
        SDL_Quit();
        exit(0);
    } else if (signum == SIGSEGV) {
      void *array[10];
      size_t size;

      // get void*'s for all entries on the stack
      size = backtrace(array, 10);

      // print out all the frames to stderr
      syslog(LOG_ERR, "Error: signal %d:\n", signum);
      int fd = -1;
      if (( fd = open("/tmp/PiLock.crash", O_RDWR|O_CREAT) ) >= 0) {
          backtrace_symbols_fd(array, size, fd);
          close(fd);
      }
      SDL_Quit();
      exit(1);
    }
}

int doAuthorization(int mode, uint64_t keyCode) {
    int userId, isAuthorized, reason;

    if (mode == ACCESS_PIN) {
        syslog(LOG_NOTICE, "Checking PIN authorization: %llu\n", keyCode);
    } else {
        syslog(LOG_NOTICE, "Checking RFID authorization: %llX\n", keyCode);
    }
    sqlite3* pdb = openDB((const char *)"auditlog.db");

    getCodeDetails(pdb, mode, keyCode, userId, isAuthorized, reason);
    dbLogAccessAttempt(pdb, mode, keyCode, isAuthorized, reasonMsgs[reason], userId, NULL, 0);
    netLogAccessAttempt("http://www.toyshed.net/postAccess.php", mode, keyCode, isAuthorized, reasonMsgs[reason], userId, NULL, 0);
    sendEmail_handler(NULL, NULL, NULL);

    closeDB(pdb);

    if (isAuthorized) {
        timer->pattern(KEYPAD_BUZZER_PIN, pattern_L, 0, 1);
        timer->latch(DOOR_STRIKE_PIN, 5000000L, 1);
    } else
        timer->pattern(KEYPAD_BUZZER_PIN, pattern_SS, 0, 1);

    return isAuthorized;
}

static void *keypadThread(void *data) {
    uint64_t keyCode = 0;
    time_t lastKeyPressTime = 0;

    timer = new Timer();
    if (timer == NULL) {
        syslog(LOG_ERR, "Error initializing timer system.\n" );
        return NULL;
    }

    while (1) {
        // check keypad stuff
        if ((lastKeyPressTime + 20 < time(NULL)) && (keyCode != 0)) {
            keyCode = 0;
            timer->pattern(KEYPAD_BUZZER_PIN, pattern_FFPFF, 0, 1);
        }
        if (wiegandGetPendingBitCount())
        {
            uint64_t keyData = 0;
            int bits = wiegandReadData(&keyData, sizeof(keyData));
            syslog(LOG_INFO, "Keypad: %d: %llX\n", bits, keyData);
            if (bits > 16) {
                doAuthorization(ACCESS_RFID, keyData);
            } else if (bits == 4) {
                if (keyData == 10) { // ESC
                    keyCode = 0;
                    timer->pattern(KEYPAD_BUZZER_PIN, pattern_FF, 0, 1);
                } else if (keyData == 11) { // ENT
                    doAuthorization(ACCESS_PIN, keyCode);
                    keyCode = 0;
                } else {
                    keyCode = (keyCode * 10) + keyData;
                    syslog(LOG_INFO, "Code: %llu\n", keyCode);
                }
                time(&lastKeyPressTime);
            }
        }
        usleep( 5000L );
    }
    return NULL;
}



void initPiGPIO()
{
    // determine timezone offset for clocks


    syslog(LOG_NOTICE, "Initializing Pi GPIO...");
    if (wiringPiSetup() != 0)
    {
        syslog(LOG_ERR, "Error initializing Pi GPIO.");
        exit(1);
    }
    pinMode(KEYPAD_DATA0_PIN, INPUT);  // Data 0
    pinMode(KEYPAD_DATA1_PIN, INPUT);  // Data 1

    pinMode(KEYPAD_BUZZER_PIN, OUTPUT); // Buzzer
    digitalWrite(KEYPAD_BUZZER_PIN, 1); // HIGH = OFF, LOW = ON

    pinMode(KEYPAD_LED_PIN, OUTPUT); // LED
    digitalWrite(KEYPAD_LED_PIN, 1); // HIGH = BLUE/default, LOW = GREEN

    pinMode(DOOR_STRIKE_PIN, OUTPUT); // Door Strike
    digitalWrite(DOOR_STRIKE_PIN, 0); // HIGH = RELEASE, LOW = CATCH

    pinMode(CAMERA_FLASH_PIN, OUTPUT); // Camera Flash
    digitalWrite(CAMERA_FLASH_PIN, 0); // HIGH = ON, LOW = OFF

    pinMode(DISPLAY_POWER_PIN, OUTPUT); // Display Power
    digitalWrite(DISPLAY_POWER_PIN, 0); // HIGH = ON, LOW = OFF
}

int standup() {
    graphicsOptions.faceDetectEnabled = 0;
    graphicsOptions.writeFacesEnabled = 0;

    initWebInterface();
    initPiGPIO();
    if ( wiegandInit(KEYPAD_DATA0_PIN, KEYPAD_DATA1_PIN) != 0) {
        syslog(LOG_ERR, "Error initializing wiegand interface." );
        return 1;
    }
    pthread_create(&keypadThread_id, NULL, &keypadThread, NULL);

    if ( initGraphics(camera_enabled) != 0) {
        syslog(LOG_ERR, "Error initializing graphics engine.");
        return 1;
    }

        syslog(LOG_ERR, "Error initializing graphics engine.");
	return 0;
}

int mainloop() {
    syslog(LOG_INFO, "Starting main process loop...");
    SDL_Surface *screen = getScreenSurface();
    while ( !g_done )
    {
        // message processing loop
        SDL_Event event;
        while ( SDL_PollEvent( &event ) )
        {
            // check for messages
            switch ( event.type )
            {
                // exit if the window is closed
            case SDL_QUIT:
                //g_done = true;
                break;

                // check for keypresses
            case SDL_KEYDOWN:
            {
                // exit if ESCAPE is pressed
                if ( event.key.keysym.sym == SDLK_ESCAPE )
                    g_done = true;
                break;
            }
            } // end switch
        } // end of message processing

        // DRAWING STARTS HERE

        // clear screen
        syslog(LOG_INFO, "Drawing screen...");
        SDL_FillRect( screen, 0, SDL_MapRGB( screen->format, 0, 0, 0 ) );

		// draw the various UI elements
		if (camera_enabled)
            drawCameraPreview();
        drawClock();
        drawAccessHistory();

        // finally, update the screen :)
        SDL_Flip( screen );
//	usleep( 50000L );
	sleep(1);
    } // end main loop
    syslog(LOG_INFO, "Exiting main process loop...");

	return 0;
}

int teardown() {
    if (g_cam != NULL) {
        syslog(LOG_INFO, "Shutting down camera...");
        StopCamera();
    }

    // free loaded bitmap
    syslog(LOG_INFO, "Releasing surfaces...");
    // SDL_FreeSurface( camImage );
    SDL_FreeSurface( getScreenSurface() );

    syslog(LOG_INFO, "Releasing Timers...");
    delete timer;

    // all is well ;)
    SDL_Quit();
    syslog(LOG_INFO, "Exited cleanly" );
    closelog();

	if (pfh != NULL)
		pidfile_remove(pfh);

	exit(EXIT_SUCCESS);
	return 0;
}

int daemonize() {
	// build PID file
	pid_t otherpid, childpid;

	pfh = pidfile_open("/var/run/pilock.pid", 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST) {
			fprintf(stderr, "PiLock is already running, pid: %jd.", (intmax_t)otherpid);
			exit(EXIT_FAILURE);
		}
		// If we cannot create pidfile from other reasons, bail.
		fprintf(stderr, "Cannot open or create pidfile.");
	}

	// fork and daemonize
	if (daemon(true, false) == -1) {
		fprintf(stderr, "Unable to daemonize.");
		pidfile_remove(pfh);
		exit(EXIT_FAILURE);
	}

	// setup signal handlers
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sig_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGINT, &action, NULL);

	childpid = fork();
	if (childpid < 0) {
		fprintf(stderr, "Unable to detatch...");
		pidfile_remove(pfh);
		exit(EXIT_FAILURE);
	}
	if (childpid > 0) {
		fprintf(stderr, "Successful fork, parent terminating.");
		exit(EXIT_SUCCESS);
	}

	// the process that continues from there (and from any code following the execution of this function) is doing so as the final daemonized child
	pidfile_write(pfh);
	syslog(LOG_INFO, "Successful fork, %jd started.", (intmax_t)childpid);

	chdir("/home/keymaster/PiLock");

	return 0;
}

int stupidTest() {
	int img_buff_size = 0;
	void *img_buff = NULL;
	CCamera* g_cam = NULL;

	g_cam = StartCamera(1280,1024, 30, 4, true);
	if (g_cam != NULL) {

		int frame_width = g_cam->getOutput(0)->Width;
		int frame_height = g_cam->getOutput(0)->Height;

		img_buff_size = frame_width * frame_height * 4;
		img_buff = cvAlloc(img_buff_size);

		if (img_buff != NULL) {
			g_cam->getOutput(0)->ReadFrame(img_buff, img_buff_size);
			IplImage* src_img = cvCreateImageHeader(cvSize(frame_width, frame_height), IPL_DEPTH_8U, 4);
			cvSetImageData(src_img, img_buff, frame_width * 4);
		} else {
			syslog(LOG_ERR, "Unable to allocate memory for image.\n");
			img_buff_size = 0;
		}

		StopCamera();
	} else {
		syslog(LOG_ERR, "Unable to access camera.\n");
	}

	// put crap in the access log
	sqlite3 *pDb = NULL;
	if ((pDb = openDB("auditlog.db")) != NULL) {
		dbLogAccessAttempt(pDb, ACCESS_RFID, "554FD0294BA", true, msgAccessGranted, 0, img_buff, img_buff_size);
		closeDB(pDb);
	}

	// try and log to the web
	netLogAccessAttempt("http://www.toyshed.net/postAccess.php", ACCESS_RFID, 0x554FD0294BA, true, reasonMsgs[0], 0, img_buff, img_buff_size);

	return 0;
}

int main ( int argc, char** argv ) {
	int foreground = 0;
	int i = 1;

	setlogmask(LOG_UPTO (LOG_NOTICE)); // get this value from the config table in the database

	while (i < argc) {

		if (!strcmp("--fg",argv[i]) || !strcmp("--foreground",argv[i]))
			foreground = 1;

		if (!strcmp("--debug",argv[i])) {
            setlogmask(LOG_UPTO (LOG_DEBUG)); // get this value from the config table in the database
        }

		if (!strcmp("--use-camera",argv[i])) {
            camera_enabled = true;
        }

        if (!strcmp("--auth", argv[i]) && (i+1 < argc)) {
            openlog("keymaster", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
            initWebInterface();
            initPiGPIO();
                int keyCode = atoi(argv[i+1]);
                sleep(1);
                if (doAuthorization(ACCESS_PIN, keyCode)) {
                    printf("Authorization successful.");
                } else {
                    printf("Authorization unsuccessful.");
                }
            exit(EXIT_SUCCESS);
        }

		if (!strcmp("--test",argv[i])) {
			stupidTest();
			exit(EXIT_SUCCESS);
		}

		i++;
	}

	if (foreground) {
		openlog("keymaster", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
	} else {
		daemonize();
		openlog("keymaster", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
	}

    // Send a "start-up" email messages to the admin that the PiLock app has been started
    time_t t = time(NULL);
    tm* localTime = localtime(&t);
    char timestamp[22];
    sprintf(timestamp, "[%4d-%02d-%02d %02d:%02d:%02d]",
                    localTime->tm_year + 1900, localTime->tm_mon + 1, localTime->tm_mday,
                    localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
	char msgLine[144] = "";
	char *body[] = { msgLine, NULL };

    sprintf(msgLine, "[TOYSHED] %s : PiLock was started.", timestamp);
	sendMMSemail(fromAddr, toAddr, body, NULL);


	if (standup() == 0)
		mainloop();
	return teardown();
}
