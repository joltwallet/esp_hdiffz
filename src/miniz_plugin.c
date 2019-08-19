/**
 * @file miniz_plugin
 * @brief HDiffPatch Plugin to decompress a diff using esp32's miniz ROM.
 *
 * Based off of the zlib plugin in HDiffPatch/decompress_plugin_demo.h
 */

#define LOG_LOCAL_LEVEL 4

#include "miniz_plugin.h" 
#include "miniz.h"
#include "esp_err.h"
#include "esp_log.h"

/**
 *
 */
typedef struct _zlib_TDecompress{
    hpatch_StreamPos_t code_begin;                 /**< */
    hpatch_StreamPos_t code_end;                   /**< */
    const struct hpatch_TStreamInput* codeStream;  /**< */
    
    unsigned char*  dec_buf;                       /**< */
    size_t          dec_buf_size;                  /**< */
    mz_stream       d_stream;                      /**< */
    signed char     window_bits;                   /**< */
} _zlib_TDecompress;

static const char TAG[] = "hdiffz_miniz_plugin";

/*********************
 * HELPER PROTOTYPES *
 *********************/

static hpatch_BOOL _zlib_reset_for_next_node(_zlib_TDecompress* self);

/****************
 * PLUGIN HOOKS *
 ****************/

#define _close_check(value) { if (!(value)) { ESP_LOGE(TAG,"check "#value " ERROR!\n"); result=hpatch_FALSE; } }

/**
 * @brief Hook that verifies the diff header compressionType matches.
 * @param compressType NULL-terminated string indicated compression type.
 * @return True if compressionType matches plugin.
 */
static hpatch_BOOL miniz_is_can_open(const char* compressType) {
    if ((0==strcmp(compressType,"zlib"))||(0==strcmp(compressType,"pzlib"))) {
        ESP_LOGD(TAG, "miniz_is_can_open: TRUE");
        return true;
    }
    else{
        ESP_LOGD(TAG, "miniz_is_can_open: FALSE");
        return false;
    }
}

/**
 * @brief Allocate and initiate plugin.
 * @param decompressPlugin Not Used.
 * @param dataSize Not Used.
 * @param[in] codeStream Data producer
 * @param[in] code_begin Pointer to start of data.
 * @param[in] code_end Pointer to end of data.
 * @return. Returns pointer to decompress object. Returns NULL on error.
 */
static hpatch_decompressHandle miniz_decompress_open( struct hpatch_TDecompress* decompressPlugin,
            hpatch_StreamPos_t dataSize,
            const struct hpatch_TStreamInput* codeStream,
            hpatch_StreamPos_t code_begin,
            hpatch_StreamPos_t code_end) {

    ESP_LOGD(TAG, "miniz_decompress_open");

    _zlib_TDecompress* self = NULL;
    signed char window_bits;
    int decompress_buf_size;
    unsigned char *_mem_buf = NULL;
    size_t _mem_buf_size = 0;

    if (code_end-code_begin<1) {
        ESP_LOGE(TAG, "Provided data has length less than 1.");
        goto exit;
    }

    /* Get the number of windowBits */
    if (!codeStream->read(codeStream,code_begin,(unsigned char*)&window_bits,
                          (unsigned char*)&window_bits+1)) return 0;
    decompress_buf_size = 1 << window_bits;
    ++code_begin;
    assert(window_bits >= 8);
    ESP_LOGD(TAG, "WindowBits %d detected.", window_bits);

    /* Allocate space for the decompress object and the decompress buffer */
    _mem_buf_size = sizeof(_zlib_TDecompress) + decompress_buf_size;
    _mem_buf = malloc(_mem_buf_size);
    if (!_mem_buf) {
        ESP_LOGE(TAG, "OOM");
        goto exit;
    }

    self = (_zlib_TDecompress *)_mem_buf;
    memset(self, 0, sizeof(_zlib_TDecompress));
    self->dec_buf      = (unsigned char*)self+sizeof(_zlib_TDecompress);
    self->dec_buf_size = (_mem_buf+_mem_buf_size)-((unsigned char*)self+sizeof(_zlib_TDecompress));
    self->codeStream   = codeStream;
    self->code_begin   = code_begin;
    self->code_end     = code_end;
    self->window_bits  = window_bits;
    
    /* Init the inflater */
    int res = inflateInit2(&self->d_stream, self->window_bits);
    if(res != MZ_OK){
        ESP_LOGE(TAG, "Failed in init inflate object (Error %d).", res);
        goto exit;
    }

    return self;

exit:
    if( NULL!=_mem_buf ) free(_mem_buf);
    return NULL;
}


/**
 * @brief Deinit and de-allocate the plugin.
 * @param[in,out] decompressPlugin Not Used.
 * @param[in,out] decompressHandle Decompress Object.
 * @return True on success; False otherwise.
 */
