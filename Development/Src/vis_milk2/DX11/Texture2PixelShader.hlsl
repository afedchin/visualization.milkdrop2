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
Texture2D    g_Texture0 : register(t0);
SamplerState g_Sampler0 : register(s0);

struct PS_INPUT
{
  float4 Diffuse   : COLOR;
  float4 Tex0      : TEXCOORD0;
  float2 Tex1      : TEXCOORD1;
};

float4 main(PS_INPUT In) : SV_TARGET
{
  return g_Texture0.Sample(g_Sampler0, In.Tex0.xy) * In.Diffuse;
}