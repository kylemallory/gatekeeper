#include <graphics.h>
#include <database.h>

using namespace std;
using namespace cv;

const static SDL_Color colorWhite = { 255, 255, 255, 255 };
const static SDL_Color colorRed = { 255, 0, 0, 255 };
const static SDL_Color colorYellow = { 255, 255, 0, 255 };
const static SDL_Color colorGreen = { 0, 255, 0, 255 };
const static SDL_Color colorCyan = { 0, 255, 255, 255 };
const static SDL_Color colorBlue = { 0, 0, 255, 255 };
const static SDL_Color colorMagenta = { 255, 0, 255, 255 };
const static SDL_Color colorBlack = { 0, 0, 0, 255 };

CascadeClassifier cascade;
//CCamera* g_cam = NULL;
SDL_Window* screenWindow = NULL;
SDL_Texture* screenTexture = NULL;
SDL_Renderer* screenRenderer = NULL;
SDL_Surface* screenSurface = NULL;
SDL_Rect screenSize;
SDL_Rect faceRect[10];

TTF_Font* fontConsole = NULL;
TTF_Font* fontTimeBig = NULL;
TTF_Font* fontTimeSmall = NULL;
TTF_Font* fontLCD = NULL;

td_graphicsOptions graphicsOptions;
/*
void getSDLInfo(int options)
{
    const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
    fprintf( stderr, "hw: %d\n", video_info->hw_available );
    fprintf( stderr, "wm: %d\n", video_info->wm_available );
    fprintf( stderr, "blit_hw: %d\n", video_info->blit_hw );
    fprintf( stderr, "blit_hw_CC: %d\n", video_info->blit_hw_CC );
    fprintf( stderr, "blit_hw_A: %d\n", video_info->blit_hw_A );
    fprintf( stderr, "blit_sw: %d\n", video_info->blit_sw );
    fprintf( stderr, "blit_sw_CC: %d\n", video_info->blit_sw_CC );
    fprintf( stderr, "blit_sw_A: %d\n", video_info->blit_sw_A );
    fprintf( stderr, "blit_fill: %d\n", video_info->blit_fill );
    fprintf( stderr, "video_mem: %d\n", video_info->video_mem );
    fprintf( stderr, "current_w: %d\n", video_info->current_w );
    fprintf( stderr, "current_h: %d\n", video_info->current_h );
    fprintf( stderr, "vfmt->BitsPerPixel: %d\n", video_info->vfmt->BitsPerPixel );
    fprintf( stderr, "vfmt->BytesPerPixel: %d\n", video_info->vfmt->BytesPerPixel );
    fprintf( stderr, "vfmt->Rloss: %d\n", video_info->vfmt->Rloss );
    fprintf( stderr, "vfmt->Gloss: %d\n", video_info->vfmt->Gloss );
    fprintf( stderr, "vfmt->Bloss: %d\n", video_info->vfmt->Bloss );
    fprintf( stderr, "vfmt->Aloss: %d\n", video_info->vfmt->Aloss );
    fprintf( stderr, "vfmt->Rshift: %d\n", video_info->vfmt->Rshift );
    fprintf( stderr, "vfmt->Gshift: %d\n", video_info->vfmt->Gshift );
    fprintf( stderr, "vfmt->Bshift: %d\n", video_info->vfmt->Bshift );
    fprintf( stderr, "vfmt->Ashift: %d\n", video_info->vfmt->Ashift );
    fprintf( stderr, "vfmt->Rmask: %d\n", video_info->vfmt->Rmask );
    fprintf( stderr, "vfmt->Gmask: %d\n", video_info->vfmt->Gmask );
    fprintf( stderr, "vfmt->Bmask: %d\n", video_info->vfmt->Bmask );
    fprintf( stderr, "vfmt->Amask: %d\n", video_info->vfmt->Amask );
    fprintf( stderr, "vfmt->colorkey: %d\n", video_info->vfmt->colorkey );
    fprintf( stderr, "vfmt->alpha: %d\n", video_info->vfmt->alpha );

    // Get available fullscreen/hardware modes
    SDL_Rect** modes = SDL_ListModes( video_info->vfmt, options);

    // Check if there are any modes available
    if ( modes == ( SDL_Rect** )0 )
    {
        printf( "No modes available!\n" );
        exit( -1 );
    }

    // Check if our resolution is restricted
    if ( modes == ( SDL_Rect** ) - 1 )
    {
        printf( "All resolutions available.\n" );
    }
    else
    {
        // Print valid modes
        printf( "Available Modes\n" );
        for (int i = 0; modes[i]; ++i )
            printf( "  %d x %d\n", modes[i]->w, modes[i]->h );
    }
}
*/

