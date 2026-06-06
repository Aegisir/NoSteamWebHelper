#include <windows.h>

#include "SteamControl.h"

BOOL WINAPI DllMainCRTStartup(HINSTANCE hLibModule, DWORD dwReason, LPVOID lpReserved)
{
    (void)lpReserved;

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hLibModule);
        SteamControl_Start(hLibModule);
    }
    return TRUE;
}
