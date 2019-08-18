
#define LOG_LOCAL_LEVEL 4

#include "esp_log.h"
#include "esp_hdiffz.h"

#include "esp_system.h"
#include "esp_flash_partitions.h"
#include "miniz_plugin.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

#define CONFIG_HDIFFZ_OTA_RINGBUF_SIZE 2048
#define CONFIG_HDIFFZ_OTA_RINGBUF_TIMEOUT pdMS_TO_TICKS(1000)
#define CONFIG_HDIFFZ_OTA_TASK_SIZE 20000
#define CONFIG_HDIFFZ_OTA_TASK_PRIORITY 5
#define CONFIG_HDIFFZ_OTA_TASK_NAME "hdiffz_ota"

static const char TAG[] = "esp_hdiffz_ota";

typedef struct esp_hdiffz_ota_handle_t{
    struct {
        const esp_partition_t *src;
        const esp_partition_t *dst;
    } part;
    struct {
        esp_ota_handle_t ota;
        TaskHandle_t task;
        RingbufHandle_t ringbuf;
    } handle;
    SemaphoreHandle_t complete;
    size_t diff_size;
}esp_hdiffz_ota_handle_t;

/**************
 * PROTOTYPES *
 **************/
static hpatch_BOOL partition_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end);
static hpatch_BOOL partition_write(const struct hpatch_TStreamOutput* stream,
        hpatch_StreamPos_t writeToPos,
        const unsigned char* data,
        const unsigned char* data_end);

static hpatch_BOOL ringbuf_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end);

static void esp_hdiffz_ota_task( void *params );

static void esp_hdiffz_ota_handle_del(esp_hdiffz_ota_handle_t *h);

/*********************
 * PUBLIC FUNCTIONS  *
 *********************/
/**
 * @brief Apply 
 */
//see patch_decompress_mem

esp_err_t esp_hdiffz_ota_begin(size_t diff_size, esp_hdiffz_ota_handle_t **out_handle) {

    /******************
     * Get Partitions *
     ******************/
    const esp_partition_t *src;
    const esp_partition_t *dst;
    {
        const esp_partition_t *configured = esp_ota_get_boot_partition();
        src = esp_ota_get_running_partition();
        if (configured != src) {
            ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, "
                    "but running from offset 0x%08x",
                     configured->address, src->address);
            ESP_LOGW(TAG, "(This can happen if either the OTA boot data or "
                    "preferred boot image become corrupted somehow.)");
        }
        ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
                src->type, src->subtype, src->address);

        dst = esp_ota_get_next_update_partition(NULL);
        assert(dst != NULL);
    }

    return esp_hdiffz_ota_begin_adv(src, dst, OTA_SIZE_UNKNOWN, diff_size, out_handle);
}

esp_err_t esp_hdiffz_ota_begin_adv(const esp_partition_t *src, const esp_partition_t *dst,
        size_t image_size, size_t diff_size, esp_hdiffz_ota_handle_t **out_handle) {
    esp_err_t err = ESP_FAIL;
    esp_hdiffz_ota_handle_t *h;

    *out_handle = calloc(1, sizeof(esp_hdiffz_ota_handle_t));
    h = *out_handle;
    if( NULL == h ) {
        err = ESP_ERR_NO_MEM;
        goto exit;
    }
    h->part.src = src;
    h->part.dst = dst;
    h->diff_size = diff_size;

    h->complete = xSemaphoreCreateBinary();
    if(NULL == h->complete){
        err = ESP_ERR_NO_MEM;
        goto exit;
    }

    err = esp_ota_begin(h->part.dst, image_size, &h->handle.ota);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        goto exit;
    }

    /* Create ringbuf that diff data will be streamed to */
    h->handle.ringbuf = xRingbufferCreate(
            CONFIG_HDIFFZ_OTA_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if( NULL == h->handle.ringbuf) {
        ESP_LOGE(TAG, "Failed to allocate ringbuf.");
        err = ESP_ERR_NO_MEM;
        goto exit;
    }

    /* Create hdiffz task */
    {
        BaseType_t res;
        res = xTaskCreate(esp_hdiffz_ota_task,
                CONFIG_HDIFFZ_OTA_TASK_NAME,
                CONFIG_HDIFFZ_OTA_TASK_SIZE, h,
                CONFIG_HDIFFZ_OTA_TASK_PRIORITY,
                &h->handle.task);
        if(pdPASS != res) {
            ESP_LOGE(TAG, "Failed to create hdiffz task.");
            goto exit;
        }
    }

    return ESP_OK;

exit:
    if(NULL != h) esp_hdiffz_ota_handle_del(h);
    *out_handle = NULL;
    return err;
}