int initTTFonts()
{

    // initialize TTF engine
    syslog(LOG_NOTICE, "Initializing TrueType Engine...\n");
    if ( TTF_Init() == -1 )
    {
        syslog(LOG_ERR, "Unable to init TTF engine.\n" );
        return 1;
    }

    syslog(LOG_NOTICE, "Loading Fonts...\n");
    fontConsole  = TTF_OpenFont( "fonts/kimberle.ttf", 24 );
    if ( fontConsole == NULL)
    {
	char path[512];
        syslog(LOG_ERR, "Unable to locate Console font.\n" );
	syslog(LOG_ERR, "Looking for fonts in %s\n", getcwd(path, 511));
        return 1;
    }

    fontTimeBig  = TTF_OpenFont( "fonts/kimberle.ttf", 80 );
    if ( fontTimeBig == NULL)
    {
        syslog(LOG_ERR, "Unable to locate Big-Time font.\n" );
        return 1;
    }

    fontTimeSmall  = TTF_OpenFont( "fonts/chintzy.ttf", 40 );
    if ( fontTimeSmall == NULL)
    {
        syslog(LOG_ERR, "Unable to locate Small-Time font.\n" );
        return 1;
    }
    syslog(LOG_NOTICE, "Finished loading fonts.\n");

    return 0;
}

int initFaceDetector()
{
    syslog(LOG_NOTICE, "Initializing Haar Cascade Classifier...\n");
    String cascade_name = "/usr/share/opencv/lbpcascades/lbpcascade_frontalface.xml";
    if (!cascade.load(cascade_name))
    {
        syslog(LOG_ERR, "Error loading face detection cascade classifier...\n");
        return 1;
    }
    return 0;
}

int detectFaces(SDL_Rect* faceRects, int maxFaces)
{
#ifdef CAMERA
    if (g_cam == NULL)
        return 0;

    std::vector<Rect> faces;
    int resLevel = 0;

    // find the lowest resolution image from the camera to perform face detection on.
    for (resLevel = 0; (resLevel < 4) && (g_cam->getOutput(resLevel) != NULL); resLevel++);
    if (resLevel > 3) resLevel = 3;

    int frame_width = g_cam->getOutput(resLevel)->Width;
    int frame_height = g_cam->getOutput(resLevel)->Height;

    // but we will scale all the results to fit the highest resolution image from the camera
    double scale_width = (double)g_cam->getOutput(0)->Width / (double)frame_width;
    double scale_height = (double)g_cam->getOutput(0)->Height / (double)frame_height;

    int img_buff_size = frame_width * frame_height * 4;
    void *img_buff = cvAlloc(img_buff_size);
    int i = 0;

    if (img_buff == NULL)
        return 0;

    g_cam->getOutput(resLevel)->ReadFrame(img_buff, img_buff_size);
    IplImage* src_img = cvCreateImageHeader(cvSize(frame_width, frame_height), IPL_DEPTH_8U, 4);
    cvSetImageData(src_img, img_buff, frame_width * 4);

    IplImage* src_gray = cvCreateImage (cvGetSize(src_img), IPL_DEPTH_8U, 1);
    cvCvtColor (src_img, src_gray, CV_BGR2GRAY);
    //cvEqualizeHist (src_gray, src_gray);

    cascade.detectMultiScale(src_gray, faces); // , 1.1, 2, 0, Size(64, 256));
    if (faceRects != NULL)
    {
        for (; (i < (int)faces.size()) && (i < maxFaces); i++)
        {
            // add to
            faceRects[i].x = faces[i].x * scale_width;
            faceRects[i].y = faces[i].y * scale_height;
            faceRects[i].w = faces[i].width * scale_width;
            faceRects[i].h = faces[i].height * scale_height;
        }
    }

    cvReleaseImage(&src_gray);
    cvReleaseImageHeader(&src_img);
    cvFree(&img_buff);

    return i;
#else
    return 0;
#endif
}

