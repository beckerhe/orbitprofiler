project(Widgets)

add_library(Widgets STATIC)

target_sources(Widgets PRIVATE FilterBarTableViewWidget.cpp FilterBarTableViewWidget.ui)
target_sources(Widgets PUBLIC include/Widgets/FilterBarTableViewWidget.h)

target_include_directories(Widgets PUBLIC include/)
target_link_libraries(Widgets PUBLIC Qt5::Widgets)
set_target_properties(Widgets PROPERTIES AUTOMOC ON)
set_target_properties(Widgets PROPERTIES AUTOUIC ON)