esp_err_t esp_hdiffz_ota_write(esp_hdiffz_ota_handle_t *handle, const void *data, size_t size) {
    if(pdTRUE != xRingbufferSend(handle->handle.ringbuf, data, size, portMAX_DELAY)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_hdiffz_ota_end(esp_hdiffz_ota_handle_t *handle) {
    esp_err_t err = ESP_FAIL;

    // Wait until the hdiffz task is done.
    xSemaphoreTake(handle->complete, portMAX_DELAY);

    /************************
     * Close the ota_handle *
     ************************/
    if (esp_ota_end(handle->handle.ota) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        goto exit;
    }
    handle->handle.ota = 0;

    /**********************************************
     * Set the update_partition as boot partition *
     **********************************************/
    err = esp_ota_set_boot_partition(handle->part.dst);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        goto exit;
    }

    ESP_LOGI(TAG, "OTA Complete. Please Reboot System");

    err = ESP_OK;

exit:
    esp_hdiffz_ota_handle_del(handle);
    return err;
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/


/**
 * @brief Read data from partition.
 *
 *
 * 
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

/**
 * @brief Read data from ring buffer.
 * @return True on success, False otherwise
 */
static hpatch_BOOL ringbuf_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end) {


    RingbufHandle_t ringbuf = stream->streamImport;
    char *ringbuf_ptr;
    size_t n_bytes = out_data_end - out_data;

    ESP_LOGD(TAG, "Attempting to receive %d bytes from %d.", n_bytes, (int)readFromPos);

    while(n_bytes > 0){
        size_t bytes_received = 0;
        ringbuf_ptr = xRingbufferReceiveUpTo(ringbuf, &bytes_received,
                CONFIG_HDIFFZ_OTA_RINGBUF_TIMEOUT, n_bytes);
        n_bytes -= bytes_received;
        if( 0 == bytes_received ) {
            ESP_LOGD(TAG, "0 bytes received");
            return hpatch_FALSE;
        }
        memcpy(out_data, ringbuf_ptr, bytes_received);
        vRingbufferReturnItem(ringbuf, ringbuf_ptr);
        out_data += bytes_received;
    }

    assert(out_data == out_data_end);
    ESP_LOGD(TAG, "All bytes received");

    return hpatch_TRUE;
}

static void esp_hdiffz_ota_task( void *params ){
    esp_hdiffz_ota_handle_t *h = params;

    hpatch_TStreamOutput out_stream = { 0 };
    hpatch_TStreamInput  old_stream = { 0 };
    hpatch_TStreamInput  diff_stream;

    out_stream.streamImport = (void*)h->part.dst;
    out_stream.streamSize = h->part.dst->size;
    out_stream.write = partition_write;

    old_stream.streamImport = (void*)h->part.src;
    old_stream.streamSize = h->part.src->size;
    old_stream.read = partition_read;

    diff_stream.streamImport = h->handle.ringbuf;
    diff_stream.streamSize = h->diff_size;
    diff_stream.read = ringbuf_read;

    if(!patch_decompress(&out_stream, &old_stream, &diff_stream, minizDecompressPlugin)){
        ESP_LOGE(TAG, "Failed to run patch_decompress");
    }

    xSemaphoreGive(h->complete);

    h->handle.task = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Cleanup and Free esp_hdiffz_ota_handle_t object.
 * @param h Handle to free
 */
static void esp_hdiffz_ota_handle_del(esp_hdiffz_ota_handle_t *h){
    if(h->handle.task) vTaskDelete(h->handle.task);
    if(h->handle.ota) esp_ota_end(h->handle.ota);
    if(h->handle.ringbuf) vRingbufferDelete(h->handle.ringbuf);
    if(h->complete) vSemaphoreDelete(h->complete);
    free(h);
}