int initGraphics(bool camera_enabled)
{
    // also clean up any previously errant SDL states
    SDL_Quit();

    // initialize SDL video
    syslog(LOG_NOTICE, "Initializing SDL Framework...\n");
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        syslog(LOG_ERR, "Unable to init SDL: %s\n", SDL_GetError() );
        return 1;
    }
    // create a new window 720x480
    // getSDLInfo(SDL_FULLSCREEN);
    // make sure SDL cleans up before exit
    atexit( SDL_Quit );
    //const SDL_VideoInfo *video_info = SDL_GetVideoInfo();
    //syslog(LOG_NOTICE, "Setting the video mode (%dx%dx%dbpp, FULLSCREEN)\n", video_info->current_w, video_info->current_h, video_info->vfmt->BitsPerPixel);
    //screen = SDL_SetVideoMode( video_info->current_w, video_info->current_h, video_info->vfmt->BitsPerPixel, SDL_FULLSCREEN ); // SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN );
    screenWindow = SDL_CreateWindow("Gatekeeper", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_FULLSCREEN);
    if ( !screenWindow ) {
        syslog(LOG_ERR, "Unable to set video mode: %s\n", SDL_GetError() );
        return 1;
    }

    SDL_GetWindowSize(screenWindow, &screenSize.w, &screenSize.h);
    SDL_GetWindowPosition(screenWindow, &screenSize.x, &screenSize.y);

    screenRenderer = SDL_CreateRenderer(screenWindow, -1, 0);
    screenTexture = SDL_CreateTexture(screenRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, screenSize.w, screenSize.h);
    screenSurface = SDL_CreateRGBSurface(0, screenSize.w, screenSize.h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    // hide the cursor
    SDL_ShowCursor(0);

    if ( initTTFonts() != 0 ) {
        syslog(LOG_ERR, "Error initializing font engine.\n" );
        return 1;
    }

    //initialize camera (camera isn't required to run)
    if (camera_enabled) {
#ifdef CAMERA
        syslog(LOG_NOTICE, "Initializing Pi Camera...\n");
        g_cam = StartCamera(1280,1024, 30, 4, true);
        if (g_cam != NULL)
            initFaceDetector();
        else
            syslog(LOG_NOTICE, "Unable to initialize camera.  Continuing without it...");
#endif
    }

    return 0;
}

void teardownGraphics() {
    SDL_FreeSurface(screenSurface);
}

