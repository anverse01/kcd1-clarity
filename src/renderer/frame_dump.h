#pragma once
#include <d3d11.h>

#include <string>

// One-shot dump of DLSS inputs/outputs and the back buffer to BMP files in <mod dir>\kcd_dumps\, for debugging
// what RenderDoc cannot capture (NGX does not run under RenderDoc). Render thread only.
namespace kc::frame_dump {

void request();          // from the UI: dump the next DLSS frame and the following Present
bool armed();            // the TAA path should call save() this frame
void save(ID3D11DeviceContext* ctx, ID3D11Resource* res, const char* name);
void on_present(IDXGISwapChain* swapchain);  // saves the back buffer if a dump is in progress, then finishes
const std::string& last_result();

}  // namespace kc::frame_dump
