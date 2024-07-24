cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(retro-sound C CXX ASM)
SET(BUILD_NAME "${PROJECT_NAME}")


# Run from SRAM
set(PICO_COPY_TO_RAM,1)
set(PICO_DEFAULT_BINARY_TYPE copy_to_ram)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#add_compile_options(-flto)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -funroll-loops  -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -Ofast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops  -ffast-math -feliminate-unused-debug-types -ffunction-sections -fdata-sections -Ofast")

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

pico_sdk_init()

# INCLUDE FILES THAT SHOULD BE COMPILED:
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

message(STATUS "Add source files:")
foreach (SRC_FILE IN LISTS SRC)
	message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")

add_executable(${PROJECT_NAME} ${SRC})

pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
pico_set_boot_stage2(${PROJECT_NAME} slower_boot2)


set(FAMILY rp2040)
set(BOARD pico_sdk)

target_include_directories(${PROJECT_NAME} PRIVATE src)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
add_subdirectory(drivers/audio)
target_link_libraries(${PROJECT_NAME} PRIVATE
		audio

		pico_runtime
		pico_stdlib
		hardware_pio
		hardware_pwm
		hardware_dma
		pico_multicore

#		tinyusb_board
		tinyusb_device

)

target_compile_definitions(${PROJECT_NAME} PRIVATE
		PICO_HEAP_SIZE=0
		USE_ZONE_FOR_MALLOC=1
		PICO_FLASH_SIZE_BYTES=16777216
)

target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections --function-sections
		-Wl,-wrap,remquof
		-Wl,-wrap,floor
)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME})



