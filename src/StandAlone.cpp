/*
*      Copyright (C) 2005-2015 Team Kodi
*      http://kodi.tv
*
*  This Program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This Program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with XBMC; see the file COPYING.  If not, see
*  <http://www.gnu.org/licenses/>.
*
*/

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <malloc.h>
#include <crtdbg.h>

#include <windows.h>
#include "vis_milk2/plugin.h"
#include <math.h>
#include <d3d11_1.h>

HWND gHWND = NULL;
ID3D11Device*           pD3DDevice = nullptr;
ID3D11DeviceContext*    pImmediateContext = nullptr;
IDXGISwapChain*         pSwapChain = nullptr;
ID3D11RenderTargetView* pRenderTargetView = nullptr;
ID3D11DepthStencilView* pDepthStencilView = nullptr;
D3D_FEATURE_LEVEL       featureLevel = D3D_FEATURE_LEVEL_11_0;

CPlugin g_plugin;

HRESULT CreateDevice(int iWidth, int iHeight)
{
  HRESULT hr = S_OK;

  UINT createDeviceFlags = 0;
#ifdef _DEBUG
  createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  D3D_FEATURE_LEVEL featureLevels[] =
  {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1,
  };

  hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels),
                         D3D11_SDK_VERSION, &pD3DDevice, &featureLevel, &pImmediateContext);

  if (hr == E_INVALIDARG)
  {
    // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, &featureLevels[1], ARRAYSIZE(featureLevels) - 1,
                           D3D11_SDK_VERSION, &pD3DDevice, &featureLevel, &pImmediateContext);
  }

  if (FAILED(hr))
    return hr;

  // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
  IDXGIFactory1* dxgiFactory = nullptr;
  {
    IDXGIDevice* dxgiDevice = nullptr;
    hr = pD3DDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (SUCCEEDED(hr))
    {
      IDXGIAdapter* adapter = nullptr;
      hr = dxgiDevice->GetAdapter(&adapter);
      if (SUCCEEDED(hr))
      {
        hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
        adapter->Release();
      }
      dxgiDevice->Release();
    }
  }

  if (FAILED(hr))
    return hr;

  // Create swap chain
  IDXGIFactory2* dxgiFactory2 = nullptr;
  hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
  if (dxgiFactory2)
  {
    // DirectX 11.1 or later
    DXGI_SWAP_CHAIN_DESC1 sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.Width = iWidth;
    sd.Height = iHeight;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    IDXGISwapChain1* pSwapChain1 = nullptr;
    hr = dxgiFactory2->CreateSwapChainForHwnd(pD3DDevice, gHWND, &sd, nullptr, nullptr, &pSwapChain1);
    if (SUCCEEDED(hr))
    {
      hr = pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&pSwapChain));
      pSwapChain1->Release();
    }

    dxgiFactory2->Release();
  }
  else
  {
    // DirectX 11.0 systems
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = iWidth;
    sd.BufferDesc.Height = iHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    sd.OutputWindow = gHWND;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    hr = dxgiFactory->CreateSwapChain(pD3DDevice, &sd, &pSwapChain);
  }

  dxgiFactory->MakeWindowAssociation(gHWND, DXGI_MWA_NO_ALT_ENTER);
  dxgiFactory->Release();

  if (FAILED(hr))
    return hr;

  // Create a render target view
  ID3D11Texture2D* pBackBuffer = nullptr;
  hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
  if (FAILED(hr))
    return hr;

  hr = pD3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTargetView);
  pBackBuffer->Release();
  if (FAILED(hr))
    return hr;

  // Create depth stencil texture
  D3D11_TEXTURE2D_DESC descDepth;
  ZeroMemory(&descDepth, sizeof(descDepth));
  descDepth.Width = iWidth;
  descDepth.Height = iHeight;
  descDepth.MipLevels = 1;
  descDepth.ArraySize = 1;
  descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  descDepth.SampleDesc.Count = 1;
  descDepth.SampleDesc.Quality = 0;
  descDepth.Usage = D3D11_USAGE_DEFAULT;
  descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  descDepth.CPUAccessFlags = 0;
  descDepth.MiscFlags = 0;

  ID3D11Texture2D* pDepthStencil;
  hr = pD3DDevice->CreateTexture2D(&descDepth, nullptr, &pDepthStencil);
  if (FAILED(hr))
    return hr;

  // Create the depth stencil view
  D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
  ZeroMemory(&descDSV, sizeof(descDSV));
  descDSV.Format = descDepth.Format;
  descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  descDSV.Texture2D.MipSlice = 0;
  hr = pD3DDevice->CreateDepthStencilView(pDepthStencil, &descDSV, &pDepthStencilView);

  pDepthStencil->Release();

  if (FAILED(hr))
    return hr;

  pImmediateContext->OMSetRenderTargets(1, &pRenderTargetView, pDepthStencilView);

  // Setup the viewport
  CD3D11_VIEWPORT vp(0.0f, 0.0f, (float)iWidth, (float)iHeight);
  pImmediateContext->RSSetViewports(1, &vp);

  return hr;
}

LRESULT CALLBACK StaticWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
  switch( uMsg )
  {
  case WM_CLOSE:
    {
      HMENU hMenu;
      hMenu = GetMenu( hWnd );
      if( hMenu != NULL )
        DestroyMenu( hMenu );
      DestroyWindow( hWnd );
      UnregisterClass( "Direct3DWindowClass", NULL );
      return 0;
    }

  case WM_DESTROY:
    PostQuitMessage( 0 );
    break;
  }

  return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

float sin1add = 0.05f;
float sin2add = 0.08f;
void RenderFrame()
{
  float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  pImmediateContext->ClearRenderTargetView(pRenderTargetView, color);

  float waves[576*2];
  static float sin1 = 0;
  static float sin2 = 0;

//  sin1 += 10;
//  sin2 += 20;

  float sin1start = sin1;
  float sin2start = sin2;

  float Current = 0;
  for ( int i=0; i < 576; i++)
  {
//    if ( ( rand() % 10) > 4)
//      iCurrent += (short)(rand() % (255));
//    else
//      iCurrent -= (short)(rand() % (255));
    Current = sinf(sin1+sin2);
//    Current += sinf(sin2);
    sin1 += sin1add;
    sin2 += sin2add;
    waves[i*2+0] = Current*0.2f;
    waves[i*2+1] = Current*0.2f;
//    waves[0][i] = (rand() % 128 ) / 128.0f;//iCurrent;//iCurrent;
  //  waves[1][i] = (rand() % 128 ) / 128.0f;//iCurrent;//iCurrent;
  }
  sin1 = sin1start + sin1add;
  sin2 = sin2start + sin2add*7;

  g_plugin.PluginRender((unsigned char*) &waves[0], (unsigned char*)&waves[1] );

  pSwapChain->Present(1, 0);
}

void MainLoop()
{
  bool bGotMsg;
  MSG msg;
  msg.message = WM_NULL;
  PeekMessage( &msg, NULL, 0U, 0U, PM_NOREMOVE );

  while( WM_QUIT != msg.message )
  {
    // Use PeekMessage() so we can use idle time to render the scene. 
    bGotMsg = ( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) != 0 );

    if( bGotMsg )
    {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
    }
    else
    {
      // Render a frame during idle time (no messages are waiting)
      RenderFrame();
    }
  }
}


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
  _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
  _CrtSetBreakAlloc(60);

  // Register the windows class
  WNDCLASS wndClass;
  wndClass.style = CS_DBLCLKS;
  wndClass.lpfnWndProc = StaticWndProc;
  wndClass.cbClsExtra = 0;
  wndClass.cbWndExtra = 0;
  wndClass.hInstance = hInstance;
  wndClass.hIcon = NULL;
  wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
  wndClass.hbrBackground = ( HBRUSH )GetStockObject( BLACK_BRUSH );
  wndClass.lpszMenuName = NULL;
  wndClass.lpszClassName = "Direct3DWindowClass";

  if( !RegisterClass( &wndClass ) )
  {
    DWORD dwError = GetLastError();
    if( dwError != ERROR_CLASS_ALREADY_EXISTS )
      return -1;
  }

  // Find the window's initial size, but it might be changed later
  int nDefaultWidth = 1280;
  int nDefaultHeight = 720;

  RECT rc;
  SetRect( &rc, 0, 0, nDefaultWidth, nDefaultHeight );
  AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, false );

  // Create the render window
  HWND hWnd = CreateWindow( "Direct3DWindowClass", "MD2", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, ( rc.right - rc.left ), ( rc.bottom - rc.top ), 0,
    NULL, hInstance, 0 );
  if( hWnd == NULL )
  {
    DWORD dwError = GetLastError();
    return -1;
  }
  gHWND = hWnd;

  ShowWindow( hWnd, SW_SHOW );

  if (S_OK != CreateDevice( nDefaultWidth, nDefaultHeight ))
    return -1;

  BOOL bSuccess;
  bSuccess = g_plugin.PluginPreInitialize(nullptr, nullptr);
  if (!bSuccess)
    return -1;
  bSuccess = g_plugin.PluginInitialize( pImmediateContext, 0, 0, nDefaultWidth, nDefaultHeight, nDefaultHeight / (float)nDefaultWidth);
  if (!bSuccess)
    return -1;

  MainLoop();

  g_plugin.PluginQuit();

  pImmediateContext->Flush();
  pImmediateContext->ClearState();

  pRenderTargetView->Release();
  pDepthStencilView->Release();
  pSwapChain->Release();
  pImmediateContext->Release();
  pD3DDevice->Release();

  return 0;
}


struct _DEBUG_STATE
  {
  _DEBUG_STATE() {}
  ~_DEBUG_STATE() { _CrtDumpMemoryLeaks(); }
  };

#pragma init_seg(compiler)
_DEBUG_STATE ds;
