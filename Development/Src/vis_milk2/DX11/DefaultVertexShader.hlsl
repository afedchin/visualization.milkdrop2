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

cbuffer cbTransforms : register(b0)
{
  float4x4 g_world;
  float4x4 g_view;
  float4x4 g_proj;
};

struct VS_INPUT
{
  float4 vPosition : POSITION;
  float4 Diffuse   : COLOR;
  float4 Tex0      : TEXCOORD0;
  float2 Tex1      : TEXCOORD1;
};

struct VS_OUTPUT
{
  float4 Diffuse   : COLOR;
  float4 Tex0      : TEXCOORD0;
  float2 Tex1      : TEXCOORD1;
  float4 vPosition : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT In)
{
  VS_OUTPUT output = (VS_OUTPUT)0.0;

  float4 worldPos = mul(In.vPosition, g_world);
  float4 cameraPos = mul(worldPos, g_view); 
  output.vPosition = mul(cameraPos, g_proj);
  output.Diffuse = saturate(In.Diffuse);
  output.Tex0 = In.Tex0;

  return output;
}