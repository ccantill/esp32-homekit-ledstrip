# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(PROJECT_PATH)/main/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/main -lmain
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += main
COMPONENT_LDFRAGMENTS += 
component-main-build: component-homekit-build component-esp32_digital_led_lib-build
