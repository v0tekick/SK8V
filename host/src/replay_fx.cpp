#include "replay_fx.h"
#include "d3d11_saved_state.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <d3d11.h>
#include <windows.h>

namespace replayfx {
namespace {
using d3dstate::SafeRelease;
std::atomic<int> selected{0};
std::atomic<bool> showUi{false};
ID3D11Device* device = nullptr;
ID3D11DeviceContext* context = nullptr;
ID3D11VertexShader* vs = nullptr;
ID3D11PixelShader* ps = nullptr;
ID3D11Buffer* constants = nullptr;
ID3D11SamplerState* sampler = nullptr;
ID3D11DepthStencilState* noDepth = nullptr;
ID3D11Texture2D* copy = nullptr;
ID3D11ShaderResourceView* copyView = nullptr;
UINT width = 0, height = 0;
DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

constexpr char shader[] = R"(
cbuffer Params : register(b0) { float2 size; float time; int effect; int showUi; float3 padding; };
Texture2D scene : register(t0);
SamplerState linearClamp : register(s0);
struct Vert { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
Vert VS(uint id : SV_VertexID) {
    Vert o;
    float2 p = float2((id << 1) & 2, id & 2);
    o.pos = float4(p * float2(2,-2) + float2(-1,1), 0, 1);
    o.uv = p;
    return o;
}
float hash(float2 p) { return frac(sin(dot(p, float2(127.1,311.7))) * 43758.5453); }
float4 PS(Vert i) : SV_Target {
    float2 uv = i.uv;
    float3 c;
    // The replay chrome is drawn by GTA into opaque bands before Present.
    // Preserve it after the post effect. With HUD hidden the bands stay black.
    const float top = 0.085;
    const float bottom = 0.855;
    if (uv.y < top || uv.y > bottom) {
        if (showUi != 0) return scene.Sample(linearClamp, uv);
        if (effect == 2 || effect == 4) return float4(0,0,0,1);
    }
    if (effect == 1) {
        // Barrel lens: sample increasingly far from the centre. The source
        // camera has a 110+ degree FOV, so the edge pixels have scene data.
        float2 q = uv * 2 - 1;
        float r2 = dot(q,q);
        float2 bent = q * (1 + 0.22 * r2);
        float2 sourceUv = bent * 0.5 + 0.5;
        c = scene.Sample(linearClamp, sourceUv).rgb;
        float aperture = 1 - smoothstep(1.22, 1.48, length(q));
        c *= aperture * (1 - 0.25 * smoothstep(0.35, 1.35, length(q)));
        c = saturate((c - 0.5) * 1.08 + 0.5);
    } else if (effect == 2) {
        // A real 4:3 crop uses the original source coordinates. Remapping the
        // full 16:9 image into the gate would squeeze every object in the shot.
        float halfWidth = min(0.5, (bottom-top) * (4.0/3.0) * size.y / size.x * 0.5);
        if (abs(uv.x - 0.5) > halfWidth) return float4(0,0,0,1);
        float2 gate = uv;
        float tracking = smoothstep(0.015,0.0,abs(frac(uv.y * 0.7 - time * 0.12)-0.5));
        gate.x += (hash(float2(floor(uv.y*size.y/3), floor(time*12))) - 0.5) * 0.003;
        gate.x += tracking * 0.012;
        float r = scene.Sample(linearClamp, gate + float2(0.002,0)).r;
        float g = scene.Sample(linearClamp, gate).g;
        float b = scene.Sample(linearClamp, gate - float2(0.002,0)).b;
        c = float3(r,g,b);
        float scan = 0.90 + 0.10 * sin(uv.y * size.y * 3.14159);
        float noise = (hash(floor(uv*size) + floor(time*29)) - 0.5) * 0.09;
        c = saturate(c * float3(1.08,1.03,0.91) * scan + noise + tracking*0.09);
    } else if (effect == 4) {
        // Square sensor crop with a round doorbell lens. x and y offsets stay
        // in their original pixel scale, so the scene is cropped, never squeezed.
        float halfWidth = min(0.5, (bottom-top) * size.y / size.x * 0.5);
        float2 q = float2((uv.x-0.5)/halfWidth, (uv.y-(top+bottom)*0.5)/((bottom-top)*0.5));
        float radius = length(q);
        if (abs(uv.x-0.5) > halfWidth || uv.y < top || uv.y > bottom || radius > 1.0)
            return float4(0,0,0,1);
        float2 sourceUv = float2(0.5, (top+bottom)*0.5) +
                          float2(q.x*halfWidth, q.y*(bottom-top)*0.5) * (1.0 + 0.20*radius*radius);
        c = scene.Sample(linearClamp, sourceUv).rgb;
        c = saturate((c-0.5)*1.09+0.53);
        c *= 1.0 - 0.32*smoothstep(0.55,1.0,radius);
        c *= 1.0 - smoothstep(0.965,1.0,radius);
        c += (hash(floor(uv*size)+floor(time*8))-0.5)*0.018;
    } else if (effect == 0) {
        c = scene.Sample(linearClamp,uv).rgb;
    } else {
        c = scene.Sample(linearClamp,uv).rgb;
        float luminance = dot(c,float3(0.2126,0.7152,0.0722));
        c = lerp(float3(luminance,luminance,luminance),c,0.36);
        c = saturate((c-0.5)*1.18+0.5 + (hash(floor(uv*size)+floor(time*24))-0.5)*0.035);
        c *= 1 - 0.3 * smoothstep(0.45,1.1,length(uv*2-1));
    }
    return float4(c,1);
}
)";

void ReleaseSize() {
    SafeRelease(copyView);
    SafeRelease(copy);
    width = height = 0;
    format = DXGI_FORMAT_UNKNOWN;
}
void ReleaseAll() {
    ReleaseSize();
    SafeRelease(vs); SafeRelease(ps); SafeRelease(constants);
    SafeRelease(sampler); SafeRelease(noDepth);
    SafeRelease(context); SafeRelease(device);
}
bool Prepare(ID3D11Device* d) {
    if (device == d && vs && ps) return true;
    ReleaseAll();
    device = d; device->AddRef(); device->GetImmediateContext(&context);
    auto* v = d3dstate::CompileShader(shader,sizeof(shader)-1,"replay_fx.hlsl","VS","vs_5_0",nullptr,"Replay FX");
    auto* p = d3dstate::CompileShader(shader,sizeof(shader)-1,"replay_fx.hlsl","PS","ps_5_0",nullptr,"Replay FX");
    const bool ok = v && p && SUCCEEDED(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs)) &&
                    SUCCEEDED(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps));
    SafeRelease(v); SafeRelease(p);
    if (!ok) { ReleaseAll(); return false; }
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth = 32; cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    D3D11_DEPTH_STENCIL_DESC dd{}; dd.DepthEnable = FALSE; dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    return SUCCEEDED(device->CreateBuffer(&cb,nullptr,&constants)) &&
           SUCCEEDED(device->CreateSamplerState(&sd,&sampler)) &&
           SUCCEEDED(device->CreateDepthStencilState(&dd,&noDepth));
}
} // namespace

