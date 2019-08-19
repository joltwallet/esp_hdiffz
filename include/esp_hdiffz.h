#ifndef ESP_HDIFFZ_H__
#define ESP_HDIFFZ_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include <string.h>
#include "sdkconfig.h"
#include "esp_partition.h"

#include "HPatch/patch.h"
#include "HPatch/patch_types.h"

#include "miniz_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *NOTE: 
 * * All diff streams must be zlib compressed.
 */

/*********
 * FILES *
 *********/

/**
 * @param Create the patched file using a diff from memory.
 *
 * Not performed inplace incase of failure during transmission.
 *
 * @param[in] in Opened file containing old data.
 * @param[out] out Opened file to write patched data to.
 * @param[in] diff Full diff array.
 * @param[in] diff_size number of bytes in diff.
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_patch_file_from_mem(FILE *in, FILE *out, const char *diff, size_t diff_size);

/**
 * @param Create the patched file using a diff file.
 *
 * Not performed inplace incase of failure during transmission.
 *
 * @param[in] in Opened file containing old data.
 * @param[out] out Opened file to write patched data to.
 * @param[in] diff Full diff array.
 * @param[in] diff_size number of bytes in diff.
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_patch_file(FILE *in, FILE *out, FILE *diff);


/**
 * @brief Performs an hdiffpatch firmware upgrade.
 *
 * Assumes that the diff is applied to the currently running firmware.
 * Patched firmware will be flashed to the next free OTA partition.
 *
 * @param[in] diff hdiffpatch file to apply.
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_ota_file(FILE *diff);

/**
 * @brief esp_hdiffz_ota_file, but with more explicit parameters.
 * @param[in] diff hdiffpatch file to apply.
 * @param[in] src partition to be apply patch from
 * @param[in] dst partition to save the patched firmware
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_ota_file_adv(FILE *diff, const esp_partition_t *src, const esp_partition_t *dst);

/*******
 * OTA *
 *******/

/* NOTE: OTA Streaming is currently not enabled because HDiffPatch needs 
 * a serialization/deserialization that doesn't have random access.
 * Technically this is possible, but it's a sizeable amount of work for now. */
#if 0

typedef struct esp_hdiffz_ota_handle_t esp_hdiffz_ota_handle_t;

/**
 * @brief esp_hdiffz_ota_begin, but with more explicit parameters.
 * @param[in] src partition to be apply patch from
 * @param[in] dst partition to save the patched firmware
 * @param[in] image_size Resulting patched firmware size; set to OTA_SIZE_UNKNOWN if not known.
 * @param[in] diff_size Size of the complete diff stream.
 * @param[out] out_handle
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_ota_begin_adv(const esp_partition_t *src, const esp_partition_t *dst, size_t image_size, size_t diff_size, esp_hdiffz_ota_handle_t **out_handle);

/**
 * @brief Begins an hdiffpatch firmware upgrade.
 *
 * Assumes that the diff is applied to the currently running firmware.
 * Patched firmware will be flashed to the next free OTA partition.
 *
 * @param[in] diff_size Size of the complete diff stream.
 * @param[out] out_handle
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_ota_begin(size_t diff_size, esp_hdiffz_ota_handle_t **out_handle);

/**
 * @brief Performs OTA using currently running partition as old data.
 *
 * Call as many times as you want as the patch data comes in.
 *
 * Notes:
 *     * Assumes that the old data is coming from the currently running partition.
 *     * Will apply the update to the next OTA partition.
 *     * The diff patch must be zlib compressed.
 *     * Not thread safe.
 *     * call esp_hdiffz_patch_ota_close() upon completion. Then perform a esp_restart().
 *
 * @param[in] diff stream
 * @param[in] diff_size number of bytes in current diff chunk.
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_ota_write(esp_hdiffz_ota_handle_t *handle, const void *data, size_t size);

/**
 * @brief Close and finalize the OTA
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_ota_end(esp_hdiffz_ota_handle_t *handle);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif
