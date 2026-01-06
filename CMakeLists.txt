cmake_minimum_required(VERSION 3.21)
project(MeshCoreQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt integration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

# Source files
set(SOURCES
    main.cpp
    src/connection/SerialConnection.cpp
    src/protocol/CommandBuilder.cpp
    src/protocol/ResponseParser.cpp
    src/models/Channel.cpp
    src/models/Contact.cpp
    src/models/Message.cpp
    src/core/ChannelManager.cpp
    src/core/MeshClient.cpp
    src/ui/CLI/CommandLineInterface.cpp
)

set(HEADERS
    src/protocol/ProtocolConstants.h
    src/connection/ConnectionState.h
    src/connection/SerialConnection.h
    src/protocol/CommandBuilder.h
    src/protocol/ResponseParser.h
    src/core/DeviceInfo.h
    src/models/Channel.h
    src/models/Contact.h
    src/models/Message.h
    src/core/ChannelManager.h
    src/core/MeshClient.h
    src/ui/CLI/CommandLineInterface.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::SerialPort
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Platform-specific settings
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()
