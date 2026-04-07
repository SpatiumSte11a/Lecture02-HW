#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ============================================================
// Video / Window Config
// ============================================================
struct VideoConfig
{
    int Width = 800;
    int Height = 600;
    bool IsFullscreen = false;
    bool NeedsResize = false;
    int VSync = 1;
} g_Config;

// fixed-size window for this homework
DWORD g_WindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

// ============================================================
// DirectX Global Variables
// ============================================================
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;

// ============================================================
// Triangle Data
// ============================================================
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

// small red demo triangle for the homework scene
Vertex g_Vertices[] =
{
    {  0.0f,  0.12f, 0.5f, 1.0f, 0.2f, 0.2f, 1.0f },
    {  0.10f, -0.10f, 0.5f, 1.0f, 0.2f, 0.2f, 1.0f },
    { -0.10f, -0.10f, 0.5f, 1.0f, 0.2f, 0.2f, 1.0f }
};

// simple passthrough shader
const char* g_ShaderSource = R"(
struct VS_INPUT
{
    float3 pos : POSITION;
    float4 col : COLOR;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    return input.col;
}
)";

// ============================================================
// Video Resource Rebuild
// ============================================================
void RebuildVideoResources(HWND hWnd)
{
    if (!g_pSwapChain) return;

    // release old render target before ResizeBuffers
    if (g_pRenderTargetView)
    {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }

    HRESULT hr = g_pSwapChain->ResizeBuffers(
        0,
        g_Config.Width,
        g_Config.Height,
        DXGI_FORMAT_UNKNOWN,
        0
    );

    if (FAILED(hr))
    {
        printf("ResizeBuffers failed\n");
        return;
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr) || pBackBuffer == nullptr)
    {
        printf("GetBuffer failed\n");
        return;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    if (FAILED(hr))
    {
        printf("CreateRenderTargetView failed\n");
        return;
    }

    // keep the window size fixed in windowed mode
    if (!g_Config.IsFullscreen)
    {
        RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
        AdjustWindowRect(&rc, g_WindowStyle, FALSE);

        SetWindowPos(
            hWnd,
            nullptr,
            0, 0,
            rc.right - rc.left,
            rc.bottom - rc.top,
            SWP_NOMOVE | SWP_NOZORDER
        );
    }

    g_Config.NeedsResize = false;
}

// ============================================================
// Shader / Buffer Setup
// ============================================================
bool CreateTriangleResources()
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    HRESULT hr = D3DCompile(
        g_ShaderSource,
        strlen(g_ShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "VS",
        "vs_4_0",
        0,
        0,
        &vsBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            printf("VS Compile Error: %s\n", (char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    hr = D3DCompile(
        g_ShaderSource,
        strlen(g_ShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "PS",
        "ps_4_0",
        0,
        0,
        &psBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            printf("PS Compile Error: %s\n", (char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        if (vsBlob) vsBlob->Release();
        return false;
    }

    hr = g_pd3dDevice->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &g_pVertexShader
    );
    if (FAILED(hr))
    {
        printf("CreateVertexShader failed\n");
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    hr = g_pd3dDevice->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &g_pPixelShader
    );
    if (FAILED(hr))
    {
        printf("CreatePixelShader failed\n");
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_pd3dDevice->CreateInputLayout(
        layout,
        2,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &g_pInputLayout
    );

    vsBlob->Release();
    psBlob->Release();

    if (FAILED(hr))
    {
        printf("CreateInputLayout failed\n");
        return false;
    }

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(g_Vertices);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = g_Vertices;

    hr = g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);
    if (FAILED(hr))
    {
        printf("CreateBuffer failed\n");
        return false;
    }

    return true;
}

// ============================================================
// Window Procedure
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// ============================================================
// Entry Point
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // ------------------------------------------------------------
    // 1. Register and create window
    // ------------------------------------------------------------
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DX11VideoClass";
    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, g_WindowStyle, FALSE);

    HWND hWnd = CreateWindowW(
        L"DX11VideoClass",
        L"Lecture04-HW",
        g_WindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd)
    {
        printf("CreateWindow failed\n");
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);

    // ------------------------------------------------------------
    // 2. Create DX11 Device and SwapChain
    // ------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = g_Config.Width;
    sd.BufferDesc.Height = g_Config.Height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        nullptr,
        &g_pImmediateContext
    );

    if (FAILED(hr))
    {
        printf("D3D11CreateDeviceAndSwapChain failed\n");
        return 0;
    }

    // build the first render target
    RebuildVideoResources(hWnd);

    // ------------------------------------------------------------
    // 3. Create triangle resources
    // ------------------------------------------------------------
    if (!CreateTriangleResources())
    {
        printf("Triangle resource creation failed\n");
        return 0;
    }

    // ------------------------------------------------------------
    // 4. Game Loop
    // ------------------------------------------------------------
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // check ESC every frame
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
            }

            // toggle fullscreen
            if (GetAsyncKeyState('F') & 0x0001)
            {
                g_Config.IsFullscreen = !g_Config.IsFullscreen;
                g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
                g_Config.NeedsResize = true;
            }

            // rebuild render target after fullscreen change
            if (g_Config.NeedsResize)
            {
                RebuildVideoResources(hWnd);
            }

            // ----------------------------------------------------
            // rendering
            // ----------------------------------------------------
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };

            D3D11_VIEWPORT vp = {};
            vp.TopLeftX = 0.0f;
            vp.TopLeftY = 0.0f;
            vp.Width = (float)g_Config.Width;
            vp.Height = (float)g_Config.Height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;

            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            // static triangle draw path
            UINT stride = sizeof(Vertex);
            UINT offset = 0;

            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetInputLayout(g_pInputLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

            g_pImmediateContext->Draw(3, 0);

            g_pSwapChain->Present(g_Config.VSync, 0);
        }
    }

    // ============================================================
    // Cleanup
    // ============================================================
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return (int)msg.wParam;
}