SDL_Surface* getCameraImage(int scaledWidth, int scaledHeight)
{
#ifdef CAMERA
    SDL_Surface* scaledBMP;
    if (g_cam != NULL) {
        // get camera image
        int i = 3;
        for (i = 3; i >= 0; i--)
            if (g_cam && g_cam->getOutput(i) && (g_cam->getOutput(i)->Width >= scaledWidth))
                break;

        i = 0;
        syslog(LOG_NOTICE, "Using Image Level: %d: Width=%d\n", i, g_cam->getOutput(i)->Width);
        SDL_Surface* camImage = SDL_CreateRGBSurface(0, g_cam->getOutput(i)->Width, g_cam->getOutput(i)->Height, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
        g_cam->ReadFrame(i, camImage->pixels, camImage->w*camImage->h*4);

        // printf("Scaling image...\n");
        // scale the image and locate it in the upper-right corner of the screen
        double scaleFactor = (double)scaledWidth / (double)camImage->w;
        //int scaledHeight = bmp->h * scaleFactor;
        scaledBMP = rotozoomSurface(camImage, 0, scaleFactor, 1);
        rectangleRGBA(scaledBMP, 0, 0, scaledBMP->w-1, scaledBMP->h-1, 255, 255, 255, 255);

        SDL_FreeSurface(camImage);
    } else {
        scaledBMP = SDL_CreateRGBSurface(0, scaledWidth, scaledHeight, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
    }

    return scaledBMP;
#else
    return NULL;
#endif
}

void clearScreenSurface() {
    SDL_FillRect( screenSurface, 0, SDL_MapRGB( screenSurface->format, 0, 0, 0 ) );
}

void updateRenderer() {
    SDL_UpdateTexture( screenTexture, NULL, screenSurface->pixels, screenSurface->pitch );
    SDL_RenderClear( screenRenderer );
    SDL_RenderCopy( screenRenderer, screenTexture, NULL, NULL );
    SDL_RenderPresent( screenRenderer );
}

int drawCameraPreview()
{
#ifdef CAMERA
    // SDL_Surface* cameraImage = getPiCameraImage( (screen->w / 2), (screen->h / 2) );
    int faces = 0;

    if (g_cam != NULL) {
        SDL_Surface* camImage = SDL_CreateRGBSurface(0, g_cam->getOutput(0)->Width, g_cam->getOutput(0)->Height, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
        g_cam->ReadFrame(0, camImage->pixels, camImage->w*camImage->h*4);

        // scale the image and locate it in the upper-right corner of the screen
        double scaleFactor = (double)(screen->w / 2.0) / (double)camImage->w;
        SDL_Surface* previewImage = rotozoomSurface(camImage, 0, scaleFactor, 1);
        rectangleRGBA(previewImage, 0, 0, previewImage->w-1, previewImage->h-1, 255, 255, 255, 255);

        if (graphicsOptions.faceDetectEnabled) {
            faces = detectFaces(faceRect, 10);
            while (faces--)
            {
                if (graphicsOptions.writeFacesEnabled) {
                    void *jpegBuffer = { 0 };
                    unsigned long jpegBufferLen = 0;
                    char datestr[40], facename[40];
                    time_t rawtime;
                    struct tm *timeinfo;

                    time(&rawtime);
                    timeinfo = localtime(&rawtime);
                    strftime(datestr, 40, "%Y%m%d-%H%M%S", timeinfo);

                    SDL_Surface* face = SDL_CreateRGBSurface(0, faceRect[faces].w, faceRect[faces].h, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
                    SDL_BlitSurface(camImage, &faceRect[faces], face, NULL);
                    sdlSurfaceToJPEG(face, 85, &jpegBuffer, &jpegBufferLen);

                    sprintf(facename, "faces/%s_%02d.jpg", datestr, faces);
                    int fd = open(facename, O_CREAT|O_WRONLY, 0664);
                    write(fd, jpegBuffer, jpegBufferLen);
                    close(fd);
                    syslog(LOG_INFO, "Wrote: %s", facename);

                    if (jpegBuffer != NULL) {
                        free(jpegBuffer);
                    }
                    SDL_FreeSurface(camImage);
                }

                faceRect[faces].x *= scaleFactor;
                faceRect[faces].y *= scaleFactor;
                faceRect[faces].w *= scaleFactor;
                faceRect[faces].h *= scaleFactor;
                rectangleRGBA(previewImage, faceRect[faces].x, faceRect[faces].y, faceRect[faces].x + faceRect[faces].w, faceRect[faces].y + faceRect[faces].h, 255, 255, 255, 255);
            }
        }

        SDL_Rect cameraDest = { screen->w - previewImage->w, 0, 0, 0 };
        SDL_BlitSurface( previewImage, NULL, screen, &cameraDest );
        SDL_FreeSurface( previewImage );
        SDL_FreeSurface( camImage );
    }

    return faces;
#else
    return 0;
#endif
}

void drawAccessHistory()
{
    struct stat dbStat;
    static time_t dbLastModified;
    static char auditLines[10][81];
    static int lines = -1;


    SDL_Rect consoleDest = { 0, screenSurface->h / 2 + 10, 0, 0 };


    stat("auditlog.db", &dbStat);
    if ((lines == -1) || (dbStat.st_mtime != dbLastModified)) {
	dbLastModified = dbStat.st_mtime;
    	lines = dbGetAuditLog(auditLines, 80, 10);
    }

    for (int i = 0; i < lines; i++) {
        SDL_Color c = { 255, 255, 255, 64 };
        SDL_Surface* textConsole = TTF_RenderText_Blended(fontConsole, auditLines[i], c);
        //rectangleRGBA( textConsole, 0, 0, textConsole->w-1, textConsole->h-1, 255, 255, 255, 255);
        consoleDest.y += textConsole->h;
        SDL_BlitSurface( textConsole, NULL, screenSurface, &consoleDest);
        SDL_FreeSurface( textConsole );
    }
}

void drawClock()
{
    char timeStr[32];
    time_t t = time(NULL);

    tm* localTime = localtime(&t);
    sprintf(timeStr, "%2d:%02d:%02d", localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
    SDL_Surface* textTime = TTF_RenderText_Blended(fontTimeBig, timeStr, colorYellow);
    SDL_Rect timeRect =
    {
        (screenSurface->w / 4) - (textTime->w / 2),
        0,
        textTime->w, textTime->h
    };
    //rectangleRGBA(textTime, 0, 0, textTime->w-1, textTime->h-1, 255, 255, 255, 255);
    SDL_BlitSurface( textTime, NULL, screenSurface, &timeRect);
    SDL_FreeSurface(textTime);

    // draw the date
    char dateStr[32];
    strftime(dateStr, 32, "%A, %b %e, %G", localTime);
    SDL_Surface* textDate = TTF_RenderText_Blended(fontConsole, dateStr, colorYellow);
    SDL_Rect dateRect =
    {
        (screenSurface->w / 4) - (textDate->w / 2),
        timeRect.y + timeRect.h + 10,
        textDate->w, textDate->h
    };
    //rectangleRGBA(textDate, 0, 0, textDate->w-1, textDate->h-1, 255, 255, 255, 255);
    SDL_BlitSurface( textDate, NULL, screenSurface, &dateRect);
    SDL_FreeSurface(textDate);
}

void sdlSurfaceToJPEG(SDL_Surface *m_surface, int jpegQuality, void **jpegBuffer, unsigned long *jpegLen) {
    SDL_Surface *surface;
	struct jpeg_compress_struct cinfo = {0};
	struct jpeg_error_mgr       jerr;
	int row_stride;

	SDL_PixelFormat fmt = *(m_surface->format);
	fmt.BitsPerPixel = 24;
	fmt.BytesPerPixel = 3;
	fmt.Rmask = 0x0000FF;
	fmt.Gmask = 0x00FF00;
	fmt.Bmask = 0xFF0000;
    fmt.Rshift=0;
    fmt.Gshift=8;
    fmt.Bshift=16;

	surface = SDL_ConvertSurface(m_surface, &fmt, 0);

	*jpegBuffer = NULL;
	*jpegLen = 0;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_mem_dest(&cinfo, (unsigned char **)jpegBuffer, jpegLen);

	cinfo.image_width      = (JDIMENSION) surface->w;
	cinfo.image_height     = (JDIMENSION) surface->h;
	cinfo.input_components = surface->format->BytesPerPixel;
	cinfo.in_color_space   = JCS_RGB;
	row_stride = surface->w * surface->format->BytesPerPixel;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, jpegQuality, true);
	jpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < cinfo.image_height) {
	    JSAMPROW rowStart = (JSAMPROW)(&((char *)surface->pixels)[cinfo.next_scanline * row_stride]);
		jpeg_write_scanlines(&cinfo, &rowStart, 1);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	SDL_FreeSurface(surface);
}
