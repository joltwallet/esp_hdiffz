#define LOG_LOCAL_LEVEL 4

#include "esp_log.h"
#include "esp_hdiffz.h"

#include "esp_system.h"
#include "miniz_plugin.h"
#include "rw.h"


static const char TAG[] = "esp_hdiffz_file";

/**************
 * PROTOTYPES *
 **************/

/********************
 * PUBLIC FUNCTIONS *
 ********************/

esp_err_t esp_hdiffz_patch_file_from_mem(FILE *in, FILE *out, const char *diff, size_t diff_size) {
    esp_err_t err = ESP_FAIL;

    hpatch_TStreamOutput out_stream = { 0 };
    hpatch_TStreamInput  old_stream = { 0 };
    hpatch_TStreamInput  diff_stream;

    old_stream.streamImport = in;
    old_stream.streamSize = esp_hdiffz_get_file_size(in);
    old_stream.read = esp_hdiffz_file_read;

    out_stream.streamImport = out;
    out_stream.streamSize = UINT32_MAX;
    out_stream.write = esp_hdiffz_file_write;

    mem_as_hStreamInput(&diff_stream, (const unsigned char *)diff, (const unsigned char *)&diff[diff_size]);

    if(!patch_decompress(&out_stream, &old_stream, &diff_stream, minizDecompressPlugin)){
        ESP_LOGE(TAG, "Failed to run patch_decompress");
        goto exit;
    }

    return ESP_OK;

exit:
    return err;
}

esp_err_t esp_hdiffz_patch_file(FILE *in, FILE *out, FILE *diff){
    esp_err_t err = ESP_FAIL;

    hpatch_TStreamOutput out_stream = { 0 };
    hpatch_TStreamInput  old_stream = { 0 };
    hpatch_TStreamInput  diff_stream;

    old_stream.streamImport = in;
    old_stream.streamSize = esp_hdiffz_get_file_size(in);
    old_stream.read = esp_hdiffz_file_read;

    out_stream.streamImport = out;
    out_stream.streamSize = UINT32_MAX;
    out_stream.write = esp_hdiffz_file_write;

    diff_stream.streamImport = diff;
    diff_stream.streamSize = esp_hdiffz_get_file_size(diff);
    diff_stream.read = esp_hdiffz_file_read;

    if(!patch_decompress(&out_stream, &old_stream, &diff_stream, minizDecompressPlugin)){
        ESP_LOGE(TAG, "Failed to run patch_decompress");
        goto exit;
    }

    return ESP_OK;

exit:
    return err;
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/


