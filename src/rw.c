#define LOG_LOCAL_LEVEL 4

#include "rw.h"
#include "esp_log.h"

static const char TAG[] = "hdiffz_rw";

/**
 * @brief Read data from file.
 * @return True on success, False otherwise
 */
hpatch_BOOL esp_hdiffz_file_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end) {
    int n_bytes = out_data_end - out_data;
    ESP_LOGD(TAG, "Reading %d bytes from file.", n_bytes);

    FILE *file = (FILE*)stream->streamImport;

    fseek(file, readFromPos, SEEK_SET);
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
hpatch_BOOL esp_hdiffz_file_write(const struct hpatch_TStreamOutput* stream,
        hpatch_StreamPos_t writeToPos,
        const unsigned char* data,
        const unsigned char* data_end) {
    int n_bytes = data_end - data;
    ESP_LOGD(TAG, "Writing %d bytes to file. \"%.*s\"", n_bytes, n_bytes, data);

    FILE *file = (FILE*)stream->streamImport;

    fseek(file, writeToPos, SEEK_SET);
    if (n_bytes!=fwrite(data, 1, n_bytes, file)) return hpatch_FALSE;

    return hpatch_TRUE;
}

size_t esp_hdiffz_get_file_size(FILE *f) {
    size_t size;
    long int pos;
    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, pos);
    return size;
}
