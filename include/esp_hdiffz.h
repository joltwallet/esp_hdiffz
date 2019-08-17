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


/**
 * @param Create the patched file.
 *
 * Not performed inplace incase of failure during transmission.
 *
 * @param[in] in Opened file containing old data.
 * @param[out] out Opened file to write patched data to.
 * @param[in] diff stream
 * @param[in] diff_size number of bytes in current diff chunk.
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_patch_file(FILE *in, FILE *out, const char *diff, size_t diff_size);

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
esp_err_t esp_hdiffz_patch_ota(const char *diff, size_t diff_size);

/**
 * @brief Close and finalize the OTA
 * @return ESP_OK on success.
 */
esp_err_t esp_hdiffz_patch_ota_close();

#ifdef __cplusplus
} // extern "C"
#endif

#endif
