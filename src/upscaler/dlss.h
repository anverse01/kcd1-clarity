#pragma once
#include <d3d11.h>

#include <cstdint>

// Thin wrapper over the NGX DLSS super-sampling feature on D3D11. Render thread only.
namespace kc::dlss {

// Feature creation flags. Changing any of them recreates the feature.
struct Settings {
    bool auto_exposure = false;   // off: exposure texture 1.0
    bool is_hdr = true;           // colour is linear (true) or display-referred LDR (false)
    bool depth_inverted = false;  // 1 = near (reversed Z)
    bool mv_jittered = false;     // motion vectors include the jitter difference
};

struct Status {
    bool init_tried = false;
    bool available = false;      // NGX initialised and DLSS SR supported on this GPU/driver
    int init_result = 0;         // NVSDK_NGX_Result of Init / capability query, if it failed
    bool needs_driver = false;
    bool feature_created = false;
    int create_result = 0;
    int last_eval_result = 0;
    UINT in_width = 0, in_height = 0, out_width = 0, out_height = 0;
    uint64_t evaluations = 0;
    // OptiScaler (any proxy name) loaded in the process when NGX was initialised: our NGX calls then go
    // through it, and its menu (Insert) picks DLSS / FSR / XeSS. Empty when absent.
    char optiscaler_module[64] = {};
    char optiscaler_version[32] = {};
};

struct EvalInputs {
    ID3D11Resource* colour = nullptr;
    ID3D11Resource* depth = nullptr;   // R32_FLOAT
    ID3D11Resource* motion = nullptr;  // R16G16_FLOAT, scaled to pixels by mv_scale
    ID3D11Resource* output = nullptr;  // UAV-capable
    UINT render_width = 0, render_height = 0;
    float jitter_x = 0.0f, jitter_y = 0.0f;  // render pixels
    float mv_scale_x = 1.0f, mv_scale_y = 1.0f;
    bool reset = false;
};

Settings& settings();
const Status& status();

// Initialises NGX once, then (re)creates the DLAA/DLSS feature when sizes or settings change.
// Returns true when a feature is ready to evaluate. Sets *recreated when it made a new one.
bool ensure(ID3D11DeviceContext* ctx, UINT in_width, UINT in_height, UINT out_width, UINT out_height,
            bool* recreated);
void request_recreate();
bool evaluate(ID3D11DeviceContext* ctx, const EvalInputs& in);

}  // namespace kc::dlss
