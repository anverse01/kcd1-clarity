#pragma once
#include <d3d11.h>

// Saves and restores the D3D11 immediate-context state our passes and NGX can touch, by plain Get/Set calls.
//
// We used SwapDeviceContextState before, but it also resets driver state that lives outside D3D11 (NVAPI depth
// bounds test, which KCD's deferred lighting uses). The engine caches that state and never sets it again, so
// lighting went black in blocks and stayed black (kcd-re db/notes/10-taa.md, "black-screen bug").
namespace kc {

class StateBackup {
public:
    explicit StateBackup(ID3D11DeviceContext* ctx);
    ~StateBackup();  // restores
    StateBackup(const StateBackup&) = delete;
    StateBackup& operator=(const StateBackup&) = delete;

private:
    static constexpr UINT kSrv = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
    static constexpr UINT kCb = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
    static constexpr UINT kSamp = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
    static constexpr UINT kVb = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
    static constexpr UINT kRt = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
    static constexpr UINT kVp = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    static constexpr UINT kInst = 256;

    struct Stage {
        ID3D11DeviceChild* shader = nullptr;
        ID3D11ClassInstance* inst[kInst] = {};
        UINT ninst = 0;
        ID3D11ShaderResourceView* srv[kSrv] = {};
        ID3D11Buffer* cb[kCb] = {};
        ID3D11SamplerState* samp[kSamp] = {};
    };

    ID3D11DeviceContext* ctx_;
    UINT uav_slots_;
    Stage vs_, ps_, cs_;
    ID3D11GeometryShader* gs_ = nullptr;
    ID3D11HullShader* hs_ = nullptr;
    ID3D11DomainShader* ds_ = nullptr;
    ID3D11UnorderedAccessView* cs_uav_[D3D11_1_UAV_SLOT_COUNT] = {};

    ID3D11InputLayout* layout_ = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11Buffer* vb_[kVb] = {};
    UINT vb_stride_[kVb] = {}, vb_offset_[kVb] = {};
    ID3D11Buffer* ib_ = nullptr;
    DXGI_FORMAT ib_format_ = DXGI_FORMAT_UNKNOWN;
    UINT ib_offset_ = 0;

    ID3D11RenderTargetView* rtv_[kRt] = {};
    ID3D11DepthStencilView* dsv_ = nullptr;
    ID3D11BlendState* blend_ = nullptr;
    float blend_factor_[4] = {};
    UINT sample_mask_ = 0;
    ID3D11DepthStencilState* depth_ = nullptr;
    UINT stencil_ref_ = 0;
    ID3D11RasterizerState* raster_ = nullptr;
    D3D11_VIEWPORT vp_[kVp] = {};
    UINT nvp_ = kVp;
    D3D11_RECT scissor_[kVp] = {};
    UINT nscissor_ = kVp;
};

}  // namespace kc
