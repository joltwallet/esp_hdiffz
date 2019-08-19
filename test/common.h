#ifndef ESP_HDIFFZ_TEST_COMMON_H__
#define ESP_HDIFFZ_TEST_COMMON_H__

#include "stddef.h"

void test_fs_setup(void);
void test_fs_teardown(void);
void test_spiffs_create_file_with_data(const char *name, const char* data, size_t len);
void test_spiffs_create_file_with_text(const char* name, const char* text);

#endif
