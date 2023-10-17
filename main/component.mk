#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_SRCDIRS += src coap/src cn-cbor/src
COMPONENT_ADD_INCLUDEDIRS = include coap/include cn-cbor/include  ../../../ESP8266_RTOS_SDK/components/esp8266/include/driver
EXTRA_COMPONENT_DIRS = $(IDF_PATH)/examples/common_components/protocol_examples_common