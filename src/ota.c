
#include "esp_log.h"
#include "esp_hdiffz.h"

#include "esp_system.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "miniz_plugin.h"
#include "esp_ota_ops.h"


static const char TAG[] = "esp_hdiffz_ota";
static esp_ota_handle_t ota_handle = 0; // underlying uint32_t
static const esp_partition_t *running_partition = NULL;
static const esp_partition_t *update_partition = NULL;

/**************
 * PROTOTYPES *
 **************/
static esp_err_t ota_handle_init( );
static hpatch_BOOL partition_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end);
static hpatch_BOOL partition_write(const struct hpatch_TStreamOutput* stream,
        hpatch_StreamPos_t writeToPos,
        const unsigned char* data,
        const unsigned char* data_end);

/*********************
 * PUBLIC FUNCTIONS  *
 *********************/
/**
 * @brief Apply 
 */
//see patch_decompress_mem

esp_err_t esp_hdiffz_patch_ota(const char *diff, size_t diff_size) {
    esp_err_t err = ESP_FAIL;

    /**********************************
     * Get/Init Handle of OTA Process *
     **********************************/

    if( 0 != ota_handle || NULL != update_partition || NULL != running_partition){
        err = ota_handle_init();
        if( ESP_OK != err ){
            ESP_LOGE(TAG, "Failed to get OTA Handle");
            goto exit;
        }
    }
    hpatch_TStreamOutput out_stream = { 0 };
    hpatch_TStreamInput  old_stream = { 0 };
    hpatch_TStreamInput  diff_stream;

    old_stream.streamImport = (void*)running_partition;
    old_stream.streamSize = running_partition->size;
    old_stream.read = partition_read;

    out_stream.streamImport = (void*)update_partition;
    out_stream.streamSize = update_partition->size;
    out_stream.read_writed = partition_read;
    out_stream.write = partition_write;

    mem_as_hStreamInput(&diff_stream, (const unsigned char *)diff, (const unsigned char *)&diff[diff_size]);

    if(!patch_decompress(&out_stream, &old_stream, &diff_stream, minizDecompressPlugin)){
        ESP_LOGE(TAG, "Failed to run patch_decompress");
        goto exit;
    }


    return ESP_OK;

exit:
    return err;
}

esp_err_t esp_hdiffz_patch_ota_close() {
    esp_err_t err = ESP_FAIL;

    /************************
     * Close the ota_handle *
     ************************/
    if (esp_ota_end(ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        ota_handle = 0;
        goto exit;
    }
    else {
        ota_handle = 0;
    }

    /*********************
     * Post-Flight Check *
     *********************/
    if (esp_partition_check_identity(esp_ota_get_running_partition(), update_partition) == true) {
        ESP_LOGI(TAG, "The current running firmware is same as the firmware just downloaded");
    }

    /**********************************************
     * Set the update_partition as boot partition *
     **********************************************/
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        goto exit;
    }

    ESP_LOGI(TAG, "OTA Complete. Please Reboot System");
    return ESP_OK;

exit:
    return err;
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

/**
 * @brief Initiates OTA, populates update_partition handle.
 * @return ESP_OK on success
 */
static esp_err_t ota_handle_init( ) {
    esp_err_t err = ESP_FAIL;

    if( 0 != ota_handle || NULL != update_partition || NULL != running_partition){
        ESP_LOGW(TAG, "OTA already in prgoress");
        goto exit;
    }

    ESP_LOGI(TAG, "Starting OTA Firmware Update...");

    /*******************************
     * Run Some System Diagnostics *
     *******************************/
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    running_partition    = esp_ota_get_running_partition();

    if (configured != running_partition) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, "
                "but running from offset 0x%08x",
                 configured->address, running_partition->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or "
                "preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
            running_partition->type, running_partition->subtype, running_partition->address);

    /**********************************
     * Find Partition to Write Update *
     **********************************/
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
            update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    /**********************************
     * Find Partition to Write Update *
     **********************************/
    /* This action wipes the update_partition */
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        goto exit;
    }
    err = ESP_OK;

exit:
    return err;
}

/**
 * @brief Read data from partition.
 * @return True on success, False otherwise
 */
static hpatch_BOOL partition_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end) {
    esp_err_t err;
    int n_bytes = out_data_end - out_data;

    esp_partition_t *part = (esp_partition_t*)stream->streamImport;

    err = esp_partition_read(part, readFromPos, out_data, n_bytes);

    switch(err){
        case ESP_OK:
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "read offset exceeded partition size (%s)", esp_err_to_name(err));
            return hpatch_FALSE;
        case ESP_ERR_INVALID_SIZE:
            ESP_LOGE(TAG, "Reading %d bytes at offset %d would exceed partition bounds (%s)",
                    (uint32_t)n_bytes, (uint32_t)readFromPos, esp_err_to_name(err));
            return hpatch_FALSE;
        default:
            ESP_LOGE(TAG, "Unknown error reading from partition (%s)", esp_err_to_name(err));
            return hpatch_FALSE;
    }
    return hpatch_TRUE;
}

/** 
 * @brief Write data to partition.
 * @param stream[in] stream
 * @param writeToPos[in] Offset to write to.
 * @param data[in] Pointer to beginning of data array to write
 * @param data_end[in] Pointer to end of data array to write
 */
static hpatch_BOOL partition_write(const struct hpatch_TStreamOutput* stream,
        hpatch_StreamPos_t writeToPos,
        const unsigned char* data,
        const unsigned char* data_end) {
    esp_err_t err;
    int n_bytes = data_end - data;

    esp_partition_t *part = (esp_partition_t*)stream->streamImport;

    err = esp_partition_write(part, writeToPos, data, n_bytes);

    switch(err){
        case ESP_OK:
            break;
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(TAG, "read offset exceeded partition size (%s)", esp_err_to_name(err));
            return hpatch_FALSE;
        case ESP_ERR_INVALID_SIZE:
            ESP_LOGE(TAG, "Writing %d bytes at offset %d would exceed partition bounds (%s)",
                    (uint32_t)n_bytes, (uint32_t)writeToPos, esp_err_to_name(err));
            return hpatch_FALSE;
        default:
            ESP_LOGE(TAG, "Unknown error reading from partition (%s)", esp_err_to_name(err));
            return hpatch_FALSE;
    }
    return hpatch_TRUE;

}

