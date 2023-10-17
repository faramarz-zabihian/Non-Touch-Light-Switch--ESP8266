 # The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
 project(Opprah)
cmake_minimum_required(VERSION 3.5)

#set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello-world)
#idf_build_component(cbor)

set (cbor_make_source_path D:/AI/Aiva/libs/cbor)
idf_build_set_property(CBOR_IDF_SOURCE_PATH ${cbor_make_source_path})
option(CN_CBOR_ALIGN_READS "Use memcpy in ntoh*p()" ON)
if(CN_CBOR_ALIGN_READS)
    idf_build_set_property(COMPILE_DEFINITIONS "-DCBOR_ALIGN_READS" APPEND)
endif()


#idf_build_get_property(cbor_make_source_path CBOR_IDF_SOURCE_PATH)
#add_subdirectory(cbor)
#[[add_subdirectory(cn-cbor)
target_link_libraries(${CMAKE_PROJECT_NAME}.elf cn-cbor)]]