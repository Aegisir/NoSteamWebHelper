#include "NativeWindow.h"
#include "SteamControl.h"

#define TRAY_CLASS_NAME L"NoSteamWebHelperTray"
#define MAIN_CLASS_NAME L"NoSteamWebHelperWindow"
#define WINDOW_TITLE L"NoSteamWebHelper"
#define TRAY_MESSAGE (WM_APP + 1)
#define UI_SHOW_MESSAGE (WM_APP + 2)
#define UI_STATE_MESSAGE (WM_APP + 3)

#define TRAY_ID 1
#define IDM_SHOW 1001
#define IDM_ENABLE 1002
#define IDM_DISABLE 1003
#define IDC_STATUS 2001
#define IDC_ENABLE 2002
#define IDC_DISABLE 2003

static HINSTANCE g_instance;
static HWND g_trayWindow;
static HWND g_mainWindow;
static HWND g_statusText;
static HWND g_enableButton;
static HWND g_disableButton;
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
static LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

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

static VOID UpdateMainWindowState(void)
{
    BOOL disabled = InterlockedCompareExchange(&g_disabled, 0, 0) ? TRUE : FALSE;

    if (g_statusText)
        SetWindowTextW(g_statusText, disabled ? L"CEF status: disabled" : L"CEF status: enabled");
    if (g_enableButton)
        EnableWindow(g_enableButton, disabled);
    if (g_disableButton)
        EnableWindow(g_disableButton, !disabled);
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

static VOID ShowMainWindow(void)
{
    if (!g_mainWindow)
        g_mainWindow = CreateWindowExW(0, MAIN_CLASS_NAME, WINDOW_TITLE,
                                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                                       CW_USEDEFAULT, 360, 160, NULL, NULL, g_instance, NULL);

    if (!g_mainWindow)
        return;

    UpdateMainWindowState();
    ShowWindow(g_mainWindow, SW_SHOWNORMAL);
    SetForegroundWindow(g_mainWindow);
}

static VOID ShowTrayMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    BOOL disabled = InterlockedCompareExchange(&g_disabled, 0, 0) ? TRUE : FALSE;
    AppendMenuW(menu, MF_STRING, IDM_SHOW, L"Show");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
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

    if (command == IDM_SHOW)
        ShowMainWindow();
    else if (command == IDM_ENABLE)
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

    if (g_trayWindow)
        PostMessageW(g_trayWindow, UI_STATE_MESSAGE, disabled ? 1 : 0, 0);
}

static DWORD WINAPI UiThreadProc(LPVOID parameter)
{
    (void)parameter;

    if (!RegisterWindowClass(TRAY_CLASS_NAME, TrayWindowProc) || !RegisterWindowClass(MAIN_CLASS_NAME, MainWindowProc))
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
        else if (lParam == WM_LBUTTONDBLCLK)
            ShowMainWindow();
        return 0;

    case UI_SHOW_MESSAGE:
        ShowMainWindow();
        return 0;

    case UI_STATE_MESSAGE:
        InterlockedExchange(&g_disabled, wParam ? 1 : 0);
        UpdateMainWindowState();
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

static LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;

    switch (message)
    {
    case WM_CREATE:
        g_statusText = CreateWindowExW(0, L"STATIC", L"CEF status: enabled", WS_CHILD | WS_VISIBLE, 24, 22, 300, 24,
                                       hwnd, (HMENU)IDC_STATUS, g_instance, NULL);
        g_enableButton = CreateWindowExW(0, L"BUTTON", L"Enable CEF", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 24, 64,
                                         138, 30, hwnd, (HMENU)IDC_ENABLE, g_instance, NULL);
        g_disableButton = CreateWindowExW(0, L"BUTTON", L"Disable CEF", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 176, 64,
                                          138, 30, hwnd, (HMENU)IDC_DISABLE, g_instance, NULL);
        UpdateMainWindowState();
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_ENABLE)
            SteamControl_SetDisabled(FALSE);
        else if (LOWORD(wParam) == IDC_DISABLE)
            SteamControl_SetDisabled(TRUE);
        return 0;

    case WM_CLOSE:
        // Keep the UI thread alive; the tray icon owns lifetime.
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        g_mainWindow = NULL;
        g_statusText = NULL;
        g_enableButton = NULL;
        g_disableButton = NULL;
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
