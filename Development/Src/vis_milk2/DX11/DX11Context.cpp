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

#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "DDSTextureLoader.h"
#include "DX11Context.h"
#include <vector>
#include "WICTextureLoader.h"
#include <wrl.h>
#include "../vis_milk2/support.h"
#include "../vis_milk2/utility.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace
{
  #include "../ColorPixelShader.inc"
  #include "../DefaultVertexShader.inc"
  #include "../DiffusePixelShader.inc"
  #include "../TexturePixelShader.inc"
  #include "../Texture2PixelShader.inc"
}

DX11Context::DX11Context(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
  : m_pDevice(pDevice), 
    m_pContext(pContext),
    m_pImmContext(NULL),
    m_pVShader(NULL),
    m_pInputLayout(NULL),
    m_pVBuffer(NULL),
    m_pIBuffer(NULL),
    m_pIFanBuffer(NULL),
    m_pCBuffer(NULL),
    m_bCBufferIsDirty(false),
    m_uCurrShader(0),
    m_pState(NULL)
{
  for (size_t i = 0; i < MAX_NUM_SHADERS; i++)
    m_pPShader[i] = NULL;
}

DX11Context::~DX11Context()
{
  SafeRelease(m_pVShader);
  for (size_t i = 0; i < MAX_NUM_SHADERS; i++)
    SafeRelease(m_pPShader[i]);
  SafeRelease(m_pInputLayout);
  SafeRelease(m_pVBuffer);
  SafeRelease(m_pIBuffer);
  SafeRelease(m_pIFanBuffer);
  SafeRelease(m_pCBuffer);
  SafeRelease(m_pState);
  SafeRelease(m_pImmContext);
  m_states.reset(NULL);
}

void DX11Context::Initialize()
{
  m_states.reset(new CommonStates(m_pDevice));

  D3D11_INPUT_ELEMENT_DESC layout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };

  m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), DefaultVertexShaderCode, sizeof(DefaultVertexShaderCode), &m_pInputLayout);

  m_pDevice->CreateVertexShader(DefaultVertexShaderCode, sizeof(DefaultVertexShaderCode), NULL, &m_pVShader);

  m_pDevice->CreatePixelShader(DiffusePixelShaderCode,  sizeof(DiffusePixelShaderCode),  NULL, &m_pPShader[0]);
  m_pDevice->CreatePixelShader(TexturePixelShaderCode,  sizeof(TexturePixelShaderCode),  NULL, &m_pPShader[1]);
  m_pDevice->CreatePixelShader(ColorPixelShaderCode,    sizeof(ColorPixelShaderCode),    NULL, &m_pPShader[2]);
  m_pDevice->CreatePixelShader(Texture2PixelShaderCode, sizeof(Texture2PixelShaderCode), NULL, &m_pPShader[3]);

  CD3D11_BUFFER_DESC bDesc(sizeof(MYVERTEX) * MAX_VERTICES_COUNT,
                           D3D11_BIND_VERTEX_BUFFER, 
                           D3D11_USAGE_DYNAMIC, 
                           D3D11_CPU_ACCESS_WRITE );
  m_pDevice->CreateBuffer(&bDesc, NULL, &m_pVBuffer);

  bDesc.ByteWidth = sizeof(uint16_t) * MAX_VERTICES_COUNT / 6;
  bDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

  m_pDevice->CreateBuffer(&bDesc, NULL, &m_pIBuffer);

  std::vector<uint16_t> indicies;
  for (size_t i = 1; i < MAX_VERTICES_COUNT / 6 - 2; ++i)
  {
    indicies.push_back(0);
    indicies.push_back(i);
    indicies.push_back(i + 1);
  }
  bDesc.ByteWidth = sizeof(uint16_t) * indicies.size();
  bDesc.Usage = D3D11_USAGE_IMMUTABLE;
  bDesc.CPUAccessFlags = 0;
  D3D11_SUBRESOURCE_DATA bData = { indicies.data() };

  m_pDevice->CreateBuffer(&bDesc, &bData, &m_pIFanBuffer);

  XMStoreFloat4x4(&m_transforms.world, XMMatrixTranspose(XMMatrixIdentity()));
  XMStoreFloat4x4(&m_transforms.view, XMMatrixTranspose(XMMatrixIdentity()));
  XMStoreFloat4x4(&m_transforms.proj, XMMatrixTranspose(XMMatrixIdentity()));

  bDesc.ByteWidth = sizeof(cbTransforms);
  bDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bDesc.Usage = D3D11_USAGE_DYNAMIC;
  bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  bData.pSysMem = &m_transforms;

  m_pDevice->CreateBuffer(&bDesc, &bData, &m_pCBuffer);

  m_pDevice->GetImmediateContext(&m_pImmContext);
}

