PROJECT_NAME := hdiffz

EXTRA_COMPONENT_DIRS := \
	$(abspath ../esp_full_miniz)

include $(IDF_PATH)/make/project.mk
