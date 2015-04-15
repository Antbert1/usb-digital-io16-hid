// dllmain.c : Defines the entry point for the DLL

#include "targetver.h"
#define WIN32_EXTRALEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>

extern void internal_usb_io16_uninit(void);

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    switch ( ul_reason_for_call )
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        internal_usb_io16_uninit();
        break;
    }
    return TRUE;
}

