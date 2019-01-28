# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += /home/christoph/workspace/esp-homekit-demo/components/homekit/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/homekit -lhomekit
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += homekit
COMPONENT_LDFRAGMENTS += 
component-homekit-build: component-wolfssl-build component-cJSON-build component-http-parser-build
