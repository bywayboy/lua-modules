cmake_minimum_required(VERSION 2.6)

PROJECT(lmu)

SET(SOURCES
	src/lua-lmu.c
	src/lua-json.c
	src/lua-tpl.c
	src/automem.c
)

INCLUDE_DIRECTORIES(include src)
ADD_LIBRARY(lmu SHARED ${SOURCES})
TARGET_LINK_LIBRARIES(lmu lua)
 
INSTALL(TARGETS 
	lmu DESTINATION /usr/lib
)