void Set(int effect, bool uiVisible) {
    showUi.store(uiVisible, std::memory_order_release);
    selected.store(std::clamp(effect,0,4),std::memory_order_release);
}

void Present(IDXGISwapChain* chain) {
    const int effect = selected.load(std::memory_order_acquire);
    if (!effect || !chain) return;
    ID3D11Device* d = nullptr;
    if (FAILED(chain->GetDevice(__uuidof(ID3D11Device),reinterpret_cast<void**>(&d))) || !d) return;
    if (!Prepare(d)) { d->Release(); return; }
    d->Release();
    ID3D11Texture2D* back = nullptr;
    if (FAILED(chain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&back))) || !back) return;
    D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc);
    if (desc.SampleDesc.Count != 1) { back->Release(); return; }
    if (!copy || desc.Width != width || desc.Height != height || desc.Format != format) {
        ReleaseSize();
        D3D11_TEXTURE2D_DESC cd = desc;
        cd.BindFlags = D3D11_BIND_SHADER_RESOURCE; cd.CPUAccessFlags = 0;
        cd.Usage = D3D11_USAGE_DEFAULT; cd.MiscFlags = 0;
        if (FAILED(device->CreateTexture2D(&cd,nullptr,&copy)) ||
            FAILED(device->CreateShaderResourceView(copy,nullptr,&copyView))) {
            ReleaseSize(); back->Release(); return;
        }
        width=desc.Width; height=desc.Height; format=desc.Format;
    }
    ID3D11RenderTargetView* target = nullptr;
    if (FAILED(device->CreateRenderTargetView(back,nullptr,&target))) { back->Release(); return; }
    d3dstate::SavedState saved; saved.Save(context);
    ID3D11ShaderResourceView* none = nullptr;
    context->PSSetShaderResources(0,1,&none);
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->CopyResource(copy,back);
    D3D11_MAPPED_SUBRESOURCE map{};
    if (SUCCEEDED(context->Map(constants,0,D3D11_MAP_WRITE_DISCARD,0,&map))) {
        struct Params { float size[2], time; int effect, showUi; float padding[3]; }
            params{{static_cast<float>(width),static_cast<float>(height)},GetTickCount()*0.001f,effect,
                   showUi.load(std::memory_order_acquire) ? 1 : 0,{}};
        std::memcpy(map.pData,&params,sizeof(params));
        context->Unmap(constants,0);
        D3D11_VIEWPORT vp{0,0,static_cast<float>(width),static_cast<float>(height),0,1};
        context->RSSetViewports(1,&vp);
        context->RSSetState(nullptr);
        context->OMSetRenderTargets(1,&target,nullptr);
        context->OMSetDepthStencilState(noDepth,0);
        context->OMSetBlendState(nullptr,nullptr,0xffffffff);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs,nullptr,0);
        context->PSSetShader(ps,nullptr,0);
        context->GSSetShader(nullptr,nullptr,0);
        context->HSSetShader(nullptr,nullptr,0);
        context->DSSetShader(nullptr,nullptr,0);
        context->PSSetConstantBuffers(0,1,&constants);
        context->PSSetShaderResources(0,1,&copyView);
        context->PSSetSamplers(0,1,&sampler);
        context->Draw(3,0);
        context->PSSetShaderResources(0,1,&none);
    }
    saved.Restore(context);
    target->Release(); back->Release();
}

void Stop() { selected.store(0); showUi.store(false); ReleaseAll(); }
} // namespace replayfx
