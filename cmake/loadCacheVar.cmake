
include("${CMAKE_CURRENT_LIST_DIR}/libOTeFindBuildDir.cmake")


if(NOT EXISTS "${libOTe_BIN_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "cache file does not exist at ${libOTe_BIN_DIR}")
endif()


LOAD_CACHE("${libOTe_BIN_DIR}/" READ_WITH_PREFIX LOAD_CACHE_LIBOTE_
    ENABLE_BOOST
    ENABLE_SODIUM
    ENABLE_RELIC
    ENABLE_CIRCUITS
    ENABLE_SIMPLESTOT_ASM
    ENABLE_MR_KYBER
    )
set(ENABLE_BOOST          ${LOAD_CACHE_LIBOTE_ENABLE_BOOST})
set(ENABLE_SODIUM         ${LOAD_CACHE_LIBOTE_ENABLE_SODIUM})
set(ENABLE_RELIC          ${LOAD_CACHE_LIBOTE_ENABLE_RELIC})
set(ENABLE_CIRCUITS       ${LOAD_CACHE_LIBOTE_ENABLE_CIRCUITS})
set(ENABLE_SIMPLESTOT_ASM ${LOAD_CACHE_LIBOTE_ENABLE_SIMPLESTOT_ASM})
set(ENABLE_MR_KYBER       ${LOAD_CACHE_LIBOTE_ENABLE_MR_KYBER})

