#include "common.h"
#include "esp_spiffs.h"
#include "unity.h"

static const char* spiffs_test_partition_label = "flash_test";

void test_fs_setup(void)
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = spiffs_test_partition_label,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    TEST_ESP_OK(esp_vfs_spiffs_register(&conf));
}

void test_fs_teardown(void)
{
    TEST_ESP_OK(esp_vfs_spiffs_unregister(spiffs_test_partition_label));
}

void test_spiffs_create_file_with_data(const char *name, const char* data, size_t len) {
    FILE* f = fopen(name, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL(len, fwrite(data, 1, len, f));
    TEST_ASSERT_EQUAL(0, fclose(f));
}

void test_spiffs_create_file_with_text(const char* name, const char* text)
{
    FILE* f = fopen(name, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_TRUE(fputs(text, f) != EOF);
    TEST_ASSERT_EQUAL(0, fclose(f));
}


