cmake_minimum_required(VERSION 3.0)
project(gatekeeper)

set(CMAKE_CXX_STANDARD 11)

set(CAMERA_SRC_FILES
    src/camera.cpp
    src/camera.h
    src/cameracontrol.cpp
    src/cameracontrol.h
    src/mmalincludes.h
    src/picamera.cpp)

set(UI_SRC_FILES
    src/graphics.cpp
    src/graphics.h )

add_compile_options(-fpermissive)
set(SOURCE_FILES
    src/assets.h
    src/database.cpp
    src/database.h
    src/Event.cpp
    src/Event.h
    src/main.cpp
    src/Timer.cpp
    src/Timer.h
    src/ts_util.cpp
    src/ts_util.h
    src/webinterface.cpp
    src/webinterface.h
    src/wiegand.cpp
    src/wiegand.h)

include_directories(src /usr/include /usr/local/include)

add_executable(gatekeeper ${SOURCE_FILES})