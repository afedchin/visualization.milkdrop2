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
#include <d3d9.h>
#include "../../vis_milk2/vis_milk2/plugin.h"
#include <math.h>

IDirect3D9*	pD3D9 = NULL;
IDirect3DDevice9* pD3DDevice = NULL;
HWND gHWND = NULL;

CPlugin g_plugin;

void CreateDevice( int iWidth, int iHeight )
{
	pD3D9 = Direct3DCreate9( D3D_SDK_VERSION );
	D3DDISPLAYMODE mode;
	pD3D9->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &mode );

//  	iWidth = mode.Width;
//  	iHeight = mode.Height;
//  	bool bWindowed = TRUE;

	D3DPRESENT_PARAMETERS PresentParameters;
	memset( &PresentParameters, 0, sizeof( PresentParameters ) );

	PresentParameters.BackBufferCount			=	1;
	PresentParameters.BackBufferFormat			=	mode.Format;
	PresentParameters.BackBufferWidth			=	iWidth;
	PresentParameters.BackBufferHeight			=	iHeight;
	PresentParameters.SwapEffect				=	D3DSWAPEFFECT_COPY;
	PresentParameters.Flags						=	0;
	PresentParameters.EnableAutoDepthStencil	=	TRUE;
	PresentParameters.AutoDepthStencilFormat	=	D3DFMT_D24X8;
	PresentParameters.Windowed					=	TRUE;
	PresentParameters.PresentationInterval		=	D3DPRESENT_INTERVAL_ONE;
	PresentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
	PresentParameters.hDeviceWindow				=	( HWND )gHWND;

	pD3D9->CreateDevice( D3DADAPTER_DEFAULT,
						 D3DDEVTYPE_HAL,
						 ( HWND )gHWND,
						 D3DCREATE_HARDWARE_VERTEXPROCESSING,
						 &PresentParameters,
						 &pD3DDevice );
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

	pD3DDevice->BeginScene();

	float waves[576*2];
	static float sin1 = 0;
	static float sin2 = 0;
	
//	sin1 += 10;
//	sin2 += 20;

	float sin1start = sin1;
	float sin2start = sin2;

	float Current = 0;
	for ( int i=0; i < 576; i++)
	{
// 		if ( ( rand() % 10) > 4)
// 			iCurrent += (short)(rand() % (255));
// 		else
// 			iCurrent -= (short)(rand() % (255));
		Current = sinf(sin1+sin2);
//		Current += sinf(sin2);
		sin1 += sin1add;
		sin2 += sin2add;
		waves[i*2+0] = Current*0.2f;
		waves[i*2+1] = Current*0.2f;
//		waves[0][i] = (rand() % 128 ) / 128.0f;//iCurrent;//iCurrent;
	//	waves[1][i] = (rand() % 128 ) / 128.0f;//iCurrent;//iCurrent;
	}
	sin1 = sin1start + sin1add;
	sin2 = sin2start + sin2add*7;

	g_plugin.PluginRender((unsigned char*) &waves[0], (unsigned char*)&waves[1] );

	pD3DDevice->EndScene();

	pD3DDevice->Present( NULL, NULL, 0, NULL );
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

	CreateDevice( nDefaultWidth, nDefaultHeight );

	g_plugin.PluginPreInitialize(0,0);
	g_plugin.PluginInitialize( pD3DDevice, 0, 0, nDefaultWidth, nDefaultHeight, nDefaultHeight / (float)nDefaultWidth);
	
	MainLoop();

	g_plugin.PluginQuit();

	pD3DDevice->Release();
	pD3D9->Release();

	return 0;
}


struct _DEBUG_STATE
  {
  _DEBUG_STATE() {}
  ~_DEBUG_STATE() { _CrtDumpMemoryLeaks(); }
  };

#pragma init_seg(compiler)
_DEBUG_STATE ds;