add_library(client_o1 STATIC
    client_o1.cpp
)

target_include_directories(client_o1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link with pugixml
target_link_libraries(client_o1 pugixml)
