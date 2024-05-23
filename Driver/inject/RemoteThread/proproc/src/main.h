#include <windows.h>
#include <tchar.h>
#include <string>
#include <psapi.h>
#include "detours.h"

#define ATTACH(x,y)   DetAttach(x,y,#x)
#define DETACH(x,y)   DetDetach(x,y,#x)

#ifdef __cplusplus
extern "C" {
#endif

int TestMessageBox(int a, int b);

BOOL OnProcessAttach( _In_ HMODULE ModuleHandle);

BOOL ProcessDetach(HMODULE hDll);

LONG AttachDetours(VOID);

LONG DetachDetours(VOID);

VOID DetAttach(PVOID *ppbReal, PVOID pbMine, PCHAR psz);

VOID DetDetach(PVOID *ppbReal, PVOID pbMine, PCHAR psz);

BOOL ThreadAttach(HMODULE hDll);

BOOL ThreadDetach(HMODULE hDll);

#ifdef __cplusplus
}
#endif