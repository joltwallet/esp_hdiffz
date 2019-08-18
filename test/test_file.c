#include "esp_hdiffz.h"

#include "unity.h"

#include "esp_spiffs.h"

static const char* spiffs_test_partition_label = "flash_test";

static void test_fs_setup(void)
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = spiffs_test_partition_label,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    TEST_ESP_OK(esp_vfs_spiffs_register(&conf));
}

static void test_fs_teardown(void)
{
    TEST_ESP_OK(esp_vfs_spiffs_unregister(spiffs_test_partition_label));
}

static void test_spiffs_create_file_with_data(const char *name, const char* data, size_t len) {
    FILE* f = fopen(name, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL(len, fwrite(data, 1, len, f));
    TEST_ASSERT_EQUAL(0, fclose(f));
}

static void test_spiffs_create_file_with_text(const char* name, const char* text)
{
    FILE* f = fopen(name, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_TRUE(fputs(text, f) != EOF);
    TEST_ASSERT_EQUAL(0, fclose(f));
}

/**
 * Simplest file test case with patch all in one go.
 *
 * Comment about this test:
 *     The diff file was generated on a unix system.
 *     The input "old_txt" file had contents "foo"; however, unix text files 
 *     terminate in a "\n", so the contents were really "foo\n". Same goes 
 *     for "foobar\n"
 */
TEST_CASE("Small file apply patch from mem", "[hdiffz]")
{
    int cb;
    char buf[100];
    FILE *f_old, *f_new;

    const char soln[] = "foobar\n";
    const char fn_old[] = "/spiffs/old.txt";
    const char fn_new[] = "/spiffs/new.txt";
    const char old_txt[] = "foo\n";
    const char diff[] = {
      0x48, 0x44, 0x49, 0x46, 0x46, 0x31, 0x33, 0x26, 0x7a, 0x6c, 0x69, 0x62,
      0x00, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00,
      0x06, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x0a
    };

    test_fs_setup();

    test_spiffs_create_file_with_text(fn_old, old_txt);

    f_old = fopen(fn_old, "rb");
    f_new = fopen(fn_new, "wb");
    TEST_ESP_OK(esp_hdiffz_patch_file_from_mem(f_old, f_new, diff, sizeof(diff)));
    fclose(f_old);
    fclose(f_new);

    f_new = fopen(fn_new, "rb");
    cb = fread(buf, 1, sizeof(buf), f_new);
    fclose(f_new);

    TEST_ASSERT_EQUAL(strlen(soln), cb); // NULL-terminator is not included in cb.
    TEST_ASSERT_EQUAL_STRING(soln, buf);
    //TEST_ASSERT_EQUAL(0, strcmp(spiffs_test_hello_str, buf));

    test_fs_teardown();
}

TEST_CASE("Small file apply patch from file", "[hdiffz]")
{
    int cb;
    char buf[100];
    FILE *f_old, *f_new, *f_diff;

    const char soln[] = "foobar\n";
    const char fn_old[] = "/spiffs/old.txt";
    const char fn_new[] = "/spiffs/new.txt";
    const char fn_diff[] = "/spiffs/diff.txt";
    const char old_txt[] = "foo\n";
    const char diff[] = {
      0x48, 0x44, 0x49, 0x46, 0x46, 0x31, 0x33, 0x26, 0x7a, 0x6c, 0x69, 0x62,
      0x00, 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00,
      0x06, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x0a
    };

    test_fs_setup();

    test_spiffs_create_file_with_text(fn_old, old_txt);
    test_spiffs_create_file_with_data(fn_diff, diff, sizeof(diff));

    f_old = fopen(fn_old, "rb");
    f_new = fopen(fn_new, "wb");
    f_diff = fopen(fn_diff, "rb");

    TEST_ESP_OK(esp_hdiffz_patch_file(f_old, f_new, f_diff));

    fclose(f_old);
    fclose(f_new);
    fclose(f_diff);

    f_new = fopen(fn_new, "rb");
    cb = fread(buf, 1, sizeof(buf), f_new);
    fclose(f_new);

    TEST_ASSERT_EQUAL(strlen(soln), cb); // NULL-terminator is not included in cb.
    TEST_ASSERT_EQUAL_STRING(soln, buf);

    test_fs_teardown();
}


