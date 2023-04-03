cmake_minimum_required(VERSION 3.13)

project(dvm LANGUAGES C)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if(NINTENDO_DS)
		set(CMAKE_INSTALL_PREFIX "${NDS_ROOT}" CACHE PATH "" FORCE)
	elseif(NINTENDO_GAMECUBE OR NINTENDO_WII)
		set(CMAKE_INSTALL_PREFIX "${OGC_ROOT}" CACHE PATH "" FORCE)
		set(LIB_SUBDIR "/${OGC_SUBDIR}")
	else()
		message(FATAL_ERROR "Unknown platform")
	endif()
endif()

add_library(dvm STATIC)

target_compile_options(dvm PRIVATE
	-Wall
)

target_include_directories(dvm PRIVATE
	include
)

target_sources(dvm PRIVATE
	source/dvm_disc.c
	source/dvm_cache.c
	source/dvm_volume.c
)

if(NINTENDO_DS)
	if(DKP_NDS_PLATFORM_LIBRARY STREQUAL "calico")
		target_compile_definitions(dvm PRIVATE LIBDVM_CALICO)
		target_sources(dvm PRIVATE source/dvm_calico.c)
	else()
		target_sources(dvm PRIVATE source/dvm_libnds.c)
	endif()
endif()

if(NINTENDO_GAMECUBE OR NINTENDO_WII)
	target_sources(dvm PRIVATE source/dvm_libogc.c)
endif()

include(GNUInstallDirs)

# Install the library
install(
	TARGETS dvm
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}${LIB_SUBDIR}
)

# Install the headers
install(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING
		PATTERN *.h
)
