project(ItemModels CXX)

add_library(ItemModels STATIC)

target_sources(ItemModels PRIVATE PresetItemModel.cpp ProcessItemModel.cpp)
target_sources(ItemModels PUBLIC include/ItemModels/PresetItemModel.h
                                 include/ItemModels/ProcessItemModel.h)

target_include_directories(ItemModels PUBLIC include/)
target_link_libraries(ItemModels PUBLIC OrbitBase OrbitProtos OrbitCore
                                        Qt5::Core)
