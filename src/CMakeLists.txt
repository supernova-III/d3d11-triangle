add_executable(${PROJECT_NAME} WIN32 main.cpp basic_pixel_shader.hlsl basic_vertex_shader.hlsl)
set_source_files_properties(basic_vertex_shader.hlsl PROPERTIES VS_SHADER_TYPE Vertex)
set_source_files_properties(basic_pixel_shader.hlsl PROPERTIES VS_SHADER_TYPE Pixel)
target_link_libraries(${PROJECT_NAME} d3d10 d3d11)
set_target_properties(${PROJECT_NAME} PROPERTIES 
RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>
)