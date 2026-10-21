#include "renderer/mip_bias.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

#include "core/log.h"

namespace kc::mip_bias {

struct Clone {
    ID3D11SamplerState* sampler = nullptr;  // null: this sampler is never biased
    float bias = 0;
};

static std::atomic<float> g_bias{0.0f};
static std::mutex g_mutex;  // PSSetSamplers can come from deferred contexts on other threads
// Key holds a reference so its address cannot be reused by another sampler while it is in the map.
static std::unordered_map<ID3D11SamplerState*, Clone> g_clones;
static Status g_status;

void set_bias(float b) {
    if (b != g_bias.load(std::memory_order_relaxed))
        logf("mip_bias: %.3f", b);
    g_bias.store(b, std::memory_order_relaxed);
}
float bias() { return g_bias.load(std::memory_order_relaxed); }
const Status& status() { return g_status; }

// Material samplers: anisotropic, or trilinear with wrap addressing. Post-process samplers are mostly clamp
// and point/linear without mips (maxlod 0); comparison samplers are shadow lookups.
static bool wants_bias(const D3D11_SAMPLER_DESC& d) {
    if (D3D11_DECODE_IS_COMPARISON_FILTER(d.Filter) || d.MaxLOD <= 0.0f)
        return false;
    if (D3D11_DECODE_IS_ANISOTROPIC_FILTER(d.Filter))
        return true;
    return D3D11_DECODE_MIP_FILTER(d.Filter) == D3D11_FILTER_TYPE_LINEAR && d.AddressU == D3D11_TEXTURE_ADDRESS_WRAP;
}

static ID3D11SamplerState* get(ID3D11DeviceContext* ctx, ID3D11SamplerState* s, float b) {
    auto it = g_clones.find(s);
    if (it != g_clones.end() && (it->second.bias == b || !it->second.sampler))
        return it->second.sampler;
    D3D11_SAMPLER_DESC d;
    s->GetDesc(&d);
    if (it == g_clones.end()) {
        s->AddRef();
        it = g_clones.emplace(s, Clone{}).first;
        if (!wants_bias(d))
            return nullptr;
    }
    Clone& c = it->second;
    if (c.sampler) {
        c.sampler->Release();  // a binding still holds its own reference
        c.sampler = nullptr;
        --g_status.clones;
    }
    d.MipLODBias = d.MipLODBias + b;
    if (d.MipLODBias < D3D11_MIP_LOD_BIAS_MIN)
        d.MipLODBias = D3D11_MIP_LOD_BIAS_MIN;
    ID3D11Device* device = nullptr;
    ctx->GetDevice(&device);
    HRESULT hr = device->CreateSamplerState(&d, &c.sampler);
    device->Release();
    if (FAILED(hr)) {
        logf("mip_bias: CreateSamplerState failed 0x%08lx", hr);
        c.sampler = nullptr;
        g_clones.erase(it);  // retried on the next bind
        s->Release();
        return nullptr;
    }
    c.bias = b;
    ++g_status.clones;
    return c.sampler;
}

bool remap(ID3D11DeviceContext* ctx, UINT count, ID3D11SamplerState* const* in, ID3D11SamplerState** out) {
    float b = bias();
    if (b == 0.0f || !in || count > D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
        return false;
    bool any = false;
    std::lock_guard lock(g_mutex);
    for (UINT i = 0; i < count; ++i) {
        ID3D11SamplerState* r = in[i] ? get(ctx, in[i], b) : nullptr;
        out[i] = r ? r : in[i];
        if (r) {
            any = true;
            ++g_status.remapped;
        }
    }
    return any;
}

}  // namespace kc::mip_bias
