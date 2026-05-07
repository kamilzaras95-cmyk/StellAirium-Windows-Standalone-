INCLUDE_DIRECTORIES(.)

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

SET(StellAirium_SRCS
    StellAirium.hpp
    StellAirium.cpp
    Aircraft.hpp
    Aircraft.cpp
    StellAiriumDialog.hpp
    StellAiriumDialog.cpp
)

SET(StellAirium_RES "${CMAKE_CURRENT_SOURCE_DIR}/../resources/StellAirium.qrc")

IF(${QT_VERSION_MAJOR} EQUAL "5")
    QT5_ADD_RESOURCES(StellAirium_RES_CXX ${StellAirium_RES})
ELSE()
    QT_ADD_RESOURCES(StellAirium_RES_CXX ${StellAirium_RES})
ENDIF()

ADD_LIBRARY(StellAirium-static STATIC
    ${StellAirium_SRCS}
    ${StellAirium_RES_CXX}
)

TARGET_LINK_LIBRARIES(StellAirium-static
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::OpenGL
)

SET_TARGET_PROPERTIES(StellAirium-static PROPERTIES OUTPUT_NAME "StellAirium")

IF(MSVC)
    SET_TARGET_PROPERTIES(StellAirium-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
ELSE()
    SET_TARGET_PROPERTIES(StellAirium-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN -Wno-unused-parameter")
ENDIF()

ADD_DEPENDENCIES(AllStaticPlugins StellAirium-static)
SET_TARGET_PROPERTIES(StellAirium-static PROPERTIES FOLDER "plugins/StellAirium")
