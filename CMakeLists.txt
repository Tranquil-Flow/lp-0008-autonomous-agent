cmake_minimum_required(VERSION 3.14)
project(AgentModulePlugin LANGUAGES CXX)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake")
    include(cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found")
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json" METADATA_JSON)
string(JSON MODULE_NAME GET ${METADATA_JSON} name)

logos_module(
    NAME ${MODULE_NAME}
    SOURCES
        src/agent_module_impl.h
        src/agent_module_impl.cpp
        src/skill_registry.h
        src/skill_registry.cpp
        src/agent_config.h
        src/agent_config.cpp
        src/persistence.h
        src/persistence.cpp
        src/spending_gate.h
        src/spending_gate.cpp
        src/wallet_bridge.h
        src/wallet_bridge.cpp
    # Real LEZ wallet: libwallet_ffi.dylib + wallet_ffi.h are staged into lib/
    # (prebuilt from logos-execution-zone wallet-ffi). The helper links it,
    # adds lib/ to the include path, and copies the dylib next to the plugin.
    EXTERNAL_LIBS
        wallet_ffi
)
