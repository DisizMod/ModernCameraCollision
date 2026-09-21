# Third-party software

Modern Camera Collision links these statically into `ModernCameraCollision.dll`.

| Library | Licence | Source |
|---|---|---|
| CommonLibSSE-NG | MIT | https://github.com/alandtse/CommonLibSSE-NG |
| MinHook | BSD 2-Clause | https://github.com/TsudaKageyu/minhook |
| spdlog | MIT | https://github.com/gabime/spdlog |
| fmt | MIT | https://github.com/fmtlib/fmt |
| SimpleIni | MIT | https://github.com/brofield/simpleini |
| nlohmann/json | MIT | https://github.com/nlohmann/json |
| DirectXMath / DirectXTK | MIT | https://github.com/microsoft/DirectXMath |

It also runs on, but does not include:

- SKSE64 — https://skse.silverlock.org/
- Address Library for SKSE Plugins — https://www.nexusmods.com/skyrimspecialedition/mods/32444
- MCM Helper — https://github.com/Exit-9B/MCM-Helper (MIT), whose `MCM_ConfigBase` the Papyrus script extends
- SkyUI

The prediction rays, the hold after the way clears and the eased pull-in follow
ideas from John Nesky's GDC 2014 talk "50 Camera Mistakes" (Journey).
