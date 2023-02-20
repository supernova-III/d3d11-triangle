#include <Windows.h>
#include <stdint.h>
#include <stddef.h>

#include <d3d11.h>
#include <d3d10.h>

extern "C" int _fltused = 0;

using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using u32 = uint32_t;
using i32 = int32_t;
using i64 = int64_t;
using u64 = uint64_t;
using usize = size_t;
using f32 = float;
using f64 = double;

// True and False 'literals' for 32-bit bool
enum bool32 : uint32_t {
    False,
    True
};

static struct {
    ::HINSTANCE hinstance;
    ::HDC context;
    ::HWND window;
} platform_data;

void ConsoleWrite(const char* str) {
    if (!str) {
        return;
    }

    u16 size = 0;
    while (str[size++] != '\0')
        ;

    ::DWORD w{};
    ::WriteFile(::GetStdHandle(STD_OUTPUT_HANDLE), str, size, &w, nullptr);
}

namespace {

::LRESULT wndProc(::HWND window, ::UINT msg, ::WPARAM wparam, ::LPARAM lparam) {
    return ::DefWindowProcA(window, msg, wparam, lparam);
}

bool32 openWindow(u32 w, u32 h) {
    ::WNDCLASSA wndclass{};
    wndclass.hInstance = platform_data.hinstance;
    wndclass.lpszClassName = "triangle";
    wndclass.lpfnWndProc = wndProc;
    wndclass.style = CS_HREDRAW | CS_VREDRAW;

    if (!::RegisterClassA(&wndclass)) {
        return False;
    }

    platform_data.window =
        ::CreateWindowA(wndclass.lpszClassName, wndclass.lpszClassName,
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h, nullptr,
            nullptr, platform_data.hinstance, nullptr);

    ::ShowWindow(platform_data.window, SW_SHOW);
    if (platform_data.window == INVALID_HANDLE_VALUE) {
        return False;
    }

    return True;
}

}  // namespace

inline int MainLoop() {
    ::AllocConsole();
    if (!openWindow(800, 600)) {
        ConsoleWrite("Failed to open window\n");
        return 1;
    }

    ::ID3D11Device* device{};
    ::ID3D11DeviceContext* device_context{};
    ::IDXGISwapChain* swapchain{};
    ::ID3D11RenderTargetView* render_target_view{};

    // 1. Creating device, device context, and swapchain
    ::DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.Windowed = true;
    scd.OutputWindow = platform_data.window;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ::UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifndef NDEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    ::D3D_FEATURE_LEVEL feature_level{};
    auto res = ::D3D11CreateDeviceAndSwapChain(nullptr,
        D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION,
        &scd, &swapchain, &device, &feature_level, &device_context);

    if (res != S_OK) {
        return 1;
    }

    if (!swapchain || !device || !device_context) {
        return 1;
    }

    // 2. Creating render target view
    ::ID3D11Texture2D* framebuffer{};
    res = swapchain->GetBuffer(
        0, __uuidof(::ID3D11Texture2D), (void**)&framebuffer);

    if (res != S_OK) {
        return 1;
    }

    res = device->CreateRenderTargetView(
        framebuffer, nullptr, &render_target_view);
    if (res != S_OK) {
        return 1;
    }
    framebuffer->Release();

    // 3. Create shaders. We are using precompiled shaders here.
    const auto vs_bytecode_handle = ::CreateFileA("basic_vertex_shader.cso",
        GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (vs_bytecode_handle == INVALID_HANDLE_VALUE) {
        return 1;
    }
    const u16 vs_bytecode_size = ::GetFileSize(vs_bytecode_handle, 0);
    if (!vs_bytecode_size) {
        return 1;
    }

    const auto ps_bytecode_handle = ::CreateFileA("basic_pixel_shader.cso",
        GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (ps_bytecode_handle == INVALID_HANDLE_VALUE) {
        return 1;
    }
    const u16 ps_bytecode_size = ::GetFileSize(ps_bytecode_handle, 0);
    if (!ps_bytecode_size) {
        return 1;
    }

    auto vs_bytecode =
        (u8*)::HeapAlloc(::GetProcessHeap(), 0, vs_bytecode_size);
    if (!vs_bytecode) {
        return 1;
    }

    auto ps_bytecode =
        (u8*)::HeapAlloc(::GetProcessHeap(), 0, ps_bytecode_size);
    if (!ps_bytecode) {
        return 1;
    }

    ::DWORD read{};
    auto result =
        ::ReadFile(vs_bytecode_handle, vs_bytecode, vs_bytecode_size, &read, 0);
    if (!result) {
        return 1;
    }

    result =
        ::ReadFile(ps_bytecode_handle, ps_bytecode, ps_bytecode_size, &read, 0);
    if (!result) {
        return 1;
    }

    ::ID3D11VertexShader* vertex_shader{};
    ::ID3D11PixelShader* pixel_shader{};
    res = device->CreateVertexShader(
        vs_bytecode, vs_bytecode_size, 0, &vertex_shader);
    if (res != S_OK) {
        return 1;
    }
    res = device->CreatePixelShader(
        ps_bytecode, ps_bytecode_size, 0, &pixel_shader);
    if (res != S_OK) {
        return 1;
    }

    // 3. Setting the layout for the vertex shader
    ::ID3D11InputLayout* input_layout{};
    // clang-format off
    ::D3D11_INPUT_ELEMENT_DESC desc[] {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    // clang format on
    res = device->CreateInputLayout(
        desc, 1, vs_bytecode, vs_bytecode_size, &input_layout);
    if (res != S_OK) {
        return 1;
    }

    // 4. Creating vertex buffer
    // clang-format off
    f32 vertex_data[] {
        0.0f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f
    };
    u32 stride = 3 * sizeof(f32);
    u32 offset = 0;
    u32 vcount = 3;
    // clang-format on

    ::ID3D11Buffer* vertex_buffer{};
    ::D3D11_BUFFER_DESC vbd{};
    vbd.ByteWidth = sizeof(vertex_data);
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    ::D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex_data;

    res = device->CreateBuffer(&vbd, &sd, &vertex_buffer);
    if (res != S_OK) {
        return 1;
    }

    const f32 color[4]{0.1f, 0.1f, 0.1f, 1.0f};
    // 5. Drawing!
    while (true) {
        ::MSG msg{};
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        device_context->ClearRenderTargetView(render_target_view, color);

        ::RECT rect{};
        ::GetClientRect(platform_data.window, &rect);
        ::D3D11_VIEWPORT viewport = {0.0f, 0.0f,
            (::FLOAT)(rect.right - rect.left),
            (::FLOAT)(rect.bottom - rect.top), 0.0f, 1.0f};

        // Rasterization
        device_context->RSSetViewports(1, &viewport);
        // Input assembly
        device_context->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        device_context->IASetInputLayout(input_layout);
        device_context->IASetVertexBuffers(
            0, 1, &vertex_buffer, &stride, &offset);

        // Vertex shader
        device_context->VSSetShader(vertex_shader, nullptr, 0);

        // Pixel shader
        device_context->PSSetShader(pixel_shader, nullptr, 0);

        // Output merger
        device_context->OMSetRenderTargets(1, &render_target_view, nullptr);

        // Draw command
        device_context->Draw(vcount, 0);

        // Present
        swapchain->Present(1, 0);
    }

    return 0;
}

extern "C" void WinMainCRTStartup() {
    platform_data.hinstance = ::GetModuleHandleA(nullptr);
    const auto res = MainLoop();
    ::ExitProcess(res);
}