bool DX11Context::CreateTexture(unsigned int uWidth, unsigned int uHeight, unsigned int mipLevels, UINT bindFlags, DXGI_FORMAT format, ID3D11Texture2D** ppTexture, 
                                unsigned int miscFlags /*= 0*/, D3D11_USAGE usage /*=D3D11_USAGE_DEFAULT*/)
{
  CD3D11_TEXTURE2D_DESC texDesc(format, uWidth, uHeight, 1, mipLevels, bindFlags, usage);
  texDesc.MiscFlags = miscFlags;
  if (usage == D3D11_USAGE_DYNAMIC)
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (usage == D3D11_USAGE_STAGING)
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE/* | D3D11_CPU_ACCESS_READ*/;

  return S_OK == m_pDevice->CreateTexture2D(&texDesc, NULL, ppTexture);
}

bool DX11Context::CreateVolumeTexture(unsigned int uWidth, unsigned int uHeight, unsigned int uDepth, unsigned int mipLevels, UINT bindFlags, DXGI_FORMAT format, ID3D11Texture3D** ppTexture,
                                      unsigned int miscFlags /*= 0*/, D3D11_USAGE usage /*= D3D11_USAGE_DEFAULT*/)
{
  CD3D11_TEXTURE3D_DESC texDesc(format, uWidth, uHeight, uDepth, mipLevels, bindFlags, usage);
  texDesc.MiscFlags = miscFlags;
  if (usage == D3D11_USAGE_DYNAMIC)
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  if (usage == D3D11_USAGE_STAGING)
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

  return S_OK == m_pDevice->CreateTexture3D(&texDesc, NULL, ppTexture);
}

void DX11Context::DrawPrimitive(unsigned int primType, unsigned int iPrimCount, const void * pVData, unsigned int vertexStride)
{
  if (m_bCBufferIsDirty)
  {
    D3D11_MAPPED_SUBRESOURCE res;
    if (S_OK == m_pContext->Map(m_pCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res))
    {
      memcpy(res.pData, &m_transforms, sizeof(cbTransforms));
      m_pContext->Unmap(m_pCBuffer, 0);
    }
    m_bCBufferIsDirty = false;
  }

  int numVerts = NumVertsFromType(primType, iPrimCount);
  UpdateVBuffer(numVerts, pVData, vertexStride);

  unsigned int offsets = 0;
  m_pContext->IASetVertexBuffers(0, 1, &m_pVBuffer, &vertexStride, &offsets);
  m_pContext->IASetInputLayout(m_pInputLayout);
  if (primType == D3DPT_TRIANGLEFAN)
  {
    m_pContext->IASetIndexBuffer(m_pIFanBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pContext->DrawIndexed(iPrimCount * 3, 0, 0);
  }
  else
  {
    m_pContext->IASetPrimitiveTopology((D3D_PRIMITIVE_TOPOLOGY)primType);
    m_pContext->Draw(numVerts, 0);
  }
}

void DX11Context::DrawIndexedPrimitive(unsigned int primType, unsigned int uStartVertex, unsigned int iNumVertices, 
                                       unsigned int iPrimCount, const void* pIData, const void* pVData, unsigned int vertexStride)
{
  if (m_bCBufferIsDirty)
  {
    D3D11_MAPPED_SUBRESOURCE res;
    if (S_OK == m_pContext->Map(m_pCBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res))
    {
      memcpy(res.pData, &m_transforms, sizeof(cbTransforms));
      m_pContext->Unmap(m_pCBuffer, 0);
    }
    m_bCBufferIsDirty = false;
  }

  int numIndices = NumVertsFromType(primType, iPrimCount);
  UpdateVBuffer(iNumVertices, pVData, vertexStride);
  UpdateIBuffer(numIndices, pIData);

  unsigned int offsets = 0;
  m_pContext->IASetVertexBuffers(0, 1, &m_pVBuffer, &vertexStride, &offsets);
  m_pContext->IASetIndexBuffer(m_pIBuffer, DXGI_FORMAT_R16_UINT, 0);
  m_pContext->IASetInputLayout(m_pInputLayout);
  m_pContext->IASetPrimitiveTopology((D3D_PRIMITIVE_TOPOLOGY)primType);
  m_pContext->DrawIndexed(numIndices, 0, uStartVertex);
}

void DX11Context::GetRenderTarget(ID3D11Texture2D** ppTexture)
{
  ID3D11RenderTargetView* pRTView = NULL;
  ID3D11Resource* pResource = NULL;

  m_pContext->OMGetRenderTargets(1, &pRTView, NULL);
  if (pRTView)
  {
    pRTView->GetResource(&pResource);
    pResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(ppTexture));
    pResource->Release();
    pRTView->Release();
  }
}

