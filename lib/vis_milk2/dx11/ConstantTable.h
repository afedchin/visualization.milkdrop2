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

#pragma once

#include <d3d11_1.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../vis_milk2/utility.h"

using namespace DirectX;

struct ShaderVariable
{
  D3D11_SHADER_VARIABLE_DESC Description;
  D3D11_SHADER_TYPE_DESC Type;
  bool  IsDirty;
  void* Value;

  ShaderVariable() : Value(nullptr), IsDirty(false)
  {
  }
};

struct ShaderBinding
{
  D3D11_SHADER_INPUT_BIND_DESC Description;
};

struct ShaderConstantBuffer
{
  ID3D11Buffer* Data = nullptr;
  D3D11_SHADER_BUFFER_DESC Description;
  std::vector<ShaderVariable> Variables;

  bool Create(ID3D11Device* pDevice);
  bool HasChanges();

  ShaderConstantBuffer() : Data(nullptr)
  {
  }
  ~ShaderConstantBuffer()
  {
    Variables.clear();
  }
};


class CConstantTable
{
public:
  CConstantTable(ID3D11ShaderReflection* pReflection);
  ~CConstantTable();

  void AddRef()
  {
    m_iRefCount++;
  }

  void Release()
  {
    if (--m_iRefCount == 0)
      delete this;
  }

  bool GrabShaderData(ID3D11Device* pDevice);
  size_t  GetVariablesCount();
  size_t  GetBuffersCount() { return m_ConstantBuffers.size(); }
  void GetBuffers(ID3D11Buffer** ppBuffers);;
  int  GetTextureSlot(std::string& strName);

  bool SetVector(LPCSTR handle, XMFLOAT4* vector);
  bool SetMatrix(LPCSTR handle, XMMATRIX* matrix);
  bool ApplyChanges(ID3D11DeviceContext* pContext);

  ShaderVariable* GetVariableByIndex(size_t index);
  ShaderVariable* GetVariableByName(std::string& strName);
  ShaderBinding*  GetBindingByIndex(size_t index);

  D3D_FEATURE_LEVEL       MinimumFeatureLevel;
  D3D11_SHADER_DESC       ShaderDesc;

private:
  int m_iRefCount;
  ID3D11ShaderReflection* m_pReflection = NULL;
  std::vector<ShaderConstantBuffer> m_ConstantBuffers;
  std::vector<ShaderBinding>        m_Bindings;
};