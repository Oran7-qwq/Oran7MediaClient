set(RENDER "${ORAN7_SOURCE_ROOT}/src/cpp/Oran7VideoGPURender")
add_executable(render_sync_checks main.cpp "${RENDER}/D3d11Videoitem.cpp" "${RENDER}/D3d11Videoitem.h"
    "${RENDER}/D3d11VideoRenderNode.cpp" "${RENDER}/D3d11DeviceProvider.cpp" "${RENDER}/D3d11DeviceProvider.h")
target_include_directories(render_sync_checks PRIVATE "${RENDER}" "${FFMPEG_ROOT_DIR}/include")
target_link_libraries(render_sync_checks PRIVATE Oran7Core Qt6::Quick Qt6::GuiPrivate Qt6::Test
    "${FFMPEG_ROOT_DIR}/lib/avutil.lib" "${FFMPEG_ROOT_DIR}/lib/swscale.lib" d3d11 dxgi)
qt_add_shaders(render_sync_checks "render_sync_shaders" PREFIX "/" BASE "${ORAN7_SOURCE_ROOT}"
    FILES "${ORAN7_SOURCE_ROOT}/shaders/video.vert" "${ORAN7_SOURCE_ROOT}/shaders/video.frag")
