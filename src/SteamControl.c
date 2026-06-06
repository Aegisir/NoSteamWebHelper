#include "SteamControl.h"
#include "NativeWindow.h"

#include <winternl.h>
#include <wtsapi32.h>

#define STEAM_KEY_PATH L"SOFTWARE\\Valve\\Steam"
#define STEAM_RUNNING_APP_ID L"RunningAppID"
#define STEAM_WINDOW_CLASS L"vguiPopupWindow"
#define STEAM_WEBHELPER_EXE L"steamwebhelper.exe"

static HINSTANCE g_instance;
static HANDLE g_steamThread;
static volatile LONG g_hookStarted;
static volatile LONG g_runtimeStarted;
static volatile LONG g_disabled;
static volatile LONG g_threadSuspended;

static DWORD WINAPI HookThreadProc(LPVOID parameter);
static DWORD WINAPI RegistryThreadProc(LPVOID parameter);
static VOID CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objectId, LONG childId,
                                  DWORD eventThreadId, DWORD eventTime);

static BOOL IsSteamWindow(HWND hwnd)
{
    WCHAR className[16] = {};

    if (!hwnd || !GetClassNameW(hwnd, className, 16))
        return FALSE;

    return CompareStringOrdinal(STEAM_WINDOW_CLASS, -1, className, -1, FALSE) == CSTR_EQUAL &&
           GetWindowTextLengthW(hwnd) > 0;
}

static BOOL ReadDisabledState(HKEY key, BOOL *disabled)
{
    DWORD value = 0;
    DWORD size = sizeof(value);

    if (RegGetValueW(key, NULL, STEAM_RUNNING_APP_ID, RRF_RT_REG_DWORD, NULL, &value, &size) != ERROR_SUCCESS)
        return FALSE;

    *disabled = value ? TRUE : FALSE;
    return TRUE;
}

static VOID KillChildWebHelpers(void)
{
    WTS_PROCESS_INFOW *processes = NULL;
    DWORD count = 0;
    DWORD parentId = GetCurrentProcessId();

    if (!WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &processes, &count))
        return;

    for (DWORD index = 0; index < count; ++index)
    {
        if (!processes[index].pProcessName ||
            CompareStringOrdinal(processes[index].pProcessName, -1, STEAM_WEBHELPER_EXE, -1, TRUE) != CSTR_EQUAL)
            continue;

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE,
                                     processes[index].ProcessId);
        if (!process)
            continue;

        PROCESS_BASIC_INFORMATION info = {};
        if (NtQueryInformationProcess(process, ProcessBasicInformation, &info, sizeof(info), NULL) >= 0 &&
            (DWORD)(ULONG_PTR)info.InheritedFromUniqueProcessId == parentId)
            TerminateProcess(process, 0);

        CloseHandle(process);
    }

    WTSFreeMemory(processes);
}

static VOID ApplyDisabledState(BOOL disabled)
{
    InterlockedExchange(&g_disabled, disabled ? 1 : 0);
    NativeWindow_SetDisabled(disabled);

    if (disabled)
    {
        // Avoid stacking suspend counts while still killing newly spawned helpers.
        if (g_steamThread && InterlockedCompareExchange(&g_threadSuspended, 1, 0) == 0)
            SuspendThread(g_steamThread);
        KillChildWebHelpers();
        return;
    }

    if (g_steamThread && InterlockedCompareExchange(&g_threadSuspended, 0, 1) == 1)
        ResumeThread(g_steamThread);
}

static BOOL StartRuntime(DWORD steamThreadId)
{
    if (InterlockedCompareExchange(&g_runtimeStarted, 1, 0) != 0)
        return TRUE;

    g_steamThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, steamThreadId);
    NativeWindow_Start(g_instance);

    HANDLE thread = CreateThread(NULL, 0, RegistryThreadProc, NULL, 0, NULL);
    if (thread)
    {
        CloseHandle(thread);
        return TRUE;
    }

    if (g_steamThread)
    {
        CloseHandle(g_steamThread);
        g_steamThread = NULL;
    }
    InterlockedExchange(&g_runtimeStarted, 0);
    return FALSE;
}

BOOL SteamControl_Start(HINSTANCE hInstance)
{
    g_instance = hInstance;

    if (InterlockedCompareExchange(&g_hookStarted, 1, 0) != 0)
        return TRUE;

    HANDLE thread = CreateThread(NULL, 0, HookThreadProc, NULL, 0, NULL);
    if (!thread)
    {
        InterlockedExchange(&g_hookStarted, 0);
        return FALSE;
    }

    CloseHandle(thread);
    return TRUE;
}

BOOL SteamControl_SetDisabled(BOOL disabled)
{
    DWORD value = disabled ? 1 : 0;

    return RegSetKeyValueW(HKEY_CURRENT_USER, STEAM_KEY_PATH, STEAM_RUNNING_APP_ID, REG_DWORD, &value,
                          sizeof(value)) == ERROR_SUCCESS;
}

static DWORD WINAPI HookThreadProc(LPVOID parameter)
{
    (void)parameter;

    HWINEVENTHOOK hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, NULL, WinEventProc,
                                         GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT);
    if (!hook)
        return 0;

    MSG message = {};
    while (GetMessageW(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnhookWinEvent(hook);
    return 0;
}

static DWORD WINAPI RegistryThreadProc(LPVOID parameter)
{
    (void)parameter;

    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STEAM_KEY_PATH, 0, KEY_NOTIFY | KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    {
        if (g_steamThread)
        {
            CloseHandle(g_steamThread);
            g_steamThread = NULL;
        }
        InterlockedExchange(&g_runtimeStarted, 0);
        return 0;
    }

    BOOL disabled = FALSE;
    if (ReadDisabledState(key, &disabled))
        ApplyDisabledState(disabled);

    while (RegNotifyChangeKeyValue(key, FALSE, REG_NOTIFY_CHANGE_LAST_SET, NULL, FALSE) == ERROR_SUCCESS)
    {
        disabled = FALSE;
        if (ReadDisabledState(key, &disabled))
            ApplyDisabledState(disabled);
    }

    RegCloseKey(key);
    if (g_steamThread)
    {
        if (InterlockedCompareExchange(&g_threadSuspended, 0, 1) == 1)
            ResumeThread(g_steamThread);
        CloseHandle(g_steamThread);
        g_steamThread = NULL;
    }
    InterlockedExchange(&g_runtimeStarted, 0);
    return 0;
}

static VOID CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objectId, LONG childId,
                                  DWORD eventThreadId, DWORD eventTime)
{
    (void)hook;
    (void)event;
    (void)objectId;
    (void)childId;
    (void)eventTime;

    if (IsSteamWindow(hwnd) && StartRuntime(eventThreadId))
        PostQuitMessage(0);
}
