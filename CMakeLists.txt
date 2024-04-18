set(SX126x_Arduino_srcs 
    src/boards/mcu/espressif/board.cpp
    src/boards/mcu/espressif/spi_board.cpp
    src/boards/mcu/espressif/timer.cpp
    src/boards/mcu/board.cpp
    src/boards/sx126x/sx126x-board.cpp
    src/mac/LoRaMac.cpp
    src/mac/LoRaMacCrypto.cpp
    src/mac/LoRaMacHelper.cpp
    src/mac/region/Region.cpp
    src/mac/region/RegionAS923.cpp
    src/mac/region/RegionAU915.cpp
    src/mac/region/RegionCN470.cpp
    src/mac/region/RegionCN779.cpp
    src/mac/region/RegionCommon.cpp
    src/mac/region/RegionEU433.cpp
    src/mac/region/RegionEU868.cpp
    src/mac/region/RegionIN865.cpp
    src/mac/region/RegionKR920.cpp
    src/mac/region/RegionRU864.cpp
    src/mac/region/RegionUS915.cpp
    src/radio/sx126x/radio.cpp
    src/radio/sx126x/sx126x.cpp
    src/system/utilities.cpp
    src/system/crypto/aes.cpp
    src/system/crypto/cmac.cpp
)

idf_component_register(SRCS ${SX126x_Arduino_srcs}
                      INCLUDE_DIRS "src"
                      REQUIRES arduino
                      )
#https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#controlling-component-compilation
#target_compile_options(${COMPONENT_LIB} PRIVATE -DCORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG)
