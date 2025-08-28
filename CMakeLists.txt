cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(nativesigscan 
  LANGUAGES CXX
  VERSION 0.1.0
)

# modern c++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# platform-aware detection of binary ninja api revision
function(detect_binja_api_revision OUTPUT_VAR)
  unset(${OUTPUT_VAR} PARENT_SCOPE)
  
  if(APPLE)
    set(API_REVISION_PATH "/Applications/Binary Ninja.app/Contents/Resources/api_REVISION.txt")
    if(EXISTS "${API_REVISION_PATH}")
      file(READ "${API_REVISION_PATH}" API_CONTENT)
      # extract revision hash from the url format
      if(API_CONTENT MATCHES "/tree/([0-9a-fA-F]+)")
        set(REVISION_HASH "${CMAKE_MATCH_1}")
        # validate it's exactly 40 chars (git sha1)
        string(LENGTH "${REVISION_HASH}" HASH_LENGTH)
        if(NOT HASH_LENGTH EQUAL 40)
          set(REVISION_HASH "")
        endif()
      endif()
      if(REVISION_HASH)
        message(STATUS "auto-detected binary ninja api revision: ${REVISION_HASH}")
        set(${OUTPUT_VAR} "${REVISION_HASH}" PARENT_SCOPE)
        return()
      endif()
    endif()
    message(STATUS "could not auto-detect api revision on macos")
    
  elseif(WIN32)
    # check user install first, then global
    set(USER_PATH "$ENV{LOCALAPPDATA}/Vector35/BinaryNinja/api_REVISION.txt")
    set(GLOBAL_PATH "C:/Program Files/Vector35/BinaryNinja/api_REVISION.txt")
    
    foreach(CHECK_PATH IN ITEMS "${USER_PATH}" "${GLOBAL_PATH}")
      if(EXISTS "${CHECK_PATH}")
        file(READ "${CHECK_PATH}" API_CONTENT)
        if(API_CONTENT MATCHES "/tree/([0-9a-fA-F]+)")
          set(REVISION_HASH "${CMAKE_MATCH_1}")
          string(LENGTH "${REVISION_HASH}" HASH_LENGTH)
          if(NOT HASH_LENGTH EQUAL 40)
            set(REVISION_HASH "")
          endif()
        endif()
        if(REVISION_HASH)
          message(STATUS "auto-detected binary ninja api revision: ${REVISION_HASH}")
          set(${OUTPUT_VAR} "${REVISION_HASH}" PARENT_SCOPE)
          return()
        endif()
      endif()
    endforeach()
    message(STATUS "could not auto-detect api revision on windows")
    
  else()
    message(STATUS "auto-detection not supported on this platform")
  endif()
endfunction()

# fetch and configure binary ninja api using fetchcontent
function(fetch_binja_api REVISION)
  include(FetchContent)
  
  message(STATUS "fetching binary ninja api at revision: ${REVISION}")
  
  FetchContent_Declare(binaryninjaapi
    GIT_REPOSITORY https://github.com/Vector35/binaryninja-api.git
    GIT_TAG        ${REVISION}
    GIT_SUBMODULES_RECURSE ON
  )
  
  # configure api build options
  set(BN_API_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(HEADLESS ON CACHE BOOL "" FORCE)
  
  FetchContent_MakeAvailable(binaryninjaapi)
endfunction()

# determine api revision to use
if(NOT DEFINED BINARYNINJA_API_REVISION)
  detect_binja_api_revision(DETECTED_REVISION)
  if(DETECTED_REVISION)
    set(BINARYNINJA_API_REVISION "${DETECTED_REVISION}")
  else()
    message(FATAL_ERROR 
      "binary ninja api revision not specified and auto-detection failed.\n"
      "please provide: -DBINARYNINJA_API_REVISION=<commit_hash>"
    )
  endif()
else()
  message(STATUS "using manually specified api revision: ${BINARYNINJA_API_REVISION}")
endif()

# fetch the api
fetch_binja_api(${BINARYNINJA_API_REVISION})

# build the plugin
add_library(${PROJECT_NAME} SHARED
  src/sigscan.cpp
)

# link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
  binaryninjaapi
  fmt::fmt
)

# plugin-specific properties
set_target_properties(${PROJECT_NAME} PROPERTIES
  PREFIX ""  # no lib prefix for plugins
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
