cmake_minimum_required(VERSION 3.24)
project(BedrockInsight LANGUAGES CXX)

# The supported server build uses xmake because LeviLamina publishes its
# package and mod-packer rules through xmake. This CMake entry point keeps the
# adapter-neutral core easy to configure in IDEs and is intentionally optional
# until a SQLite provider is selected by the caller.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BEDROCKINSIGHT_BUILD_CORE "Build the adapter-neutral core" ON)
if(BEDROCKINSIGHT_BUILD_CORE)
    add_library(bedrock_insight_core STATIC
        src/bedrock_insight/core/api_payload.cpp
        src/bedrock_insight/core/api_json.cpp
        src/bedrock_insight/core/admin_api.cpp
        src/bedrock_insight/core/analytics_query.cpp
        src/bedrock_insight/core/dashboard.cpp
        src/bedrock_insight/core/event.cpp
        src/bedrock_insight/core/http_server.cpp
        src/bedrock_insight/core/http_transport.cpp
        src/bedrock_insight/core/json.cpp
        src/bedrock_insight/core/type_labels.cpp
        src/bedrock_insight/core/metrics.cpp
        src/bedrock_insight/core/rollback.cpp
        src/bedrock_insight/core/sqlite_api.cpp
        src/bedrock_insight/core/storage.cpp
    )
    target_include_directories(bedrock_insight_core PUBLIC src)
    find_path(BEDROCKINSIGHT_SQLITE_INCLUDE_DIR sqlite3.h
        HINTS "$ENV{BEDROCKINSIGHT_SQLITE_ROOT}/include")
    find_library(BEDROCKINSIGHT_SQLITE_LIBRARY NAMES sqlite3
        HINTS "$ENV{BEDROCKINSIGHT_SQLITE_ROOT}/lib")
    if(BEDROCKINSIGHT_SQLITE_INCLUDE_DIR AND BEDROCKINSIGHT_SQLITE_LIBRARY)
        target_include_directories(bedrock_insight_core PUBLIC "${BEDROCKINSIGHT_SQLITE_INCLUDE_DIR}")
        target_link_libraries(bedrock_insight_core PUBLIC "${BEDROCKINSIGHT_SQLITE_LIBRARY}")
    else()
        message(STATUS "SQLite3 not found; set BEDROCKINSIGHT_SQLITE_ROOT for a core build")
    endif()

    enable_testing()
    add_executable(BedrockInsightCoreTests tests/core_tests.cpp)
    target_include_directories(BedrockInsightCoreTests PRIVATE src)
    target_link_libraries(BedrockInsightCoreTests PRIVATE bedrock_insight_core)
    if(WIN32)
        target_link_libraries(bedrock_insight_core PUBLIC ws2_32)
    endif()
    add_test(NAME BedrockInsightCoreTests COMMAND BedrockInsightCoreTests)
endif()