void DX11Context::GetViewport(D3D11_VIEWPORT *vp)
{
  unsigned int numVP = 1;
  m_pContext->RSGetViewports(&numVP, vp);
}

void DX11Context::SetBlendState(bool bEnable, D3D11_BLEND srcBlend, D3D11_BLEND destBlend)
{
  if (m_pState)
  {
    m_pState->Release();
    m_pState = NULL;
  }

  D3D11_BLEND_DESC desc;
  ZeroMemory(&desc, sizeof(desc));

  //desc.RenderTarget[0].BlendEnable = (srcBlend  != D3D11_BLEND_ONE)
  //                                || (destBlend != D3D11_BLEND_ZERO);

  desc.RenderTarget[0].BlendEnable = bEnable;
  desc.RenderTarget[0].SrcBlend = srcBlend;
  desc.RenderTarget[0].DestBlend = destBlend;
  desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

  D3D11_BLEND srcAlphaBlend = srcBlend;
  D3D11_BLEND destBAlhalend = destBlend;

  if (srcBlend == 3 || srcBlend == 4)
    srcAlphaBlend = (D3D11_BLEND)(srcAlphaBlend + 2);
  if (srcBlend == 9 || srcBlend == 10)
    srcAlphaBlend = (D3D11_BLEND)(srcAlphaBlend - 2);
  if (destBlend == 3 || destBlend == 4)
    destBAlhalend = (D3D11_BLEND)(destBAlhalend + 2);
  if (destBlend == 9 || destBlend == 10)
    destBAlhalend = (D3D11_BLEND)(destBAlhalend - 2);

  desc.RenderTarget[0].SrcBlendAlpha = srcAlphaBlend;
  desc.RenderTarget[0].DestBlendAlpha = destBAlhalend;
  desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  if (SUCCEEDED(m_pDevice->CreateBlendState(&desc, &m_pState)))
    m_pContext->OMSetBlendState(m_pState, 0, 0xFFFFFFFF);
}

void DX11Context::SetDepth(bool bEnabled)
{
  ID3D11DepthStencilState* pState = bEnabled ? m_states->DepthDefault() : m_states->DepthNone();
  if (pState)
    m_pContext->OMSetDepthStencilState(pState, 0);
}

void DX11Context::SetRasterizerState(D3D11_CULL_MODE cullMode, D3D11_FILL_MODE fillMode)
{
  ID3D11RasterizerState* pState = NULL;

  if (fillMode == D3D11_FILL_SOLID)
  {
    if (cullMode == D3D11_CULL_NONE)
      pState = m_states->CullNone();
    if (cullMode == D3D11_CULL_FRONT)
      pState = m_states->CullClockwise();
    if (cullMode == D3D11_CULL_BACK)
      pState = m_states->CullCounterClockwise();
  }
  if (fillMode == D3D11_FILL_WIREFRAME)
  {
    if (cullMode == D3D11_CULL_NONE)
      pState = m_states->Wireframe();
  }

  if (pState)
    m_pContext->RSSetState(pState);
}

void DX11Context::SetRenderTarget(ID3D11Texture2D* pTexture)
{
  ID3D11RenderTargetView* pRTView = NULL;
  ID3D11DepthStencilView* pDSView = NULL;

  // store current ZBuffer
  m_pContext->OMGetRenderTargets(0, NULL, &pDSView);

  if (pTexture)
  {

    CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(pTexture, D3D11_RTV_DIMENSION_TEXTURE2D);
    m_pDevice->CreateRenderTargetView(pTexture, &rtvDesc, &pRTView);

    CD3D11_VIEWPORT vp(pTexture, pRTView);
    m_pContext->RSSetViewports(1, &vp);
  }

  m_pContext->OMSetRenderTargets(1, &pRTView, pDSView);

  SafeRelease(pRTView);
  SafeRelease(pDSView);
}

