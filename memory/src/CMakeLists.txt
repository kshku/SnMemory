set(HEADERFILES
    platform.h
    defines.h
    linear.h
    stack.h
    pool.h
    freelist.h
)

set(SRCS
    snmemory.c
    freelist.c
)

set(INCLUDE_BASE "${PROJECT_SOURCE_DIR}/memory/include/snmemory")

list(TRANSFORM HEADERFILES PREPEND "${INCLUDE_BASE}/")

target_sources(snmemory PRIVATE ${HEADERFILES} ${SRCS})
