# include paths for qreconduck, the internal reconduck API, and Python
set(MODULE_INCLUDES
    PRIVATE ${CMAKE_SOURCE_DIR}/qreconduck
    PRIVATE ${CMAKE_SOURCE_DIR}/reconduck/api/replay
    PRIVATE ${PYTHON_INCLUDE_DIR})
  
# preprocessor definitions for compiling
set(MODULE_DEFINES
    PRIVATE -DRECONDUCK_PLATFORM_LINUX)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option -Wno-unused-private-field -Wno-shadow -Wno-shorten-64-to-32")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 19)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-function-type")
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cast-function-type -Wno-stringop-truncation")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

# Set up rpath to find libreconduck.so
set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
if (APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "@loader_path/../lib")
    set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR TRUE)
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/:$ORIGIN/../lib${LIB_SUFFIX}/${LIB_SUBFOLDER_TRAIL_SLASH}")
endif()

# Add python library
set (CMAKE_SHARED_LINKER_FLAGS "${PYTHON_LIBRARY} ${CMAKE_SHARED_LINKER_FLAGS}")

add_library(_reconduck SHARED
    ${CMAKE_BINARY_DIR}/qreconduck/reconduck_python.cxx
    pyreconduck_stub.cpp)

set_source_files_properties(${CMAKE_BINARY_DIR}/qreconduck/reconduck_python.cxx
                            PROPERTIES GENERATED TRUE)

target_compile_definitions(_reconduck ${MODULE_DEFINES})
target_include_directories(_reconduck ${MODULE_INCLUDES})
target_link_libraries(_reconduck PRIVATE reconduck)

add_library(_qreconduck SHARED
    ${CMAKE_BINARY_DIR}/qreconduck/reconduck_python.cxx
    ${CMAKE_BINARY_DIR}/qreconduck/qreconduck_python.cxx
    pyreconduck_stub.cpp
    qreconduck_stub.cpp)

set_source_files_properties(${CMAKE_BINARY_DIR}/qreconduck/qreconduck_python.cxx
                            PROPERTIES GENERATED TRUE)

target_compile_definitions(_qreconduck ${MODULE_DEFINES})
target_include_directories(_qreconduck ${MODULE_INCLUDES})
target_link_libraries(_qreconduck PRIVATE reconduck)

# Python expects .so extension when importing shared libraries on non-Windows platform
if(APPLE)
    set_target_properties(_reconduck PROPERTIES SUFFIX ".so")
    set_target_properties(_qreconduck PROPERTIES SUFFIX ".so")
endif()

# Don't prefix with lib, python expects a bare .so.
# Also rename to non-underscore due to this (We couldn't call the
# python library 'reconduck' since that would clash with the
# prefixed target)
set_target_properties(_reconduck PROPERTIES PREFIX "")
set_target_properties(_qreconduck PROPERTIES PREFIX "")
set_target_properties(_reconduck PROPERTIES OUTPUT_NAME "reconduck")
set_target_properties(_qreconduck PROPERTIES OUTPUT_NAME "qreconduck")

# Make sure we build after the wrappers are generated
add_dependencies(_reconduck swig-bindings)
add_dependencies(_qreconduck swig-bindings)
