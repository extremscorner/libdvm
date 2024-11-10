cmake_minimum_required(VERSION 3.13)

project(dvm LANGUAGES C)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if(NINTENDO_DS AND ARM9)
		set(CMAKE_INSTALL_PREFIX "${NDS_ROOT}" CACHE PATH "" FORCE)
	elseif(NINTENDO_GBA)
		set(CMAKE_INSTALL_PREFIX "${GBA_ROOT}" CACHE PATH "" FORCE)
	elseif(NINTENDO_GAMECUBE OR NINTENDO_WII)
		set(CMAKE_INSTALL_PREFIX "${OGC_ROOT}" CACHE PATH "" FORCE)
		set(LIB_SUBDIR "/${OGC_SUBDIR}")
	else()
		message(FATAL_ERROR "Unknown platform")
	endif()
endif()

add_library(dvm STATIC)
set_target_properties(dvm PROPERTIES OUTPUT_NAME fat)

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/fatfs/CMakeLists.txt")
	include(FetchContent)
	FetchContent_Populate(FatFs
		GIT_REPOSITORY https://github.com/devkitPro/fatfs-mod
		GIT_TAG        dkp-mods
		SOURCE_DIR     ${CMAKE_SOURCE_DIR}/fatfs
	)
endif()

add_subdirectory(fatfs)

target_compile_options(dvm PRIVATE
	-Wall
)

if (NOT NINTENDO_GBA)
	target_compile_definitions(dvm PRIVATE
		# Disc I/O buffer alignment (should match CPU cache line size)
		LIBDVM_BUFFER_ALIGN=32
	)
else()
	target_compile_definitions(dvm PRIVATE
		# Disc I/O buffer alignment (should match CPU cache line size)
		LIBDVM_BUFFER_ALIGN=4
	)
endif()

target_include_directories(dvm PRIVATE
	include
)

target_link_libraries(dvm PRIVATE
	FatFs
)

target_sources(dvm PRIVATE
	source/dvm_disc.c
	source/dvm_cache.c
	source/dvm_volume.c
	source/dvm_prober.c

	source/fat_wrappers.c
	source/fat_driver.c
)

if(NINTENDO_DS)
	target_sources(dvm PRIVATE source/dvm_calico.c source/nitrofs.c)
	set(extra_install PATTERN filesystem.h)
	target_compile_definitions(dvm PRIVATE
		LIBDVM_WITH_CACHE_COPY
		LIBDVM_WITH_ALIGNED_ACCESS
	)
endif()

if(NINTENDO_GBA)
	target_sources(dvm PRIVATE source/dvm_libgba.c)
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
		PATTERN dvm.h
		PATTERN fat.h
		${extra_install}
)

# Install a dummy libfilesystem
if(NINTENDO_DS)
	add_library(filesystem STATIC include/filesystem.h)
	set_target_properties(filesystem PROPERTIES LINKER_LANGUAGE C)
	install(TARGETS filesystem ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
