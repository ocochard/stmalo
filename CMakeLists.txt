cmake_minimum_required(VERSION 3.16)
project(flibustier VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
	set(FLIBUSTIER_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Strict C99 hides mkdtemp() and friends in the macOS headers.
if(APPLE)
	add_compile_definitions(_DARWIN_C_SOURCE)
endif()

# Engine core: data access and asset decoding, no raylib dependency, so
# headless tools and tests can use it.
add_library(flibustier_core STATIC
	src/data/exe.c
	src/data/fsys.c
	src/data/progdir.c
	src/data/userlang.c
	src/data/iso9660.c
	src/data/pack.c
	src/data/sha256.c
	src/assets/font.c
	src/assets/fonts.c
	src/assets/image.c
	src/assets/img.c
	src/assets/pcx.c
	src/assets/digital.c
	src/assets/fmmusic.c
	src/audio/music.c
	src/audio/opl3.cpp
	src/external/ymfm/ymfm_opl.cpp
	src/external/ymfm/ymfm_adpcm.cpp
	src/external/ymfm/ymfm_pcm.cpp
	src/assets/pic.c
	src/assets/png.c
	src/assets/rsd.c
	src/assets/sprite.c
	src/assets/credits.c
	src/assets/text.c
	src/assets/textfiles.c
	src/assets/tileset.c
	src/assets/tiles.c
	src/assets/scenario.c
	src/assets/uitables.c
	src/assets/datafields.c
	src/assets/sheet.c
	src/assets/tiled.c
	src/assets/worldfiles.c
	src/assets/towns.c
	src/assets/world.c
	src/sim/battle.c
	src/sim/sword.c
	src/sim/land.c
	src/sim/treasure.c
	src/sim/mission.c
	src/sim/crew.c
	src/sim/prison.c
	src/sim/town.c
	src/sim/hof.c
	src/sim/json.c
	src/sim/proto.c
	src/sim/rng.c
	src/sim/save.c
	src/sim/sim.c
	src/gfx/fb.c
	src/gfx/sea.c
	src/gfx/text.c
	src/ui/game.c
	src/ui/screens.c
	src/ui/script.c
	src/view/view.c)
target_include_directories(flibustier_core PUBLIC src)
target_compile_options(flibustier_core PRIVATE ${FLIBUSTIER_WARNINGS})
if(APPLE)
	target_link_libraries(flibustier_core PRIVATE "-framework CoreFoundation")
endif()
# The vendored stb_image is not ours to keep warning-free.
set_source_files_properties(src/assets/png.c PROPERTIES COMPILE_OPTIONS -w)
# A pack's sheets are large pictures, decoded and reduced to the game's
# colours whenever a screen loads them: optimised even in a Debug build,
# where a sword fight took seconds to start.
set_property(SOURCE src/assets/png.c src/assets/image.c src/assets/sheet.c
	APPEND PROPERTY COMPILE_OPTIONS $<$<CONFIG:Debug>:-O2>)
# The vendored ymfm (an OPL3 emulator, C++14) neither: the only C++ in the
# engine, with its C wrapper.
set_source_files_properties(src/external/ymfm/ymfm_opl.cpp
	src/external/ymfm/ymfm_adpcm.cpp src/external/ymfm/ymfm_pcm.cpp
	PROPERTIES COMPILE_OPTIONS -w)
set_source_files_properties(src/audio/opl3.cpp PROPERTIES COMPILE_OPTIONS
	-Wno-unused-parameter)
set_property(TARGET flibustier_core PROPERTY CXX_STANDARD 14)

# A package links its own raylib and FreeType in, so players need not
# install them: FLIBUSTIER_BUNDLED_DEPS skips the system's.
option(FLIBUSTIER_BUNDLED_DEPS "Build raylib and FreeType in, statically"
    OFF)

# FreeType draws the open pack's fonts: hardened for untrusted font files,
# which packs from third parties are.  Use a system FreeType when
# available, otherwise fetch a pinned release.
if(NOT FLIBUSTIER_BUNDLED_DEPS)
	find_package(Freetype QUIET)
endif()
if(FREETYPE_FOUND)
	target_link_libraries(flibustier_core PRIVATE Freetype::Freetype)
else()
	message(STATUS "System FreeType not found, fetching FreeType 2.14.3")
	include(FetchContent)
	set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
	FetchContent_Declare(freetype
		GIT_REPOSITORY https://github.com/freetype/freetype.git
		GIT_TAG VER-2-14-3
		GIT_SHALLOW TRUE)
	FetchContent_MakeAvailable(freetype)
	target_link_libraries(flibustier_core PRIVATE freetype)
endif()
if(UNIX)
	target_link_libraries(flibustier_core PRIVATE m)
endif()

# Use a system raylib 5.5 or later when available (CMake config or
# pkg-config), otherwise fetch a pinned release.
set(FLIBUSTIER_RAYLIB raylib)
if(NOT FLIBUSTIER_BUNDLED_DEPS)
	find_package(raylib QUIET)
endif()
if(raylib_FOUND AND raylib_VERSION AND raylib_VERSION VERSION_LESS 5.5)
	set(raylib_FOUND FALSE)
endif()
if(NOT raylib_FOUND AND NOT FLIBUSTIER_BUNDLED_DEPS)
	find_package(PkgConfig QUIET)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(RAYLIB QUIET IMPORTED_TARGET raylib>=5.5)
		if(RAYLIB_FOUND)
			set(FLIBUSTIER_RAYLIB PkgConfig::RAYLIB)
			set(raylib_FOUND TRUE)
		endif()
	endif()
endif()
if(NOT raylib_FOUND)
	message(STATUS "System raylib not found, fetching raylib 5.5")
	include(FetchContent)
	set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(raylib
		GIT_REPOSITORY https://github.com/raysan5/raylib.git
		GIT_TAG 5.5
		GIT_SHALLOW TRUE)
	FetchContent_MakeAvailable(raylib)
endif()

# Some raylib builds (FreeBSD raylib-6.0 package) omit libm and pthreads
# from their own dependencies, so link them explicitly.
find_package(Threads REQUIRED)
add_executable(flibustier src/main.c src/play.c src/crt.c src/viewer.c
    src/soundexport.c src/console.c)
# Windows: no console window beside the game's (see src/console.h).
set_target_properties(flibustier PROPERTIES WIN32_EXECUTABLE ON)
# macOS: Flibustier.app, the pack in its Resources (see Install).
if(APPLE)
	set_target_properties(flibustier PROPERTIES
	    MACOSX_BUNDLE ON
	    OUTPUT_NAME Flibustier
	    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/Info.plist.in
	    MACOSX_BUNDLE_GUI_IDENTIFIER org.flibustier.Flibustier
	    MACOSX_BUNDLE_BUNDLE_NAME Flibustier
	    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
	    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
endif()
target_link_libraries(flibustier PRIVATE flibustier_core ${FLIBUSTIER_RAYLIB}
	Threads::Threads)
if(UNIX)
	target_link_libraries(flibustier PRIVATE m)
endif()
target_compile_options(flibustier PRIVATE ${FLIBUSTIER_WARNINGS})
# The program's icon (desktop/, made by tools/icons.py): a resource of the
# .exe on Windows, from the bundle on macOS, elsewhere a PNG the game hands
# its window.
if(WIN32)
	enable_language(RC)
	target_sources(flibustier PRIVATE desktop/flibustier.rc)
	set_source_files_properties(desktop/flibustier.rc PROPERTIES
	    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/desktop/flibustier.ico)
elseif(NOT APPLE)
	set(fl_icon ${CMAKE_SOURCE_DIR}/desktop/icons/flibustier-64.png)
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	    ${fl_icon})
	file(READ ${fl_icon} fl_hex HEX)
	string(REGEX REPLACE "(..)" "0x\\1," fl_hex "${fl_hex}")
	file(WRITE ${CMAKE_BINARY_DIR}/gen/window_icon.h
	    "static const unsigned char window_icon_png[] = {${fl_hex}};\n")
	target_include_directories(flibustier PRIVATE ${CMAKE_BINARY_DIR}/gen)
	target_compile_definitions(flibustier PRIVATE HAVE_WINDOW_ICON)
endif()

# Headless simulation, driven by JSON lines (bots, AI agents, tests).
add_executable(flibustier-sim src/sim_main.c)
target_link_libraries(flibustier-sim PRIVATE flibustier_core)
target_compile_options(flibustier-sim PRIVATE ${FLIBUSTIER_WARNINGS})

# The career bot's sea map (tools/career-bot).
add_executable(flibustier-seamap tools/career-bot/seamap.c)
target_link_libraries(flibustier-seamap PRIVATE flibustier_core)
target_compile_options(flibustier-seamap PRIVATE ${FLIBUSTIER_WARNINGS})

# Install.  A portable install (a zip: the default on Windows) puts the
# programs, the open pack and the documents together in the prefix; a
# system install puts the programs in bin and the pack in
# share/flibustier/openpack, where the game looks for it from bin.
# FLIBUSTIER_OPENPACK names an open pack checkout to install (none by
# default); its remade music, which belongs to its author, goes with it
# unless FLIBUSTIER_OPENPACK_MUSIC is off.  The
# fetched raylib and FreeType add their own headers and libraries to a
# plain install: a package takes the "game" component alone
# (cmake --install build --component game).
include(GNUInstallDirs)
option(FLIBUSTIER_PORTABLE "Install everything in one directory" ${WIN32})
set(FLIBUSTIER_OPENPACK "" CACHE PATH "Open pack checkout to install")
option(FLIBUSTIER_OPENPACK_MUSIC
    "Install the open pack's remade music" ON)
if(APPLE)
	# The .app beside the documents; the game finds its pack in
	# Contents/Resources from Contents/MacOS.
	set(fl_bin .)
	set(fl_data Flibustier.app/Contents/Resources)
	set(fl_doc .)
elseif(FLIBUSTIER_PORTABLE)
	set(fl_bin .)
	set(fl_data .)
	set(fl_doc .)
else()
	set(fl_bin ${CMAKE_INSTALL_BINDIR})
	set(fl_data ${CMAKE_INSTALL_DATADIR}/flibustier)
	set(fl_doc ${CMAKE_INSTALL_DOCDIR})
	file(RELATIVE_PATH fl_rel /${fl_bin} /${fl_data})
	target_compile_definitions(flibustier PRIVATE
	    DATA_FROM_BIN="${fl_rel}")
endif()
if(APPLE)
	install(TARGETS flibustier BUNDLE DESTINATION ${fl_bin}
	    COMPONENT game)
else()
	install(TARGETS flibustier flibustier-sim DESTINATION ${fl_bin}
	    COMPONENT game)
endif()
install(FILES LICENSE README.md MANUAL.md DESTINATION ${fl_doc}
    COMPONENT game)
# The desktop menu entry and its icons, for a system install on Unix.
if(UNIX AND NOT APPLE AND NOT FLIBUSTIER_PORTABLE)
	install(FILES desktop/flibustier.desktop
	    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT game)
	install(FILES desktop/flibustier.svg
	    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
	    COMPONENT game)
	foreach(n 16 24 32 48 64 128 256 512)
		install(FILES desktop/icons/flibustier-${n}.png
		    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${n}x${n}/apps
		    RENAME flibustier.png COMPONENT game)
	endforeach()
endif()
if(FLIBUSTIER_OPENPACK)
	if(NOT EXISTS ${FLIBUSTIER_OPENPACK}/pack.ini)
		message(FATAL_ERROR
		    "FLIBUSTIER_OPENPACK: no pack.ini in ${FLIBUSTIER_OPENPACK}")
	endif()
	# What playing needs: provenance and the coverage report stay in
	# the pack's repository.
	set(fl_skip PATTERN provenance EXCLUDE PATTERN COVERAGE.md EXCLUDE
	    PATTERN .git* EXCLUDE)
	if(NOT FLIBUSTIER_OPENPACK_MUSIC)
		list(APPEND fl_skip PATTERN music.ini EXCLUDE
		    PATTERN CREDITS.txt EXCLUDE)
		file(STRINGS ${FLIBUSTIER_OPENPACK}/pack.d/music.ini fl_tunes
		    REGEX "^music/.*=")
		foreach(t ${fl_tunes})
			string(REGEX REPLACE ".*= *music/([^.]*).*" "\\1" t "${t}")
			list(APPEND fl_skip REGEX "/music/${t}\\.[^/]*$" EXCLUDE)
		endforeach()
	endif()
	install(DIRECTORY ${FLIBUSTIER_OPENPACK}/
	    DESTINATION ${fl_data}/openpack COMPONENT game ${fl_skip})
endif()

# macOS: stripping and the added pack leave the .app with no valid
# signature, and Apple Silicon will not start unsigned code: seal it ad hoc
# (no developer account) once everything is in place.
if(APPLE)
	install(CODE "
		execute_process(COMMAND codesign --force --deep -s -
		    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/Flibustier.app\"
		    COMMAND_ERROR_IS_FATAL ANY)"
	    COMPONENT game)
endif()

# Packages (cpack in the build directory): the "game" component alone, in
# a zip on Windows and a tarball elsewhere, named for the engine's
# version and the open pack's (its git tag or commit).
set(fl_name flibustier-${PROJECT_VERSION})
if(FLIBUSTIER_OPENPACK)
	execute_process(COMMAND git -C ${FLIBUSTIER_OPENPACK} describe
	    --tags --always OUTPUT_VARIABLE fl_packver
	    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	if(fl_packver)
		string(APPEND fl_name -openpack-${fl_packver})
	endif()
endif()
string(TOLOWER ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR} fl_sys)
if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
	set(fl_sys darwin-universal)
endif()
set(CPACK_PACKAGE_NAME flibustier)
set(CPACK_PACKAGE_VENDOR "Olivier Cochard-Labbé")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Open source engine re-implementation of Pirates! Gold")
set(CPACK_PACKAGE_FILE_NAME ${fl_name}-${fl_sys})
if(WIN32)
	set(CPACK_GENERATOR ZIP)
elseif(APPLE)
	set(CPACK_GENERATOR DragNDrop)
else()
	set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL game)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)	# unzips to a folder
