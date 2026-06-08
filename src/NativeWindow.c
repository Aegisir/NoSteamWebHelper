#include "NativeWindow.h"
#include "SteamControl.h"

#define TRAY_CLASS_NAME L"NoSteamWebHelperTray"
#define TRAY_MESSAGE (WM_APP + 1)

#define TRAY_ID 1
#define IDM_ENABLE 1002
#define IDM_DISABLE 1003

static HINSTANCE g_instance;
static HWND g_trayWindow;
static UINT g_taskbarCreated;
static volatile LONG g_started;
static volatile LONG g_disabled;
static NOTIFYICONDATAW g_trayIcon = {
    .cbSize = sizeof(NOTIFYICONDATAW),
    .uID = TRAY_ID,
    .uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP,
    .uCallbackMessage = TRAY_MESSAGE,
    .szTip = L"Steam WebHelper",
};

static DWORD WINAPI UiThreadProc(LPVOID parameter);
static LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static BOOL RegisterWindowClass(LPCWSTR className, WNDPROC procedure)
{
    WNDCLASSW windowClass = {
        .hInstance = g_instance,
        .lpfnWndProc = procedure,
        .lpszClassName = className,
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
    };

    return RegisterClassW(&windowClass) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static VOID AddTrayIcon(HWND hwnd)
{
    g_trayIcon.hWnd = hwnd;
    g_trayIcon.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    Shell_NotifyIconW(NIM_ADD, &g_trayIcon);
}

static VOID DeleteTrayIcon(HWND hwnd)
{
    g_trayIcon.hWnd = hwnd;
    Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
}

static VOID ShowTrayMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    BOOL disabled = InterlockedCompareExchange(&g_disabled, 0, 0) ? TRUE : FALSE;
    AppendMenuW(menu, MF_STRING | (disabled ? 0 : MF_CHECKED), IDM_ENABLE, L"Enable CEF");
    AppendMenuW(menu, MF_STRING | (disabled ? MF_CHECKED : 0), IDM_DISABLE, L"Disable CEF");

    POINT cursor = {};
    GetCursorPos(&cursor);
    SetForegroundWindow(hwnd);

    UINT command = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON |
                                            TPM_RETURNCMD,
                                  cursor.x, cursor.y, 0, hwnd, NULL);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    if (command == IDM_ENABLE)
        SteamControl_SetDisabled(FALSE);
    else if (command == IDM_DISABLE)
        SteamControl_SetDisabled(TRUE);
}

BOOL NativeWindow_Start(HINSTANCE hInstance)
{
    g_instance = hInstance;

    if (InterlockedCompareExchange(&g_started, 1, 0) != 0)
        return TRUE;

    HANDLE thread = CreateThread(NULL, 0, UiThreadProc, NULL, 0, NULL);
    if (!thread)
    {
        InterlockedExchange(&g_started, 0);
        return FALSE;
    }

    CloseHandle(thread);
    return TRUE;
}

VOID NativeWindow_SetDisabled(BOOL disabled)
{
    InterlockedExchange(&g_disabled, disabled ? 1 : 0);
}

static DWORD WINAPI UiThreadProc(LPVOID parameter)
{
    (void)parameter;

    if (!RegisterWindowClass(TRAY_CLASS_NAME, TrayWindowProc))
    {
        InterlockedExchange(&g_started, 0);
        return 0;
    }

    g_trayWindow = CreateWindowExW(WS_EX_LEFT | WS_EX_LTRREADING, TRAY_CLASS_NAME, NULL, WS_OVERLAPPED, 0, 0, 0, 0,
                                  NULL, NULL, g_instance, NULL);
    if (!g_trayWindow)
    {
        InterlockedExchange(&g_started, 0);
        return 0;
    }

    MSG message = {};
    while (GetMessageW(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}

static LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;

    switch (message)
    {
    case WM_CREATE:
        g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        AddTrayIcon(hwnd);
        return 0;

    case TRAY_MESSAGE:
        if (lParam == WM_RBUTTONUP)
            ShowTrayMenu(hwnd);
        return 0;

    case WM_DESTROY:
        DeleteTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;

    default:
        if (g_taskbarCreated && message == g_taskbarCreated)
        {
            AddTrayIcon(hwnd);
            return 0;
        }
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
