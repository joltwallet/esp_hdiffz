#include "esp_log.h"
#include "esp_hdiffz.h"

#include "esp_system.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "miniz_plugin.h"


static const char TAG[] = "esp_hdiffz_file";

/**************
 * PROTOTYPES *
 **************/
static hpatch_BOOL file_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end);
static hpatch_BOOL file_write(const struct hpatch_TStreamOutput* stream,
        hpatch_StreamPos_t writeToPos,
        const unsigned char* data,
        const unsigned char* data_end);

/********************
 * PUBLIC FUNCTIONS *
 ********************/

esp_err_t esp_hdiffz_patch_file(FILE *in, FILE *out, const char *diff, size_t diff_size) {
    esp_err_t err = ESP_FAIL;

    hpatch_TStreamOutput out_stream = { 0 };
    hpatch_TStreamInput  old_stream = { 0 };
    hpatch_TStreamInput  diff_stream;

    old_stream.streamImport = in;
    old_stream.streamSize = 0; // TODO: get actual size of input file?
    old_stream.read = file_read;

    out_stream.streamImport = out;
    out_stream.streamSize = 0; // TODO: get remaining disc size?
    out_stream.read_writed = file_read;
    out_stream.write = file_write;

    mem_as_hStreamInput(&diff_stream, (const unsigned char *)diff, (const unsigned char *)&diff[diff_size]);

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

/**
 * @brief Read data from file.
 * @return True on success, False otherwise
 */
static hpatch_BOOL file_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end) {
    esp_err_t err;
    int n_bytes = out_data_end - out_data;

    FILE *file = (FILE*)stream->streamImport;

    if (n_bytes != fread(out_data, 1, n_bytes, file)) return hpatch_FALSE;
    return hpatch_TRUE;
}

/** 
 * @brief Write data to partition.
 * @param stream[in] stream
 * @param writeToPos[in] Offset to write to.
 * @param data[in] Pointer to beginning of data array to write
 * @param data_end[in] Pointer to end of data array to write
 */
static hpatch_BOOL file_write(const struct hpatch_TStreamOutput* stream,
        hpatch_StreamPos_t writeToPos,
        const unsigned char* data,
        const unsigned char* data_end) {
    esp_err_t err;
    int n_bytes = data_end - data;

    FILE *file = (FILE*)stream->streamImport;

    if (n_bytes!=fwrite(data, 1, n_bytes, file)) return hpatch_FALSE;

    return hpatch_TRUE;
}