void DX11Context::SetSamplerState(UINT uSlot, D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode)
{
  ID3D11SamplerState* pState = NULL;
  if (addressMode == D3D11_TEXTURE_ADDRESS_WRAP)
  {
    if (filter == D3D11_FILTER_MIN_MAG_MIP_POINT)
      pState = m_states->PointWrap();
    if (filter == D3D11_FILTER_MIN_MAG_MIP_LINEAR)
      pState = m_states->LinearWrap();
    if (filter == D3D11_FILTER_ANISOTROPIC)
      pState = m_states->AnisotropicWrap();
  }
  if (addressMode == D3D11_TEXTURE_ADDRESS_CLAMP)
  {
    if (filter == D3D11_FILTER_MIN_MAG_MIP_POINT)
      pState = m_states->PointClamp();
    if (filter == D3D11_FILTER_MIN_MAG_MIP_LINEAR)
      pState = m_states->LinearClamp();
    if (filter == D3D11_FILTER_ANISOTROPIC)
      pState = m_states->AnisotropicClamp();
  }

  if (pState)
    m_pContext->PSSetSamplers(uSlot, 1, &pState);
}

void DX11Context::SetShader(unsigned int iIndex)
{
  m_pContext->VSSetShader(m_pVShader, NULL, 0);
  m_pContext->VSSetConstantBuffers(0, 1, &m_pCBuffer);

  if (iIndex >= MAX_NUM_SHADERS)
    return;

  m_pContext->PSSetShader(m_pPShader[iIndex], NULL, 0);
  m_uCurrShader = iIndex;
}

void DX11Context::SetTexture(unsigned int iSlot, ID3D11Resource* pResource)
{
  ID3D11ShaderResourceView* views[1] = { NULL };
  if (pResource)
  {
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    D3D11_RESOURCE_DIMENSION dim;
    pResource->GetType(&dim);

    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
    {
      CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc1(reinterpret_cast<ID3D11Texture2D*>(pResource), D3D11_SRV_DIMENSION_TEXTURE2D);
      srvDesc = srvDesc1;
    }
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
    {
      CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc1(reinterpret_cast<ID3D11Texture3D*>(pResource));
      srvDesc = srvDesc1;
    }
    m_pDevice->CreateShaderResourceView(pResource, &srvDesc, &views[0]);
  }

  m_pContext->PSSetShaderResources(iSlot, 1, views);

  if (views[0])
    views[0]->Release();
}

void DX11Context::SetTransform(unsigned int transType, DirectX::XMMATRIX* pMatrix)
{
  if (transType == 2)   // view
    XMStoreFloat4x4(&m_transforms.view, XMMatrixTranspose(*pMatrix));
  if (transType == 3)   // projection
    XMStoreFloat4x4(&m_transforms.proj, XMMatrixTranspose(*pMatrix));
  if (transType == 256) // world
    XMStoreFloat4x4(&m_transforms.world, XMMatrixTranspose(*pMatrix));

  m_bCBufferIsDirty = true;
}

void DX11Context::SetVertexColor(bool bUseColor)
{
  if (!bUseColor)
    m_pContext->PSSetShader(m_pPShader[m_uCurrShader], NULL, 0);
  else
    m_pContext->PSSetShader(m_pPShader[2], NULL, 0);
}

HRESULT DX11Context::CreateVertexShader(const void* pByteCode, SIZE_T codeLength, ID3D11VertexShader** ppShader, CConstantTable* p_constant_table)
{
  if (!p_constant_table->GrabShaderData(m_pDevice))
    return S_FALSE;
  return m_pDevice->CreateVertexShader(pByteCode, codeLength, NULL, ppShader);
}

HRESULT DX11Context::CreatePixelShader(const void* pByteCode, SIZE_T codeLength, ID3D11PixelShader** ppShader, CConstantTable* p_constant_table)
{
  if (!p_constant_table->GrabShaderData(m_pDevice))
    return S_FALSE;
  return m_pDevice->CreatePixelShader(pByteCode, codeLength, NULL, ppShader);
}

void DX11Context::SetVertexShader(ID3D11VertexShader* pVShader, CConstantTable* pTable)
{
  if (pTable)
  {
    pTable->ApplyChanges(m_pContext);
    ID3D11Buffer** ppBuffers = new ID3D11Buffer*[pTable->GetBuffersCount()];
    pTable->GetBuffers(ppBuffers);
    m_pContext->VSSetConstantBuffers(0, pTable->GetBuffersCount(), ppBuffers);
    delete[] ppBuffers;
  }
  m_pContext->VSSetShader((pVShader ? pVShader : m_pVShader), NULL, 0);
}

