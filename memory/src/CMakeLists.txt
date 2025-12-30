set(HEADERFILES
    platform.h
    defines.h
    linear.h
    stack.h
    pool.h
    freelist.h
    vm.h
)

set(SRCS
    freelist.c
)

set(SPECIFIC_SRCS
    vm.c
)

set(SUBDIRS
    win32
    nix
)

set(INCLUDE_BASE "${PROJECT_SOURCE_DIR}/memory/include/snmemory")

list(TRANSFORM HEADERFILES PREPEND "${INCLUDE_BASE}/")

target_sources(snmemory PRIVATE ${HEADERFILES} ${SRCS})

foreach(dir IN LISTS SUBDIRS)
    foreach(src IN LISTS SPECIFIC_SRCS)
        target_sources(snmemory PRIVATE "${dir}/${src}")
    endforeach()
endforeach()

