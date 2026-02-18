file(GLOB_RECURSE ALL_HEADERS "${CESIUM_SRC_DIR}/Cesium*/include/**/*.h")
list(APPEND ALL_HEADERS
    "${CESIUM_SRC_DIR}/CesiumJsonReader/include/CesiumJsonReader/IJsonHandler.h"
)

set(PATCHED_COUNT 0)
foreach(HEADER ${ALL_HEADERS})
    file(READ "${HEADER}" CONTENT)
    string(FIND "${CONTENT}" "std::string" HAS_STD_STRING)
    if(NOT HAS_STD_STRING EQUAL -1)
        string(FIND "${CONTENT}" "#include <string>" HAS_STRING_INCLUDE)
        if(HAS_STRING_INCLUDE EQUAL -1)
            string(FIND "${CONTENT}" "#include <string_view>" HAS_SV)
            if(NOT HAS_SV EQUAL -1)
                string(REPLACE "#include <string_view>" "#include <string>\n#include <string_view>" CONTENT "${CONTENT}")
            else()
                string(FIND "${CONTENT}" "#include <cstdint>" HAS_CSTDINT)
                if(NOT HAS_CSTDINT EQUAL -1)
                    string(REPLACE "#include <cstdint>" "#include <cstdint>\n#include <string>" CONTENT "${CONTENT}")
                else()
                    string(REPLACE "#pragma once" "#pragma once\n\n#include <string>" CONTENT "${CONTENT}")
                endif()
            endif()
            file(WRITE "${HEADER}" "${CONTENT}")
            math(EXPR PATCHED_COUNT "${PATCHED_COUNT} + 1")
        endif()
    endif()
endforeach()

file(GLOB_RECURSE ALL_CPP_FILES "${CESIUM_SRC_DIR}/Cesium*/**/*.cpp")
foreach(CPP_FILE ${ALL_CPP_FILES})
    file(READ "${CPP_FILE}" CONTENT)
    set(OLD_CONTENT "${CONTENT}")
    
    string(REGEX REPLACE "([A-Za-z0-9_]+)\\.GetParseError\\(\\)([,\\)])" "static_cast<int>(\\1.GetParseError())\\2" CONTENT "${CONTENT}")
    
    string(REPLACE "GetParseError_En(static_cast<int>(d.GetParseError()))" "GetParseError_En(d.GetParseError())" CONTENT "${CONTENT}")
    string(REPLACE "GetParseError_En(static_cast<int>(document.GetParseError()))" "GetParseError_En(document.GetParseError())" CONTENT "${CONTENT}")
    string(REPLACE "GetParseError_En(static_cast<int>(doc.GetParseError()))" "GetParseError_En(doc.GetParseError())" CONTENT "${CONTENT}")
    
    if(NOT "${CONTENT}" STREQUAL "${OLD_CONTENT}")
        file(WRITE "${CPP_FILE}" "${CONTENT}")
    endif()
endforeach()

set(ROOT_CMAKELISTS "${CESIUM_SRC_DIR}/CMakeLists.txt")
if(EXISTS "${ROOT_CMAKELISTS}")
    file(READ "${ROOT_CMAKELISTS}" CONTENT)
    
    string(REGEX REPLACE "install\\(TARGETS [^\n]+\\)" "" CONTENT "${CONTENT}")
    string(REGEX REPLACE "install\\(DIRECTORY [^\n]+\\)" "" CONTENT "${CONTENT}")
    string(REGEX REPLACE "install\\(FILES [^\n]+\\)" "" CONTENT "${CONTENT}")
    
    string(REGEX REPLACE "set_target_properties\\(spdlog [^\n]+\\)" "" CONTENT "${CONTENT}")
    
    string(FIND "${CONTENT}" "add_library(spdlog ALIAS spdlog::spdlog)" HAS_ALIAS)
    if(HAS_ALIAS EQUAL -1)
        string(REPLACE "project(CesiumNative)" "project(CesiumNative)\n\nif(TARGET spdlog::spdlog AND NOT TARGET spdlog)\n    add_library(spdlog ALIAS spdlog::spdlog)\nendif()" CONTENT "${CONTENT}")
    endif()
    
    file(WRITE "${ROOT_CMAKELISTS}" "${CONTENT}")
endif()

file(GLOB_RECURSE SUB_CMAKELISTS "${CESIUM_SRC_DIR}/**/CMakeLists.txt" "${CESIUM_SRC_DIR}/CMakeLists.txt")
foreach(CMAKELIST ${SUB_CMAKELISTS})
    file(READ "${CMAKELIST}" CONTENT)
    set(OLD_CONTENT "${CONTENT}")
    string(REGEX REPLACE "([ \r\n\t])spdlog([ \r\n\t\\)])" "\\1spdlog::spdlog\\2" CONTENT "${CONTENT}")
    
    # Only replace target_link_libraries_system usages, not the definition
    if(NOT "${CMAKELIST}" STREQUAL "${ROOT_CMAKELISTS}")
        string(REPLACE "target_link_libraries_system" "target_link_libraries" CONTENT "${CONTENT}")
    endif()
    
    string(REPLACE "/WX" "" CONTENT "${CONTENT}")
    string(REPLACE "add_compile_options(/WX)" "" CONTENT "${CONTENT}")
    
    if(NOT "${CONTENT}" STREQUAL "${OLD_CONTENT}")
        file(WRITE "${CMAKELIST}" "${CONTENT}")
    endif()
endforeach()

message(STATUS "Patched ${PATCHED_COUNT} cesium-native headers with missing <string> include and patched CMakeLists to avoid spdlog alias install errors.")
