#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#ifdef __cplusplus
#include <cstdlib>
#include <string>
#include <iostream>
#include <vector>
#else
#include <stdlib.h>
#endif

#include <stdio.h>
#include <fcntl.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/kd.h>
//#include <syslog.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_rotozoom.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_surface.h>

#include <jpeglib.h>

// #include "camera.h"

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv/ml.h>
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

class FaceDetails {
    SDL_Surface *camImage;
    SDL_Surface *previewImage;
    std::vector<SDL_Rect> faceRects;

    int getFaceCount();
    SDL_Surface* getFaceImage(int index);
    SDL_Rect* getFaceRect(int index);

    void freeMemory();
};

int initTTFonts();
int initFaceDetector();
int initGraphics(bool camera_enabled);
int initCamera();
int detectFaces(SDL_Rect* faceRects, int maxFaces);

void getSDLInfo(int options);

SDL_Surface* getCameraImage(int scaledWidth, int scaledHeight);

void updateRenderer();
void clearScreenSurface();

void drawAccessHistory();
int drawCameraPreview();
void drawClock();

void sdlSurfaceToJPEG(SDL_Surface* surface, int jpegQuality, void** jpegBuffer, unsigned long* jpegLen);

void teardownGraphics();

typedef struct _graphicsOptions {
    bool headless;
    bool faceDetectEnabled;
    bool writeFacesEnabled;
} td_graphicsOptions;

extern td_graphicsOptions graphicsOptions;


#endif

