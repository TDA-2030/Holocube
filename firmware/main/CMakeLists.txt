
set(CUBE_DIR "HoloCubic_AIO/HoloCubic_Firmware")

set(LIB_SRC_DIR 
${CUBE_DIR}/lib/Arduino-MPU6050
${CUBE_DIR}/lib/ArduinoJson-6.x/src
${CUBE_DIR}/lib/ESP32Time
${CUBE_DIR}/lib/font
${CUBE_DIR}/lib/I2Cdev
${CUBE_DIR}/lib/ESP32Time
${CUBE_DIR}/lib/pubsubclient/src
${CUBE_DIR}/lib/TJpg_Decoder/src
)

set(APP_SRC_DIR 
${CUBE_DIR}/src
# ${CUBE_DIR}/src/app/anniversary
# ${CUBE_DIR}/src/app/bilibili_fans
# ${CUBE_DIR}/src/app/file_manager
${CUBE_DIR}/src/app/audio_spectrum
# ${CUBE_DIR}/src/app/media_player
# ${CUBE_DIR}/src/app/picture
${CUBE_DIR}/src/app/screen_share
# ${CUBE_DIR}/src/app/server
# ${CUBE_DIR}/src/app/settings
${CUBE_DIR}/src/app/weather
${CUBE_DIR}/src/driver
${CUBE_DIR}/src/sys

)


idf_component_register(SRC_DIRS ${LIB_SRC_DIR} ${APP_SRC_DIR}
                    INCLUDE_DIRS ${LIB_SRC_DIR} ${CUBE_DIR}/src)

target_compile_definitions(${COMPONENT_LIB} INTERFACE ARDUINOJSON_EMBEDDED_MODE=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -w -Wno-class-memaccess -Wno-return-type -Wno-dangling-else -Wno-maybe-uninitialized -Wno-narrowing -Wno-parentheses)

# spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
