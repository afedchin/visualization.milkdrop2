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

#include <windows.h>
#include <io.h>
#include <vector>
#include "vis_milk2/plugin.h"

#ifndef TARGET_WINDOWS
#define TARGET_WINDOWS
#endif

#include <kodi/addon-instance/Visualization.h>

CPlugin g_plugin;
bool IsInitialized = false;

class CVisualizationMilkdrop2
  : public kodi::addon::CAddonBase
  , public kodi::addon::CInstanceVisualization
{
public:
  ~CVisualizationMilkdrop2() override;

  ADDON_STATUS Create() override;
  void Stop() override;
  void AudioData(const float* audioData, int audioDataLength, float* freqData, int freqDataLength) override;
  void Render() override;
  bool GetPresets(std::vector<std::string>& presets) override;
  int GetActivePreset() override;
  bool IsLocked() override { return g_plugin.m_bPresetLockedByUser; }
  bool PrevPreset() override;
  bool NextPreset() override;
  bool LoadPreset(int select) override;
  bool RandomPreset() override;
  bool LockPreset(bool lockUnlock) override;
};

ADDON_STATUS CVisualizationMilkdrop2::Create()
{
  swprintf(g_plugin.m_szPluginsDirPath, L"%hs\\resources\\", Presets().c_str());

  if (FALSE == g_plugin.PluginPreInitialize(0, 0))
    return ADDON_STATUS_UNKNOWN;

  if (FALSE == g_plugin.PluginInitialize(static_cast<ID3D11DeviceContext*>(Device()), X(), Y(), Width(), Height(), static_cast<double>(Width()) / static_cast<double>(Height())))
    return ADDON_STATUS_UNKNOWN;

  IsInitialized = true;
  return ADDON_STATUS_OK;
}

void CVisualizationMilkdrop2::Stop()
{
  if( IsInitialized )
  {
    g_plugin.PluginQuit();

    IsInitialized = false;
  }
}

unsigned char waves[2][576];

void CVisualizationMilkdrop2::AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
{
  int ipos=0;
  while (ipos < 576)
  {
    for (int i=0; i < iAudioDataLength; i+=2)
    {
      waves[0][ipos] = char (pAudioData[i] * 255.0f);
      waves[1][ipos] = char (pAudioData[i+1]  * 255.0f);
      ipos++;
      if (ipos >= 576) break;
    }
  }
}

void CVisualizationMilkdrop2::Render()
{
  g_plugin.PluginRender(waves[0], waves[1]);
}

bool CVisualizationMilkdrop2::NextPreset()
{
  g_plugin.NextPreset(1.0f);
  return true;
}

bool CVisualizationMilkdrop2::PrevPreset()
{
  g_plugin.PrevPreset(1.0f);
  return true;
}

bool CVisualizationMilkdrop2::LoadPreset(int select)
{
  g_plugin.m_nCurrentPreset = select + g_plugin.m_nDirs;

  wchar_t szFile[MAX_PATH] = { 0 };
  wcscpy(szFile, g_plugin.m_szPresetDir);  // note: m_szPresetDir always ends with '\'
  wcscat(szFile, g_plugin.m_presets[g_plugin.m_nCurrentPreset].szFilename.c_str());

  g_plugin.LoadPreset(szFile, 1.0f);
  return true;
}

bool CVisualizationMilkdrop2::LockPreset(bool lockUnlock)
{
  g_plugin.m_bPresetLockedByUser = lockUnlock;
  return true;
}

bool CVisualizationMilkdrop2::RandomPreset()
{
  g_plugin.LoadRandomPreset(1.0f);
  return true;
}

char* WideToUTF8( const wchar_t* WFilename )
{
  int SizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &WFilename[0], -1, NULL, 0, NULL, NULL);
  char* utf8Name = new char[ SizeNeeded ];
  WideCharToMultiByte(CP_UTF8, 0, &WFilename[0], -1, &utf8Name[0], SizeNeeded, NULL, NULL);
  return utf8Name;
}

//-- GetPresets ---------------------------------------------------------------
// Return a list of presets to XBMC for display
//-----------------------------------------------------------------------------
bool CVisualizationMilkdrop2::GetPresets(std::vector<std::string>& presets)
{
  if(!IsInitialized )
    return false;

  while( !g_plugin.m_bPresetListReady )
  {

  }

  for( int i = 0;  i < g_plugin.m_nPresets - g_plugin.m_nDirs; ++i)
  {
    PresetInfo& Info = g_plugin.m_presets[ i + g_plugin.m_nDirs ];
    presets.push_back(WideToUTF8( Info.szFilename.c_str() ));
  }

  return true;
}

//-- GetPreset ----------------------------------------------------------------
// Return the index of the current playing preset
//-----------------------------------------------------------------------------
int CVisualizationMilkdrop2::GetActivePreset()
{
  if( IsInitialized )
  {
    int CurrentPreset = g_plugin.m_nCurrentPreset;
    CurrentPreset -= g_plugin.m_nDirs;
    return CurrentPreset;
  }
  return -1;
}

//-- Destroy-------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
CVisualizationMilkdrop2::~CVisualizationMilkdrop2()
{
  Stop();
}

ADDONCREATOR(CVisualizationMilkdrop2) // Don't touch this!
