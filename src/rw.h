#ifndef ESP_HDIFFZ_RW_H__ 
#define ESP_HDIFFZ_RW_H__ 

#include "esp_system.h"
#include "esp_partition.h"

#include "HPatch/patch.h"

hpatch_BOOL esp_hdiffz_file_read(const struct hpatch_TStreamInput* stream,
        hpatch_StreamPos_t readFromPos,
        unsigned char* out_data,
        unsigned char* out_data_end);

hpatch_BOOL esp_hdiffz_file_write(const struct hpatch_TStreamOutput* stream,
        hpatch_StreamPos_t writeToPos,
        const unsigned char* data,
        const unsigned char* data_end);

size_t esp_hdiffz_get_file_size(FILE *f);


#endif
