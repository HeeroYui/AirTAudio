cmake_minimum_required(VERSION 2.8.3)
project(audio_orchestra)

set(CMAKE_VERBOSE_MAKEFILE ON)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  etk
  audio
  )

find_package(ALSA REQUIRED)

###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if you package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(
  INCLUDE_DIRS ../
  LIBRARIES ${PROJECT_NAME}
  CATKIN_DEPENDS etk audio
  DEPENDS system_lib
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
  ..
  ${catkin_INCLUDE_DIRS}
)

## Declare a cpp library
add_library(${PROJECT_NAME}
  ../audio/orchestra/debug.cpp
  ../audio/orchestra/status.cpp
  ../audio/orchestra/type.cpp
  ../audio/orchestra/mode.cpp
  ../audio/orchestra/state.cpp
  ../audio/orchestra/error.cpp
  ../audio/orchestra/base.cpp
  ../audio/orchestra/Interface.cpp
  ../audio/orchestra/Flags.cpp
  ../audio/orchestra/Api.cpp
  ../audio/orchestra/DeviceInfo.cpp
  ../audio/orchestra/StreamOptions.cpp
  ../audio/orchestra/api/Dummy.cpp
  ../audio/orchestra/api/Alsa.cpp
  ../audio/orchestra/api/Jack.cpp
  ../audio/orchestra/api/Pulse.cpp
  ../audio/orchestra/api/Oss.cpp
)

add_definitions(-D__LINUX_ALSA__)
add_definitions(-D__DUMMY__)

## Add cmake target dependencies of the executable/library
## as an example, message headers may need to be generated before nodes
#add_dependencies(${PROJECT_NAME} test_perfo_core_generate_messages_cpp)

## Specify libraries to link a library or executable target against
target_link_libraries(${PROJECT_NAME}
  ${ALSA_LIBRARIES}
  ${catkin_LIBRARIES}
)

#############
## Install ##
#############

## Mark executables and/or libraries for installation
install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
)

## Mark cpp header files for installation
install(DIRECTORY ../audio/orchestra/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h"
)
