cmake_minimum_required(VERSION 3.12)
project(bralltog)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Copy assets at configure time (initial build)
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})

# Custom target to copy assets at build time (for hot reload)
add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_BINARY_DIR}/assets
    COMMENT "Copying assets to build directory..."
)

set(SDLTTF_FREETYPE_VENDORED ON)
set(SDLTTF_VENDORED ON )
set(SDLTTF_HARFBUZZ OFF)
set(SDLTTF_PLUTOSVG OFF)

#Don't forget to install the dependencies required by SDL. Not building the world 
# from scratch here.

include(FetchContent)
FetchContent_Declare(
	remoteSdl3
	GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
	GIT_TAG "prerelease-3.3.4"
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
	EXCLUDE_FROM_ALL
)
FetchContent_Declare(
	remoteSdl3Ttf
	GIT_REPOSITORY "https://github.com/libsdl-org/SDL_ttf.git"
	GIT_TAG "release-3.2.2"
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
	EXCLUDE_FROM_ALL
)

FetchContent_Declare(
	remoteSdl3Image
	GIT_REPOSITORY "https://github.com/libsdl-org/SDL_Image.git"
	GIT_TAG "release-3.2.4"
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
	EXCLUDE_FROM_ALL
)

FetchContent_Declare(
	remoteSdl3Mixer
	GIT_REPOSITORY "https://github.com/libsdl-org/SDL_Mixer.git"
	GIT_TAG "prerelease-3.1.2"
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(remoteSdl3)
FetchContent_MakeAvailable(remoteSdl3Ttf)
FetchContent_MakeAvailable(remoteSdl3Image)
FetchContent_MakeAvailable(remoteSdl3Mixer)

add_library(game SHARED game.c)
target_include_directories(game
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${remoteSdl3_SOURCE_DIR}/include
		${remoteSdl3Ttf_SOURCE_DIR}/include
		${remoteSdl3Image_SOURCE_DIR}/include
		${remoteSdl3Mixer_SOURCE_DIR}/include)

target_link_libraries(game PUBLIC SDL3::SDL3
                                  SDL3_mixer::SDL3_mixer
                                  SDL3_ttf::SDL3_ttf
                                  SDL3_image::SDL3_image 
                                  m)

add_executable(bralltog bralltog.c)

target_link_libraries(bralltog PUBLIC game)

set_target_properties(bralltog PROPERTIES BUILD_RPATH "$ORIGIN")

target_compile_definitions(bralltog PRIVATE SDL_MAIN_USE_CALLBACKS)

if (EMSCRIPTEN)
	#control memory
	target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -sINITIAL_MEMORY=16777216)

	# Output to index.html for easier deployments
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")

	# Create a html webpage
	set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY SUFFIX ".html")

	# Use custom emscripten shell
	target_link_options(${CMAKE_PROJECT_NAME}
			PRIVATE
			--shell-file ${CMAKE_SOURCE_DIR}/emscripten/shell.html
			--preload-file assets 
	)
endif ()
