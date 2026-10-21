#pragma once
#include <d3d11.h>

#include <cstdint>

// Occluded object velocities. The engine draws the moving objects first, into the G-buffer plus the velocity target
// (4 targets), and then all static geometry into the G-buffer only (3 targets, kcd_frame8668: eids 232-605, then
// 629-12873). A static object drawn in front of a moving one overwrites its depth and G-buffer but not its velocity,
// so the hidden dog's velocity stays on the obstacle. The engine TAA's neighbourhood clamp hides that; DLSS follows
// the vector and drags the obstacle's texture along: a faint moving dog outline (kcd-re notes/10-taa.md).
// Fix: when the G-buffer drops the velocity target (same depth target), copy the depth. When the static pass is over
// (next binding with another first target), a full-screen pass zeroes the velocity target wherever the depth no
// longer equals the copy, so the engine's own readers (SVO GI temporal reprojection) are fixed too. The DLSS prepare
// pass also checks the copy (fallback if the cleanup cannot run).
namespace kc::velocity_mask {

// OMSetRenderTargets hook, before the call.
using DrawFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
// draw: the unhooked Draw, for the cleanup pass.
void on_set_targets(ID3D11DeviceContext* ctx, UINT n, ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv,
                    DrawFn draw);
// Start of a frame: the copy is stale until the next transition.
void on_present();
// Depth copy taken this frame (SRV of the depth part), or nullptr.
ID3D11ShaderResourceView* depth_copy();

struct Status {
    bool enabled = true;
    uint64_t snapshots = 0, cleanups = 0;
};
Status& status();

}  // namespace kc::velocity_mask
