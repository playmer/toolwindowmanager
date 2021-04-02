project(toolwindowmanager)
set(SOURCES 
ToolWindowManager.cpp
ToolWindowManagerArea.cpp
ToolWindowManagerSplitter.cpp
ToolWindowManagerTabBar.cpp
ToolWindowManagerWrapper.cpp
)

set(HEADERS
ToolWindowManager.h
ToolWindowManagerArea.h
ToolWindowManagerSplitter.h
ToolWindowManagerTabBar.h
ToolWindowManagerWrapper.h
)

set(MOC_SOURCES
    ${HEADERS})

find_package(Qt6 COMPONENTS Widgets REQUIRED)

qt6_wrap_cpp(OUT_MOC_FILES ${MOC_SOURCES})

add_library(toolwindowmanager STATIC ${SOURCES} ${HEADERS} ${OUT_MOC_FILES})
target_include_directories(toolwindowmanager 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(toolwindowmanager 
    Qt6::Core Qt6::Gui Qt6::Widgets)