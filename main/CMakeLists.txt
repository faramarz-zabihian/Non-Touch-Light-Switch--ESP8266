#set(COMPONENTS "protocol_examples_common" "spi_flash" "pthread")
set(MY_SRCS  src/main.cpp
        coap/src/request.cpp
        coap/src/CoapClient.cpp
        coap/src/coap_utils.cpp
        src/connect.c
        src/app_utils.c
        src/app_smart_config.c
        src/app_address.cpp
        cn-cbor/src/cn-cbor.c
        cn-cbor/src/cn-create.c
        cn-cbor/src/cn-encoder.c
        cn-cbor/src/cn-error.c
        cn-cbor/src/cn-get.c
        cn-cbor/src/cn-print.c
        exp-backoff/backoff_algorithm.c
        src/ExpBackoff.cpp
        src/MemoryMonitor.cpp
        src/config.cpp
        )

set(MY_INCS
        .
        "D:/AI/DEV_ESP8266/msys32/home/asus/get-started/hello_world/cbor"
        "${CMAKE_BINARY_DIR}"
        "coap/include"
        "include"
        cn-cbor/include
        ../../../ESP8266_RTOS_SDK/examples/common_components/protocol_examples_common/include

        )
idf_component_register(
        SRCS ${MY_SRCS}
        REQUIRES coap nvs_flash libsodium

        INCLUDE_DIRS ${MY_INCS}
)
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
#option(CN_CBOR_ALIGN_READS "Use memcpy in ntoh*p()" ON)
#if(CN_CBOR_ALIGN_READS)
#    idf_build_set_property(COMPILE_DEFINITIONS "-DCBOR_ALIGN_READS" APPEND)
#endif()
