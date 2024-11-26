cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_C_COMPILER "arm-linux-gnueabihf-gcc")
#set(CMAKE_CXX_COMPILER "arm-linux-gnueabihf-g++")
#set(CMAKE_SYSTEM_PROCESSOR armv7)


include_directories("./include")
include_directories("/home/chayan/python_webrtc_client/libwebrtc_125/include")
include_directories("/home/chayan/python_webrtc_client/libwebrtc_125/include/third_party/abseil-cpp")
#include_directories(${GLIB_INCLUDE_DIRS})

link_directories("/home/chayan/python_webrtc_client/libwebrtc_125/lib")
#link_directories(${GLIB_LIBRARY_DIRS})

#add_definitions(${GLIB_CFLAGS_OTHER})

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  # macOS
  file(READ ${LIBWEBRTC_PATH}/exports_libwebrtc.txt webrtc_libs)
  string(REGEX REPLACE "lib([^.]+).a[\r\n]*" "\\1;" webrtc_libs "${webrtc_libs}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBRTC_MAC=1 -DWEBRTC_POSIX=1")
  find_library(CORE_FOUNDATION CoreFoundation)
  list(APPEND webrtc_libs ${CORE_FOUNDATION})
  find_library(FOUNDATION Foundation)
  list(APPEND webrtc_libs ${FOUNDATION})
  find_library(CORE_AUDIO CoreAudio)
  list(APPEND webrtc_libs ${CORE_AUDIO})
  find_library(AUDIO_TOOLBOX AudioToolbox)
  list(APPEND webrtc_libs ${AUDIO_TOOLBOX})
  find_library(CORE_GRAPHICS CoreGraphics)
  list(APPEND webrtc_libs ${CORE_GRAPHICS})

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  # Linux
#  find_package(PkgConfig REQUIRED)
#  pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0)
  find_package(PkgConfig)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")

  list(APPEND webrtc_libs "-Wl,--start-group")
  #file(READ ${LIBWEBRTC_PATH}/exports_libwebrtc.txt webrtc_read_tmp)
  #string(REGEX REPLACE "lib([^.]+).a[\r\n]*" "\\1;" webrtc_read_tmp "${webrtc_read_tmp}")
  #list(APPEND webrtc_libs "${webrtc_read_tmp}")
  list(APPEND webrtc_libs "/home/chayan/python_webrtc_client/libwebrtc_125/lib/libwebrtc.a")
  list(APPEND webrtc_libs "-Wl,--end-group")

  pkg_search_module(X11 REQUIRED x11)
  list(APPEND webrtc_libs ${X11_LIBRARIES})
  list(APPEND webrtc_libs "dl")
  list(APPEND webrtc_libs "pthread")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBRTC_LINUX=1 -DWEBRTC_POSIX=1")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wno-unused-parameter -fno-rtti -std=c++17 ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")

include(CMakePrintHelpers)
message(WARNING "chayan")
cmake_print_variables(webrtc_libs)
cmake_print_variables(CMAKE_CXX_FLAGS)

add_library(webrtc SHARED src/libwebrtc.cpp)

target_link_libraries(webrtc
  ${webrtc_libs}
  -std=gnu++17
#  ${GLIB_LIBRARIES}
  )

