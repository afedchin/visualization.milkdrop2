/*
*  Copyright © 2010-2015 Team Kodi
*  http://kodi.tv
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include "ConstantTable.h"

using namespace DirectX;

bool ShaderConstantBuffer::Create(ID3D11Device* pDevice)
{
  CD3D11_BUFFER_DESC desc(Description.Size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
  if (FAILED(pDevice->CreateBuffer(&desc, nullptr, &Data)))
    return false;

  return true;
}

bool ShaderConstantBuffer::HasChanges()
{
  for (auto var : Variables)
  {
    if (var.IsDirty)
      return true;
  }
  return false;
}

CConstantTable::CConstantTable(ID3D11ShaderReflection* pReflection) 
  : m_iRefCount(1), m_pReflection(pReflection)
{
}

CConstantTable::~CConstantTable()
{
  if (m_pReflection)
  {
    m_pReflection->Release();
    m_pReflection = nullptr;
  }

  for (size_t i = 0; i < m_ConstantBuffers.size(); i++)
  {
    SafeRelease(m_ConstantBuffers[i].Data);
    for (size_t j = 0; j < m_ConstantBuffers[i].Variables.size(); j++)
      delete[] m_ConstantBuffers[i].Variables[j].Value;
  }
  m_ConstantBuffers.clear();
}

bool CConstantTable::GrabShaderData(ID3D11Device* pDevice)
{
  if (m_pReflection == nullptr)
    return false;

  m_pReflection->GetDesc(&ShaderDesc);
  m_pReflection->GetMinFeatureLevel(&MinimumFeatureLevel);

  for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
  {
    ShaderConstantBuffer constantBuffer;
    ID3D11ShaderReflectionConstantBuffer* buffer = m_pReflection->GetConstantBufferByIndex(i);

    if (FAILED(buffer->GetDesc(&constantBuffer.Description)))
      return false;

    for (UINT v = 0; v < constantBuffer.Description.Variables; ++v)
    {
      ShaderVariable shaderVariable;

      ID3D11ShaderReflectionVariable* variable = buffer->GetVariableByIndex(v);
      if (FAILED(variable->GetDesc(&shaderVariable.Description)))
        return false;

      ID3D11ShaderReflectionType* type = variable->GetType();
      if (FAILED(type->GetDesc(&shaderVariable.Type)))
        return false;

      constantBuffer.Variables.push_back(shaderVariable);
    }

    if (!constantBuffer.Create(pDevice))
      return false;

    m_ConstantBuffers.push_back(constantBuffer);
  }

  for (size_t i = 0; i < ShaderDesc.BoundResources; i++)
  {
    ShaderBinding shaderBinding;
    m_pReflection->GetResourceBindingDesc(i, &shaderBinding.Description);
    m_Bindings.push_back(shaderBinding);
  }


  return true;
}

int CConstantTable::GetVariablesCount()
{
  int total = 0;
  for (size_t i = 0; i < m_ConstantBuffers.size(); i++)
    total += m_ConstantBuffers[i].Variables.size();

  return total;
}

void CConstantTable::GetBuffers(ID3D11Buffer** ppBuffers)
{
  for (size_t i = 0; i < m_ConstantBuffers.size(); ++i)
  {
    ppBuffers[i] = m_ConstantBuffers[i].Data;
  }
}

int CConstantTable::GetTextureSlot(std::string& strName)
{
  for (auto binding : m_Bindings)
  {
    if ( binding.Description.Type == D3D_SIT_TEXTURE
      && binding.Description.Name == strName)
      return binding.Description.BindPoint;
  }

  return -1;
}

ShaderVariable* CConstantTable::GetVariableByName(std::string& strName)
{
  for (size_t i = 0; i < m_ConstantBuffers.size(); i++)
  {
    for (size_t j = 0; j < m_ConstantBuffers[i].Variables.size(); j++)
    {
      if (m_ConstantBuffers[i].Variables[j].Description.Name == strName)
        return &m_ConstantBuffers[i].Variables[j];
    }
  }
  return nullptr;
}

bool CConstantTable::SetVector(LPCSTR handle, XMFLOAT4* vector)
{
  std::string strName(handle);
  ShaderVariable* variable = GetVariableByName(strName);
  if (variable && variable->Type.Class == D3D_SVC_VECTOR)
  {
    if (!variable->Value)
      variable->Value = new float[4];
    memcpy(variable->Value, vector, variable->Description.Size);
    variable->IsDirty = true;

    return true;
  }

  return false;
}

bool CConstantTable::SetMatrix(LPCSTR handle, XMMATRIX* matrix)
{
  std::string strName(handle);
  ShaderVariable* variable = GetVariableByName(strName);
  if (variable && variable->Type.Class == D3D_SVC_MATRIX_COLUMNS)
  {
    XMMATRIX colums = XMMatrixTranspose(*matrix);
    XMFLOAT4X3 floats;
    XMStoreFloat4x3(&floats, colums);

    if (!variable->Value)
      variable->Value = new float[4][3];
    memcpy(variable->Value, floats.m, variable->Description.Size);
    variable->IsDirty = true;

    return true;
  }

  return false;
}

bool CConstantTable::ApplyChanges(ID3D11DeviceContext* pContext)
{
  for (size_t i = 0; i < m_ConstantBuffers.size(); i++)
  {
    if (!m_ConstantBuffers[i].HasChanges())
      continue;

    D3D11_MAPPED_SUBRESOURCE res;
    if (S_OK != pContext->Map(m_ConstantBuffers[i].Data, 0, D3D11_MAP_WRITE_DISCARD, 0, &res))
      continue;

    for (size_t j = 0; j < m_ConstantBuffers[i].Variables.size(); j++)
    {
      ShaderVariable* var = &m_ConstantBuffers[i].Variables[j];
      if (var->IsDirty && var->Value)
      {
        memcpy(static_cast<unsigned char*>(res.pData) + var->Description.StartOffset, var->Value, var->Description.Size);
        var->IsDirty = false;
      }
    }

    pContext->Unmap(m_ConstantBuffers[i].Data, 0);
  }
  return false;
}

ShaderVariable* CConstantTable::GetVariableByIndex(UINT index)
{
  for (size_t i = 0; i < m_ConstantBuffers.size(); i++)
  {
    if (index >= m_ConstantBuffers[i].Variables.size())
    {
      index -= m_ConstantBuffers[i].Variables.size();
      continue;
    }
    return &m_ConstantBuffers[i].Variables[index];
  }
  return nullptr;
}

ShaderBinding* CConstantTable::GetBindingByIndex(UINT index)
{
  if (index < m_Bindings.size())
    return &m_Bindings[index];

  return nullptr;
}
