cmake_minimum_required(VERSION 3.21)

project(
	gbps1
	LANGUAGES    C CXX ASM
	VERSION      0.1.0
	DESCRIPTION  "Game Boy emulator for PS1 (Peanut-GB core)"
)

file(GLOB _sources ${PROJECT_SOURCE_DIR}/src/*.c)
psn00bsdk_add_executable(gbps1 GPREL ${_sources})

# PSn00bSDK heeft geen <time.h>; src/compat/time.h is een kleine shim voor
# peanut_gb.h (alleen `struct tm`).
target_include_directories(gbps1 PRIVATE ${PROJECT_SOURCE_DIR}/src/compat)

# Emulator-code altijd geoptimaliseerd bouwen (staat achteraan, dus wint van
# eventuele lagere standaardstand). Controleer met: cmake --build build -v
target_compile_options(gbps1 PRIVATE -O2 -fomit-frame-pointer)

# ---- Games automatisch uit roms/ ------------------------------------------
# Elk .gb-bestand in roms/ komt op de CD en in het menu. De menunaam komt uit
# de bestandsnaam; de naam op de CD wordt automatisch een korte 8.3-naam.
# Dit genereert:
#   - GB_ROM_ENTRIES: de <file>-regels voor iso.xml
#   - build/generated/games_gen.h: de lijst met games voor main.c
file(GLOB _rom_files CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/roms/*.[gG][bB]")
list(SORT _rom_files)

set(GB_ROM_ENTRIES "")
set(_games_c "")
set(_used_names "")
set(_count 0)

foreach(_path ${_rom_files})
	get_filename_component(_base "${_path}" NAME_WE)

	# Menunaam: hoofdletters, haakjes-tags en vreemde tekens weg, max 24 tekens.
	string(TOUPPER "${_base}" _title)
	string(REGEX REPLACE "\\([^)]*\\)" "" _title "${_title}")
	string(REGEX REPLACE "\\[[^]]*\\]" "" _title "${_title}")
	string(REPLACE "'" "" _title "${_title}")
	string(REGEX REPLACE "[^A-Z0-9]+" " " _title "${_title}")
	string(STRIP "${_title}" _title)
	string(LENGTH "${_title}" _len)
	if(_len GREATER 24)
		string(SUBSTRING "${_title}" 0 24 _title)
		string(STRIP "${_title}" _title)
	endif()
	if(_title STREQUAL "")
		set(_title "GAME")
	endif()

	# Naam op de CD: alleen A-Z en 0-9, max 8 tekens, uniek.
	string(TOUPPER "${_base}" _iso)
	string(REGEX REPLACE "[^A-Z0-9]" "" _iso "${_iso}")
	if(_iso STREQUAL "")
		set(_iso "GAME")
	endif()
	string(LENGTH "${_iso}" _len)
	if(_len GREATER 8)
		string(SUBSTRING "${_iso}" 0 8 _iso)
	endif()
	set(_n 1)
	while(_iso IN_LIST _used_names)
		string(SUBSTRING "${_iso}" 0 6 _short)
		math(EXPR _n "${_n} + 1")
		if(_n LESS 10)
			set(_iso "${_short}0${_n}")
		else()
			set(_iso "${_short}${_n}")
		endif()
	endwhile()
	list(APPEND _used_names "${_iso}")

	# Pad voor in XML (& wordt &amp;).
	string(REPLACE "&" "&amp;" _xml_path "${_path}")
	string(APPEND GB_ROM_ENTRIES
		"                        <file name=\"${_iso}.GB\" type=\"data\" source=\"${_xml_path}\" />\n")
	string(APPEND _games_c "\t{ \"${_title}\", \"\\\\${_iso}.GB;1\" },\n")

	message(STATUS "Game: ${_title}  (${_base}.gb -> ${_iso}.GB)")
	math(EXPR _count "${_count} + 1")
endforeach()

if(_count EQUAL 0)
	message(WARNING "Geen .gb-bestanden in roms/. Er komt geen CD-image, alleen gbps1.exe.")
	set(_games_c "\t{ \"NO GAMES\", \"\" },\n")
endif()

file(GENERATE
	OUTPUT "${CMAKE_BINARY_DIR}/generated/games_gen.h"
	CONTENT "/* Automatisch gemaakt door CMakeLists.txt - niet aanpassen. */\nstatic const struct game games[] = {\n${_games_c}};\n"
)
target_include_directories(gbps1 PRIVATE ${CMAKE_BINARY_DIR}/generated)

if(_count GREATER 0)
	psn00bsdk_add_cd_image(
		iso      # Target-naam
		gbps1    # Uitvoer: gbps1.bin + gbps1.cue
		iso.xml  # mkpsxiso-config
		DEPENDS gbps1 system.cnf ${_rom_files}
	)
endif()
