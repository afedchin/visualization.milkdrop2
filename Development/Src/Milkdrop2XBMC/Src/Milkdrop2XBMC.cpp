#include <windows.h>
#include <io.h>
#include <vector>
#include "../../vis_milk2/vis_milk2/plugin.h"


#define TARGET_WINDOWS
#include "addons/include/xbmc_vis_dll.h"

CPlugin g_plugin;
bool IsInitialized = false;

int	GNumPresets = 0;
char** GAllPresetStrings = NULL;

extern "C" ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
	if (!props)
		return ADDON_STATUS_UNKNOWN;

	VIS_PROPS* visprops = (VIS_PROPS*)props;

	swprintf(g_plugin.m_szPluginsDirPath, L"%hs\\", visprops->presets);

  if (FALSE == g_plugin.PluginPreInitialize(0, 0))
    return ADDON_STATUS_UNKNOWN;

  if (FALSE == g_plugin.PluginInitialize((ID3D11Device*)visprops->device, (ID3D11DeviceContext*)visprops->context, visprops->x, visprops->y, visprops->width, visprops->height, visprops->pixelRatio))
    return ADDON_STATUS_UNKNOWN;

  IsInitialized = true;
	return ADDON_STATUS_NEED_SETTINGS;
//  return ADDON_STATUS_NEED_SAVEDSETTINGS; // We need some settings to be saved later before we quit this plugin
}

extern "C" void Start( int iChannels, int iSamplesPerSec, int iBitsPerSample, const char* szSongName )
{
}

extern "C" void ADDON_Stop()
{
	if( IsInitialized )
	{
		g_plugin.PluginQuit();
	
		for( int i = 0;  i < GNumPresets; i++ )
		{
			delete[] GAllPresetStrings[ i ];
		}

		delete[] GAllPresetStrings;
		GAllPresetStrings = NULL;

		IsInitialized = false;
	}
}

unsigned char waves[2][576];

extern "C" void AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
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

extern "C" void Render()
{
	g_plugin.PluginRender(waves[0], waves[1]);
}

extern "C" void GetInfo(VIS_INFO* pInfo)

{
	pInfo->bWantsFreq = false;
	pInfo->iSyncDelay = 0;
}

extern "C" bool OnAction(long action, const void *param)
{
	bool bHandled = false;

	if( action == VIS_ACTION_UPDATE_TRACK )
	{
	}
	else if( action == VIS_ACTION_UPDATE_ALBUMART )
	{
	}
	else if (action == VIS_ACTION_NEXT_PRESET)
	{
		g_plugin.NextPreset(1.0f);
		bHandled = true;
	}
	else if (action == VIS_ACTION_PREV_PRESET)
	{
    g_plugin.PrevPreset(1.0f);
    bHandled = true;
  }
	else if (action == VIS_ACTION_LOAD_PRESET && param)
	{
		g_plugin.m_nCurrentPreset = (*(int *)param) + g_plugin.m_nDirs;

		wchar_t szFile[MAX_PATH] = {0};
		lstrcpyW(szFile, g_plugin.m_szPresetDir);	// note: m_szPresetDir always ends with '\'
		lstrcatW(szFile, g_plugin.m_presets[g_plugin.m_nCurrentPreset].szFilename.c_str());

		g_plugin.LoadPreset(szFile, 1.0f);
		bHandled = true;
	}
	else if (action == VIS_ACTION_LOCK_PRESET)
	{
    g_plugin.m_bPresetLockedByUser = !g_plugin.m_bPresetLockedByUser;
    bHandled = true;
  }
	else if (action == VIS_ACTION_RANDOM_PRESET)
	{
		g_plugin.LoadRandomPreset(1.0f);
		bHandled = true;
	}

	return bHandled;
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
extern "C" unsigned int GetPresets(char ***presets)
{
	if( !presets || !IsInitialized )
	{
		return 0;
	}

	while( !g_plugin.m_bPresetListReady )
	{

	}

	if( GAllPresetStrings )
	{
		for( int i = 0;  i < GNumPresets; i++ )
		{
			delete[] GAllPresetStrings[ i ];
		}

		delete[] GAllPresetStrings;
	}


	GNumPresets = g_plugin.m_nPresets - g_plugin.m_nDirs;

	GAllPresetStrings = new char*[ GNumPresets ];

	for( int i = 0;  i < GNumPresets; i++ )
	{
		PresetInfo& Info = g_plugin.m_presets[ i + g_plugin.m_nDirs ];
		GAllPresetStrings[ i ] = WideToUTF8( Info.szFilename.c_str() );
	}

	*presets = GAllPresetStrings;
	return GNumPresets;
}

//-- GetPreset ----------------------------------------------------------------
// Return the index of the current playing preset
//-----------------------------------------------------------------------------
extern "C" unsigned GetPreset()
{
	if( IsInitialized )
	{
		int CurrentPreset = g_plugin.m_nCurrentPreset;
		CurrentPreset -= g_plugin.m_nDirs;
		return CurrentPreset;
	}
	return 0;
}

//-- IsLocked -----------------------------------------------------------------
// Returns true if preset is locked
//-----------------------------------------------------------------------------
extern "C" bool IsLocked()
{
	return false;
}

//-- Destroy-------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" void ADDON_Destroy()
{
	ADDON_Stop();
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" bool ADDON_HasSettings()
{
	return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
extern "C" ADDON_STATUS ADDON_GetStatus()
{
	return ADDON_STATUS_OK;
}

extern "C" unsigned int ADDON_GetSettings(ADDON_StructSetting*** sSet)
{
	return 0;
}

extern "C" void ADDON_FreeSettings()
{

}

extern "C" ADDON_STATUS ADDON_SetSetting(const char* id, const void* value)
{
	/*
	if ( !id || !value || IsInitialized == NULL )
		return ADDON_STATUS_UNKNOWN;
	*/
	return ADDON_STATUS_OK;
}

//-- GetSubModules ------------------------------------------------------------
// Return any sub modules supported by this vis
//-----------------------------------------------------------------------------
extern "C"   unsigned int GetSubModules(char ***presets)
{
  return 0; // this vis supports 0 sub modules
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
//-----------------------------------------------------------------------------
extern "C" void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}