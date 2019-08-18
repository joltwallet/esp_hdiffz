#!/bin/bash

#######
# You may have to adjust these
ESP_PORT=/dev/ttyUSB0
ESP_BAUDRATE=921600

#########

COMPONENTS_DIR="$(dirname "$PWD")"
echo $COMPONENTS_DIR

# Flash Partition Table, and test data
python /${IDF_PATH}/components/esptool_py/esptool/esptool.py \
    --chip esp32 \
    --port ${ESP_PORT} \
    --baud ${ESP_BAUDRATE} \
    --before default_reset \
    --after hard_reset write_flash \
    -z --flash_mode dio \
    --flash_freq 40m \
    --flash_size detect \
    0x1000 ${PWD}/bin/bootloader.bin \
    0x8000 ${PWD}/bin/partition_table_unit_test_two_ota.bin\
    0xd000 ${PWD}/bin/ota_data_initial.bin \
    0x0c0000 ${PWD}/bin/hello_world.bin \
    0x240000 ${PWD}/bin/hello_world_after_patch.bin

# Flash Unit Test App and monitor

make \
    EXTRA_COMPONENT_DIRS=${COMPONENTS} \
    -C ${IDF_PATH}/tools/unit-test-app \
    component-esp_hdiffz-clean

make \
    -C ${IDF_PATH}/tools/unit-test-app \
    EXTRA_COMPONENT_DIRS=${COMPONENTS} \
    TEST_COMPONENTS=esp_hdiffz

make \
    -C ${IDF_PATH}/tools/unit-test-app \
    EXTRA_COMPONENT_DIRS=${COMPONENTS} \
    TEST_COMPONENTS=esp_hdiffz \
    app-flash monitor

