# Compiles the MCM config script without the Creation Kit. Needs the Papyrus
# compiler (installed with the CK), the vanilla + SKSE script sources, and MCM
# Helper's SDK sources. Adjust the three paths to your setup.
$src  = "$PSScriptRoot\..\dist\Source\Scripts"
$sdk  = "$env:LOCALAPPDATA\ModOrganizer\Skyrim Special Edition\mods\MCM SDK\Source\Scripts"
$base = "C:\Game\SkyrimSE\Data\Scripts\Source"
$out  = "$PSScriptRoot\..\dist\Scripts"
& "C:\Game\SkyrimSE\Papyrus Compiler\PapyrusCompiler.exe" "MCC_MCM.psc" -f="TESV_Papyrus_Flags.flg" -i="$src;$sdk;$base" -o="$out"