void DX11Context::SetPixelShader(ID3D11PixelShader* pPShader, CConstantTable* pTable)
{
  if (pTable)
  {
    pTable->ApplyChanges(m_pContext);
    ID3D11Buffer** ppBuffers = new ID3D11Buffer*[pTable->GetBuffersCount()];
    pTable->GetBuffers(ppBuffers);
    m_pContext->PSSetConstantBuffers(0, pTable->GetBuffersCount(), ppBuffers);
    delete[] ppBuffers;
  }
  m_pContext->PSSetShader((pPShader ? pPShader : m_pPShader[m_uCurrShader]), NULL, 0);
}

void DX11Context::ClearRenderTarget(ID3D11Texture2D* pRTTexture, const float color[4])
{
  ID3D11RenderTargetView* pRTView = NULL;
  if (pRTTexture)
  {
    CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(pRTTexture, D3D11_RTV_DIMENSION_TEXTURE2D);
    m_pDevice->CreateRenderTargetView(pRTTexture, &rtvDesc, &pRTView);
    m_pContext->ClearRenderTargetView(pRTView, color);
  }
  if (pRTView)
    pRTView->Release();
}

void DX11Context::CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)
{
  m_pImmContext->CopyResource(pDstResource, pSrcResource);
}

char* _WideToUTF8(const wchar_t* WFilename)
{
  int SizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &WFilename[0], -1, NULL, 0, NULL, NULL);
  char* utf8Name = new char[SizeNeeded];
  WideCharToMultiByte(CP_UTF8, 0, &WFilename[0], -1, &utf8Name[0], SizeNeeded, NULL, NULL);
  return utf8Name;
}

HRESULT DX11Context::CreateTextureFromFile(LPCWSTR szFileName, ID3D11Resource** texture)
{
  std::string strFileName(_WideToUTF8(szFileName));
  if (GetExtension(strFileName) == "dds")
  {
    return CreateDDSTextureFromFile(m_pDevice, szFileName, texture, reinterpret_cast<ID3D11ShaderResourceView**>(NULL));
  }
  else
  {
    return CreateWICTextureFromFile(m_pDevice, szFileName, texture, reinterpret_cast<ID3D11ShaderResourceView**>(NULL));
  }
}

bool DX11Context::LockRect(ID3D11Resource* pResource, UINT uSubRes, D3D11_MAP mapType, D3D11_MAPPED_SUBRESOURCE * res)
{
  HRESULT hr = m_pImmContext->Map(pResource, uSubRes, mapType, 0, res);
  return S_OK == hr;
}

void DX11Context::UnlockRect(ID3D11Resource* pResource, UINT uSubRes)
{
  m_pImmContext->Unmap(pResource, uSubRes);
}

int DX11Context::NumVertsFromType(unsigned int primType, int iPrimCount)
{
  switch (primType)
  {
  case D3DPT_POINTLIST:
    return iPrimCount;
  case D3DPT_LINELIST:
    return iPrimCount * 2;
  case D3DPT_LINESTRIP:
    return iPrimCount + 1;
  case D3DPT_TRIANGLELIST:
    return iPrimCount * 3;
  case D3DPT_TRIANGLESTRIP:
  case D3DPT_TRIANGLEFAN:
    return iPrimCount + 2;
  default:
    return -1;
  }
}

void DX11Context::UpdateVBuffer(unsigned int iNumVerts, const void* pVData, unsigned int vertexStride)
{
  D3D11_MAPPED_SUBRESOURCE res;
  if (S_OK == m_pContext->Map(m_pVBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res))
  {
    memcpy(res.pData, pVData, min(iNumVerts, MAX_VERTICES_COUNT) * vertexStride);
    m_pContext->Unmap(m_pVBuffer, 0);
  }
}

void DX11Context::UpdateIBuffer(unsigned int iNumIndices, const void* pIData)
{
  D3D11_MAPPED_SUBRESOURCE res;
  if (S_OK == m_pContext->Map(m_pIBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res))
  {
    memcpy(res.pData, pIData, min(iNumIndices, MAX_VERTICES_COUNT / 6) * sizeof(uint16_t));
    m_pContext->Unmap(m_pIBuffer, 0);
  }
}
