if(SN_MEMORY_BUILD_SHARED)
    add_library(snmemory SHARED)
    target_compile_definitions(snmemory PRIVATE SN_EXPORT)
else()
    add_library(snmemory STATIC)
    target_compile_definitions(snmemory PRIVATE SN_MEMORY_STATIC)
endif()

target_include_directories(snmemory PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(snmemory PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(snmemory PRIVATE sn_memory_configs)

add_subdirectory(src)
