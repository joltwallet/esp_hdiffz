idf_component_register(
        SRCS
            "src/rw.c"
            "src/file.c"
            "src/miniz_plugin.c"
            "src/ota.c"
            "HDiffPatch/libHDiffPatch/HPatch/patch.c"
        INCLUDE_DIRS
            "include"
            "src"
            "HDiffPatch/libHDiffPatch/"
        REQUIRES
            "esp_full_miniz"
        PRIV_REQUIRES
            "app_update"
            "spi_flash"
)

