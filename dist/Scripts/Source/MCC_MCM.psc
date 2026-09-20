ScriptName MCC_MCM extends MCM_ConfigBase
{The MCM Helper config script for Modern Camera Collision. The menu is
config.json; the settings live in MCM Helper's INI files, which the DLL
reads. This script's one job is to tell the DLL when they changed.}

Event OnConfigClose()
	SendModEvent("MCC_SettingsChanged")
EndEvent

Event OnSettingChange(string a_ID)
	Parent.OnSettingChange(a_ID)
	SendModEvent("MCC_SettingsChanged")
EndEvent