static hpatch_BOOL miniz_decompress_close(struct hpatch_TDecompress* decompressPlugin,
        hpatch_decompressHandle decompressHandle) {

    ESP_LOGD(TAG, "miniz_decompress_close");
    _zlib_TDecompress* self;
    hpatch_BOOL result = hpatch_TRUE;

    self = (_zlib_TDecompress*)decompressHandle;
    if ( !self ) return result;

    if ( 0 != self->dec_buf ) _close_check(MZ_OK == inflateEnd(&self->d_stream));

    memset(self,0,sizeof(_zlib_TDecompress));

    if (self) free(self);
    return result;
}

/**
 * @brief Decompress some data.
 * @param[in,out] decompressHandle Decompress Object.
 * @param[out] out_part_data Pointer to buffer to fill.
 * @param[in] out_part_data_end End of output buffer.
 * @return True if (out_part_data_end-out_part_data) bytes are populated; False otherwise.
 */
static hpatch_BOOL miniz_decompress_part(hpatch_decompressHandle decompressHandle,
        unsigned char* out_part_data, 
        unsigned char* out_part_data_end) {

    ESP_LOGD(TAG, "miniz_decompress_part");
    _zlib_TDecompress* self;
    self = (_zlib_TDecompress*)decompressHandle;

    assert( out_part_data != out_part_data_end );
    
    self->d_stream.next_out = out_part_data;
    self->d_stream.avail_out = (uInt)(out_part_data_end-out_part_data);

    while (self->d_stream.avail_out>0) {
        ESP_LOGD(TAG, "Running avail_out %d", self->d_stream.avail_out);
        uInt avail_out_back,avail_in_back;
        int ret;
        hpatch_StreamPos_t codeLen=(self->code_end - self->code_begin);
        if ((self->d_stream.avail_in==0)&&(codeLen>0)) {
            size_t readLen=self->dec_buf_size;
            if (readLen>codeLen) readLen=(size_t)codeLen;
            self->d_stream.next_in=self->dec_buf;
            if (!self->codeStream->read(self->codeStream,self->code_begin,self->dec_buf,
                                        self->dec_buf+readLen)) return hpatch_FALSE;//error;
            self->d_stream.avail_in=(uInt)readLen;
            self->code_begin+=readLen;
            codeLen-=readLen;
        }
        
        avail_out_back=self->d_stream.avail_out;
        avail_in_back=self->d_stream.avail_in;
        ret=inflate(&self->d_stream,Z_PARTIAL_FLUSH);
        if (ret==MZ_OK){
            if ((self->d_stream.avail_in==avail_in_back)&&(self->d_stream.avail_out==avail_out_back)) {
                ESP_LOGE(TAG, "No available in/out data");
                return hpatch_FALSE;
            }
        }else if (ret==Z_STREAM_END){
            if (self->d_stream.avail_in+codeLen>0){ //next compress node!
                if (!_zlib_reset_for_next_node(self)){
                    ESP_LOGE(TAG, "Node complete");
                    return hpatch_FALSE;
                }
            }else{//all end
                if (self->d_stream.avail_out!=0){
                    ESP_LOGE(TAG, "Stream complete");
                    return hpatch_FALSE;
                }
            }
        }else{
            ESP_LOGE(TAG, "Unknown Error");
            return hpatch_FALSE;
        }
    }
    return hpatch_TRUE;
}

static hpatch_TDecompress _minizDecompressPlugin = {
    .is_can_open = miniz_is_can_open,
    .open = miniz_decompress_open,
    .close = miniz_decompress_close,
    .decompress_part = miniz_decompress_part,
};
hpatch_TDecompress *minizDecompressPlugin = &_minizDecompressPlugin;


/***********
 * HELPERS *
 ***********/

/**
 * @brief
 * @param self
 * @return True on success; False otherwise.
 */
static hpatch_BOOL _zlib_reset_for_next_node(_zlib_TDecompress* self){
    //backup
    Bytef*   next_out_back      = self->d_stream.next_out;
    Bytef*   next_in_back       = (Bytef*)self->d_stream.next_in;
    unsigned int avail_out_back = self->d_stream.avail_out;
    unsigned int avail_in_back  = self->d_stream.avail_in;
    //reset
    if (Z_OK!=inflateEnd(&self->d_stream)) return hpatch_FALSE;
    if (Z_OK!=inflateInit2(&self->d_stream,self->window_bits)) return hpatch_FALSE;
    //if (MZ_OK != inflateReset(&self->d_stream)) return hpatch_FALSE;
    //restore
    self->d_stream.next_out  = next_out_back;
    self->d_stream.next_in   = next_in_back;
    self->d_stream.avail_out = avail_out_back;
    self->d_stream.avail_in  = avail_in_back;
    return hpatch_TRUE;
}

