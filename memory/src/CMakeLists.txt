set(HEADERFILES
    platform.h
    defines.h
    snmemory.h
)

set(SRCS
    snmemory.c
)

set(INCLUDE_BASE "${PROJECT_SOURCE_DIR}/memory/include/snmemory")

list(TRANSFORM HEADERFILES PREPEND "${INCLUDE_BASE}/")

target_sources(snmemory PRIVATE ${HEADERFILES} ${SRCS})