set(CPACK_STRIP_FILES ON)
include(CPack)

# Tests.  test_original needs the game data: set FLIBUSTIER_DATA to the GOG
# DATA.DAT (or an install directory) in the environment, otherwise it is
# reported as skipped.
include(CTest)
if(BUILD_TESTING)
	foreach(t test_formats test_original test_sim test_ui)
		add_executable(${t} tests/${t}.c)
		target_link_libraries(${t} PRIVATE flibustier_core)
		target_compile_options(${t} PRIVATE ${FLIBUSTIER_WARNINGS})
		add_test(NAME ${t} COMMAND ${t})
	endforeach()
	set_tests_properties(test_original test_ui PROPERTIES
	    SKIP_RETURN_CODE 77)
	file(GLOB replays ${CMAKE_SOURCE_DIR}/tests/replays/*.jsonl)
	foreach(r ${replays})
		get_filename_component(name ${r} NAME_WE)
		add_test(NAME replay_${name} COMMAND ${CMAKE_COMMAND}
		    "-DSIM=${CMAKE_CROSSCOMPILING_EMULATOR};$<TARGET_FILE:flibustier-sim>"
		    -DREPLAY=${r}
		    -P ${CMAKE_SOURCE_DIR}/tests/replay.cmake)
		set_tests_properties(replay_${name} PROPERTIES
		    SKIP_REGULAR_EXPRESSION "SKIP: ")
	endforeach()
endif()
