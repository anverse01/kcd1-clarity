#include "renderer/state_backup.h"

#include <initializer_list>

namespace kc {

template <typename T>
static void release(T** p, UINT n) {
    for (UINT i = 0; i < n; ++i)
        if (p[i])
            p[i]->Release();
}

template <typename T>
static void release(T* p) {
    if (p)
        p->Release();
}

StateBackup::StateBackup(ID3D11DeviceContext* ctx) : ctx_(ctx) {
    ID3D11Device* device = nullptr;
    ctx->GetDevice(&device);
    uav_slots_ = device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1 ? D3D11_1_UAV_SLOT_COUNT
                                                                     : D3D11_PS_CS_UAV_REGISTER_COUNT;
    device->Release();

    vs_.ninst = kInst;
    ctx->VSGetShader(reinterpret_cast<ID3D11VertexShader**>(&vs_.shader), vs_.inst, &vs_.ninst);
    ctx->VSGetShaderResources(0, kSrv, vs_.srv);
    ctx->VSGetConstantBuffers(0, kCb, vs_.cb);
    ctx->VSGetSamplers(0, kSamp, vs_.samp);
    ps_.ninst = kInst;
    ctx->PSGetShader(reinterpret_cast<ID3D11PixelShader**>(&ps_.shader), ps_.inst, &ps_.ninst);
    ctx->PSGetShaderResources(0, kSrv, ps_.srv);
    ctx->PSGetConstantBuffers(0, kCb, ps_.cb);
    ctx->PSGetSamplers(0, kSamp, ps_.samp);
    cs_.ninst = kInst;
    ctx->CSGetShader(reinterpret_cast<ID3D11ComputeShader**>(&cs_.shader), cs_.inst, &cs_.ninst);
    ctx->CSGetShaderResources(0, kSrv, cs_.srv);
    ctx->CSGetConstantBuffers(0, kCb, cs_.cb);
    ctx->CSGetSamplers(0, kSamp, cs_.samp);
    ctx->CSGetUnorderedAccessViews(0, uav_slots_, cs_uav_);
    ctx->GSGetShader(&gs_, nullptr, nullptr);
    ctx->HSGetShader(&hs_, nullptr, nullptr);
    ctx->DSGetShader(&ds_, nullptr, nullptr);

    ctx->IAGetInputLayout(&layout_);
    ctx->IAGetPrimitiveTopology(&topology_);
    ctx->IAGetVertexBuffers(0, kVb, vb_, vb_stride_, vb_offset_);
    ctx->IAGetIndexBuffer(&ib_, &ib_format_, &ib_offset_);

    ctx->OMGetRenderTargets(kRt, rtv_, &dsv_);
    ctx->OMGetBlendState(&blend_, blend_factor_, &sample_mask_);
    ctx->OMGetDepthStencilState(&depth_, &stencil_ref_);
    ctx->RSGetState(&raster_);
    ctx->RSGetViewports(&nvp_, vp_);
    ctx->RSGetScissorRects(&nscissor_, scissor_);
}

StateBackup::~StateBackup() {
    ID3D11DeviceContext* c = ctx_;
    // Unbind our outputs first so restoring the engine's SRVs never hits a read/write hazard.
    ID3D11UnorderedAccessView* null_uav[D3D11_1_UAV_SLOT_COUNT] = {};
    c->CSSetUnorderedAccessViews(0, uav_slots_, null_uav, nullptr);
    c->OMSetRenderTargets(0, nullptr, nullptr);

    c->VSSetShader(static_cast<ID3D11VertexShader*>(vs_.shader), vs_.inst, vs_.ninst);
    c->VSSetShaderResources(0, kSrv, vs_.srv);
    c->VSSetConstantBuffers(0, kCb, vs_.cb);
    c->VSSetSamplers(0, kSamp, vs_.samp);
    c->PSSetShader(static_cast<ID3D11PixelShader*>(ps_.shader), ps_.inst, ps_.ninst);
    c->PSSetShaderResources(0, kSrv, ps_.srv);
    c->PSSetConstantBuffers(0, kCb, ps_.cb);
    c->PSSetSamplers(0, kSamp, ps_.samp);
    c->CSSetShader(static_cast<ID3D11ComputeShader*>(cs_.shader), cs_.inst, cs_.ninst);
    c->CSSetShaderResources(0, kSrv, cs_.srv);
    c->CSSetConstantBuffers(0, kCb, cs_.cb);
    c->CSSetSamplers(0, kSamp, cs_.samp);
    UINT keep_counts[D3D11_1_UAV_SLOT_COUNT];
    for (UINT& k : keep_counts)
        k = ~0u;
    c->CSSetUnorderedAccessViews(0, uav_slots_, cs_uav_, keep_counts);
    c->GSSetShader(gs_, nullptr, 0);
    c->HSSetShader(hs_, nullptr, 0);
    c->DSSetShader(ds_, nullptr, 0);

    c->IASetInputLayout(layout_);
    c->IASetPrimitiveTopology(topology_);
    c->IASetVertexBuffers(0, kVb, vb_, vb_stride_, vb_offset_);
    c->IASetIndexBuffer(ib_, ib_format_, ib_offset_);

    c->OMSetRenderTargets(kRt, rtv_, dsv_);
    c->OMSetBlendState(blend_, blend_factor_, sample_mask_);
    c->OMSetDepthStencilState(depth_, stencil_ref_);
    c->RSSetState(raster_);
    c->RSSetViewports(nvp_, vp_);
    c->RSSetScissorRects(nscissor_, scissor_);

    for (Stage* s : {&vs_, &ps_, &cs_}) {
        release(&s->shader, 1);
        release(s->inst, s->ninst);
        release(s->srv, kSrv);
        release(s->cb, kCb);
        release(s->samp, kSamp);
    }
    release(cs_uav_, uav_slots_);
    release(gs_);
    release(hs_);
    release(ds_);
    release(layout_);
    release(vb_, kVb);
    release(ib_);
    release(rtv_, kRt);
    release(dsv_);
    release(blend_);
    release(depth_);
    release(raster_);
}

}  // namespace kc
