add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE chip8.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE vendor)