cmake_minimum_required (VERSION 3.2.2)
set(PROJECT_NAME mame_launcher)
project ( ${PROJECT_NAME} C CXX )

if(MSVC)
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -O0 -D_FILE_OFFSET_BITS=64")
endif()

#Add pthread
find_package (Threads)

#Add expat
include(FindEXPAT)
find_package(EXPAT REQUIRED)
include_directories(${EXPAT_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} main.c linked_list.c misc.c  parse_data.c)
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${EXPAT_LIBRARIES})
