/*Copyright(C) Cream-CN 2025-2026*/
/*
 * Simple Notepad for Windows
 * Compatible with Windows 95 - Windows 11
 * Compilers: MSVC, Watcom, MinGW
 * Standard version + Pseudo-MDI (multi-process taskbar grouping)
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 1. Compiler Detection
 * ============================================================================ */

#if defined(_MSC_VER)
    #define COMPILER_NAME "MSVC"
    #define COMPILER_VERSION_STR "MSVC"
#elif defined(__WATCOMC__)
    #define COMPILER_NAME "Open Watcom"
    #define COMPILER_VERSION_STR "Watcom"
#elif defined(__MINGW32__)
    #define COMPILER_NAME "MinGW"
    #define COMPILER_VERSION_STR "MinGW"
#else
    #define COMPILER_NAME "Unknown"
    #define COMPILER_VERSION_STR "Unknown"
#endif

/* ============================================================================
 * 2. Constants and Macros
 * ============================================================================ */

#define CLASS_NAME "SimpleNotepad"
#define APP_NAME "Simple Notepad"
#define APP_USER_MODEL_ID "CreamCN.SimpleNotepad"

#define ID_EDIT 1001
#define ID_FILE_NEW 2001
#define ID_FILE_NEWWINDOW 2002
#define ID_FILE_OPEN 2003
#define ID_FILE_SAVE 2004
#define ID_FILE_SAVEAS 2005
#define ID_FILE_EXIT 2006
#define ID_EDIT_UNDO 3001
#define ID_EDIT_CUT 3002
#define ID_EDIT_COPY 3003
#define ID_EDIT_PASTE 3004
#define ID_EDIT_DELETE 3005
#define ID_EDIT_SELECTALL 3006
#define ID_HELP_ABOUT 4001

#define MAX_FILE_SIZE (4 * 1024 * 1024)

/* ============================================================================
 * 3. Data Structures
 * ============================================================================ */

typedef struct {
    HWND hWnd;
    HWND hEdit;
    char szFileName[MAX_PATH];
    BOOL bFileModified;
    HFONT hFont;
} AppContext;

static AppContext g_ctx = {0};
static char g_szTempBuffer[MAX_PATH];

/* ============================================================================
 * 4. Forward Declarations
 * ============================================================================ */

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static BOOL InitApplication(HINSTANCE);
static BOOL InitInstance(HINSTANCE, int);
static BOOL CreateMainMenu(HWND);
static BOOL CreateEditControl(HWND);
static HFONT CreateNotepadFont(void);
static void UpdateWindowTitle(HWND);
static BOOL CheckFileModified(HWND);
static void ShowAboutDialog(HWND);
static void DoFileNew(HWND);
static BOOL DoFileOpen(HWND);
static BOOL DoFileSave(HWND);
static BOOL DoFileSaveAs(HWND);
static BOOL ReadFileContent(HWND, const char*);
static BOOL WriteFileContent(HWND, const char*);
static void HandleCommand(HWND, int);
static void SetAppUserModelId(void);
static void DoFileNewWindow(HWND);
static void OpenFileFromCommandLine(LPSTR);

/* ============================================================================
 * 5. Pseudo-MDI Support (Taskbar Grouping, Windows 7+)
 * ============================================================================ */

typedef HRESULT (WINAPI *PFN_SetAppID)(PCWSTR);

/*
 * 动态加载 shell32.dll 里的 SetCurrentProcessExplicitAppUserModelID。
 * Win7+ 生效；Win95~Vista 上函数不存在，自动跳过，退化为普通多进程。
 */
static void SetAppUserModelId(void)
{
    HMODULE hShell32;
    PFN_SetAppID pfnSetAppID;
    WCHAR wszAppId[64];
    const char* p;
    int i;

    hShell32 = LoadLibraryA("shell32.dll");
    if (!hShell32)
        return;

    pfnSetAppID = (PFN_SetAppID)GetProcAddress(hShell32,
                    "SetCurrentProcessExplicitAppUserModelID");

    if (pfnSetAppID) {
        p = APP_USER_MODEL_ID;
        for (i = 0; i < 63 && p[i]; i++)
            wszAppId[i] = (WCHAR)(unsigned char)p[i];
        wszAppId[i] = L'\0';

        pfnSetAppID(wszAppId);
    }

    /* 不 FreeLibrary，保持 shell32 加载，避免 AppID 失效 */
}

/*
 * 启动一个新进程，不带文件名。
 * 新进程会得到同样的 AppID，任务栏自动分组（Win7+）。
 */
static void DoFileNewWindow(HWND hWnd)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char szExe[MAX_PATH];

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!GetModuleFileNameA(NULL, szExe, MAX_PATH))
        return;

    if (CreateProcessA(szExe, NULL, NULL, NULL, FALSE,
                       0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        MessageBox(hWnd, "Cannot start new window!", "Error",
                   MB_OK | MB_ICONERROR);
    }
}

/*
 * 解析命令行，打开文件。
 * 支持带引号和不带引号的路径，跳过前导空白。
 */
static void OpenFileFromCommandLine(LPSTR lpCmdLine)
{
    char* p;
    size_t len;

    if (!lpCmdLine || !lpCmdLine[0])
        return;

    p = lpCmdLine;
    while (*p == ' ' || *p == '\t')
        p++;

    if (*p == '"') {
        p++;
        len = strlen(p);
        if (len > 0 && p[len - 1] == '"')
            p[len - 1] = '\0';
    }

    if (!p[0])
        return;

    if (ReadFileContent(g_ctx.hWnd, p)) {
        strncpy(g_ctx.szFileName, p, MAX_PATH - 1);
        g_ctx.szFileName[MAX_PATH - 1] = '\0';
        g_ctx.bFileModified = FALSE;
        UpdateWindowTitle(g_ctx.hWnd);
    } else {
        MessageBox(g_ctx.hWnd, "Cannot open file from command line!",
                   "Error", MB_OK | MB_ICONERROR);
    }
}

/* ============================================================================
 * 6. Helper Functions
 * ============================================================================ */

static BOOL IsStringEmpty(const char* str)
{
    return str == NULL || str[0] == '\0';
}

static const char* GetFileName(const char* path)
{
    const char* p = path;
    const char* last = path;

    while (*p) {
        if (*p == '\\' || *p == '/')
            last = p + 1;
        p++;
    }
    return last;
}

/* ============================================================================
 * 7. Entry Point
 * ============================================================================ */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;

    /* 伪 MDI：设置 AppID，让任务栏分组（Win7+ 生效） */
    SetAppUserModelId();

    /* Initialize common controls */
    #ifdef __WATCOMC__
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icex);
    #else
        INITCOMMONCONTROLSEX icex = {0};
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icex);
    #endif

    if (!InitApplication(hInstance) || !InitInstance(hInstance, nCmdShow))
        return 0;

    /* 命令行带文件名则打开 */
    OpenFileFromCommandLine(lpCmdLine);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

/* ============================================================================
 * 8. Application Initialization
 * ============================================================================ */

static BOOL InitApplication(HINSTANCE hInstance)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    return RegisterClassEx(&wc) != 0;
}

static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_ctx.hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        APP_NAME,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        640, 480,
        NULL, NULL, hInstance, NULL);

    if (!g_ctx.hWnd)
        return FALSE;

    if (!CreateMainMenu(g_ctx.hWnd) || !CreateEditControl(g_ctx.hWnd))
        return FALSE;

    ShowWindow(g_ctx.hWnd, nCmdShow);
    UpdateWindow(g_ctx.hWnd);

    return TRUE;
}

/* ============================================================================
 * 9. UI Creation
 * ============================================================================ */

static BOOL CreateMainMenu(HWND hWnd)
{
    HMENU hMenu, hFileMenu, hEditMenu, hHelpMenu;

    hMenu = CreateMenu();
    if (!hMenu) return FALSE;

    hFileMenu = CreatePopupMenu();
    if (!hFileMenu) { DestroyMenu(hMenu); return FALSE; }

    AppendMenu(hFileMenu, MF_STRING, ID_FILE_NEW, "&New\tCtrl+N");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_NEWWINDOW, "New &Window");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPEN, "&Open\tCtrl+O");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVE, "&Save\tCtrl+S");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVEAS, "Save &As...");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_EXIT, "E&xit");

    hEditMenu = CreatePopupMenu();
    if (!hEditMenu) { DestroyMenu(hMenu); DestroyMenu(hFileMenu); return FALSE; }

    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_CUT, "Cu&t\tCtrl+X");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_COPY, "&Copy\tCtrl+C");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_PASTE, "&Paste\tCtrl+V");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_DELETE, "&Delete\tDel");
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_SELECTALL, "Select &All\tCtrl+A");

    hHelpMenu = CreatePopupMenu();
    if (!hHelpMenu) {
        DestroyMenu(hMenu); DestroyMenu(hFileMenu); DestroyMenu(hEditMenu);
        return FALSE;
    }

    AppendMenu(hHelpMenu, MF_STRING, ID_HELP_ABOUT, "&About");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "&File");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, "&Edit");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, "&Help");

    SetMenu(hWnd, hMenu);
    return TRUE;
}

static HFONT CreateNotepadFont(void)
{
    HFONT hFont;

    /* Try System font first (most compatible) */
    hFont = CreateFont(
        14, 8, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | FF_MODERN,
        "System");

    if (hFont)
        return hFont;

    /* Fallback to Fixedsys */
    hFont = CreateFont(
        14, 8, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | FF_MODERN,
        "Fixedsys");

    if (hFont)
        return hFont;

    /* Last resort: Courier */
    return CreateFont(
        14, 8, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | FF_MODERN,
        "Courier");
}

static BOOL CreateEditControl(HWND hWnd)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    g_ctx.hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
        0, 0,
        rcClient.right, rcClient.bottom,
        hWnd,
        (HMENU)ID_EDIT,
        GetModuleHandle(NULL),
        NULL);

    if (!g_ctx.hEdit)
        return FALSE;

    g_ctx.hFont = CreateNotepadFont();
    if (g_ctx.hFont)
        SendMessage(g_ctx.hEdit, WM_SETFONT, (WPARAM)g_ctx.hFont, TRUE);

    return TRUE;
}

/* ============================================================================
 * 10. File Operations
 * ============================================================================ */

static BOOL ReadFileContent(HWND hWnd, const char* szFileName)
{
    HANDLE hFile;
    DWORD dwSize, dwRead;
    char* pBuffer;
    BOOL success = FALSE;

    hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    dwSize = GetFileSize(hFile, NULL);

    if (dwSize == 0) {
        SetWindowText(g_ctx.hEdit, "");
        success = TRUE;
    } else if (dwSize < MAX_FILE_SIZE) {
        pBuffer = (char*)malloc(dwSize + 1);
        if (pBuffer) {
            if (ReadFile(hFile, pBuffer, dwSize, &dwRead, NULL) && dwRead == dwSize) {
                pBuffer[dwRead] = '\0';
                SetWindowText(g_ctx.hEdit, pBuffer);
                success = TRUE;
            }
            free(pBuffer);
        }
    } else {
        MessageBox(hWnd, "File too large (max 4MB)!", "Error", MB_OK | MB_ICONERROR);
    }

    CloseHandle(hFile);
    return success;
}

static BOOL WriteFileContent(HWND hWnd, const char* szFileName)
{
    HANDLE hFile;
    int len;
    char* pBuffer;
    DWORD dwSize, dwWritten;
    BOOL success = FALSE;

    hFile = CreateFile(szFileName, GENERIC_WRITE, 0,
                       NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    len = GetWindowTextLength(g_ctx.hEdit) + 1;
    pBuffer = (char*)malloc(len);

    if (pBuffer) {
        GetWindowText(g_ctx.hEdit, pBuffer, len);
        dwSize = (DWORD)strlen(pBuffer);

        if (WriteFile(hFile, pBuffer, dwSize, &dwWritten, NULL) && dwWritten == dwSize)
            success = TRUE;

        free(pBuffer);
    }

    CloseHandle(hFile);
    return success;
}

/* ============================================================================
 * 11. File Menu Actions
 * ============================================================================ */

static void DoFileNew(HWND hWnd)
{
    if (!CheckFileModified(hWnd))
        return;

    SetWindowText(g_ctx.hEdit, "");
    g_ctx.szFileName[0] = '\0';
    g_ctx.bFileModified = FALSE;
    UpdateWindowTitle(hWnd);
}

static BOOL DoFileOpen(HWND hWnd)
{
    OPENFILENAME ofn;

    if (!CheckFileModified(hWnd))
        return FALSE;

    g_szTempBuffer[0] = '\0';

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = g_szTempBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open File";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "txt";

    if (!GetOpenFileName(&ofn))
        return FALSE;

    if (!ReadFileContent(hWnd, g_szTempBuffer)) {
        MessageBox(hWnd, "Cannot open file!", "Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    strncpy(g_ctx.szFileName, g_szTempBuffer, MAX_PATH - 1);
    g_ctx.szFileName[MAX_PATH - 1] = '\0';
    g_ctx.bFileModified = FALSE;
    UpdateWindowTitle(hWnd);
    return TRUE;
}

static BOOL DoFileSave(HWND hWnd)
{
    if (IsStringEmpty(g_ctx.szFileName))
        return DoFileSaveAs(hWnd);

    if (!WriteFileContent(hWnd, g_ctx.szFileName)) {
        MessageBox(hWnd, "Cannot save file!", "Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    g_ctx.bFileModified = FALSE;
    UpdateWindowTitle(hWnd);
    return TRUE;
}

static BOOL DoFileSaveAs(HWND hWnd)
{
    OPENFILENAME ofn;

    g_szTempBuffer[0] = '\0';

    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = g_szTempBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Save File";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "txt";

    if (!GetSaveFileName(&ofn))
        return FALSE;

    strncpy(g_ctx.szFileName, g_szTempBuffer, MAX_PATH - 1);
    g_ctx.szFileName[MAX_PATH - 1] = '\0';
    return DoFileSave(hWnd);
}

/* ============================================================================
 * 12. Utility Functions
 * ============================================================================ */

static void UpdateWindowTitle(HWND hWnd)
{
    const char* pFileName;
    char szTitle[MAX_PATH + 32];

    if (IsStringEmpty(g_ctx.szFileName)) {
        pFileName = "Untitled";
    } else {
        pFileName = GetFileName(g_ctx.szFileName);
    }

    wsprintf(szTitle, "%s%s - %s",
             pFileName,
             g_ctx.bFileModified ? "*" : "",
             APP_NAME);

    SetWindowText(hWnd, szTitle);
}

static BOOL CheckFileModified(HWND hWnd)
{
    int result;

    if (!g_ctx.bFileModified)
        return TRUE;

    result = MessageBox(hWnd,
                        "File has been modified. Save changes?",
                        APP_NAME,
                        MB_YESNOCANCEL | MB_ICONQUESTION);

    if (result == IDYES)
        return DoFileSave(hWnd);
    else if (result == IDCANCEL)
        return FALSE;

    return TRUE;
}

static void ShowAboutDialog(HWND hWnd)
{
    char szAbout[1024];

    wsprintf(szAbout,
        "Simple Notepad\n"
        "Version 1.0 (Win95 Compatible)\n"
        "Pseudo-MDI (multi-process)\n\n"
        "Compiler: " COMPILER_NAME " (" COMPILER_VERSION_STR ")\n"
        "Build Date: " __DATE__ "\n"
        "Build Time: " __TIME__ "\n\n"
        "Compatible with Windows 95 - 11");

    MessageBox(hWnd, szAbout, "About", MB_OK | MB_ICONINFORMATION);
}

/* ============================================================================
 * 13. Command Handler
 * ============================================================================ */

static void HandleCommand(HWND hWnd, int wmId)
{
    switch (wmId) {
    case ID_FILE_NEW:       DoFileNew(hWnd); break;
    case ID_FILE_NEWWINDOW: DoFileNewWindow(hWnd); break;
    case ID_FILE_OPEN:      DoFileOpen(hWnd); break;
    case ID_FILE_SAVE:      DoFileSave(hWnd); break;
    case ID_FILE_SAVEAS:    DoFileSaveAs(hWnd); break;
    case ID_FILE_EXIT:      PostMessage(hWnd, WM_CLOSE, 0, 0); break;

    case ID_EDIT_UNDO:      SendMessage(g_ctx.hEdit, WM_UNDO, 0, 0); break;
    case ID_EDIT_CUT:       SendMessage(g_ctx.hEdit, WM_CUT, 0, 0); break;
    case ID_EDIT_COPY:      SendMessage(g_ctx.hEdit, WM_COPY, 0, 0); break;
    case ID_EDIT_PASTE:     SendMessage(g_ctx.hEdit, WM_PASTE, 0, 0); break;
    case ID_EDIT_DELETE:    SendMessage(g_ctx.hEdit, WM_CLEAR, 0, 0); break;
    case ID_EDIT_SELECTALL: SendMessage(g_ctx.hEdit, EM_SETSEL, 0, -1); break;

    case ID_HELP_ABOUT:     ShowAboutDialog(hWnd); break;
    }
}

/* ============================================================================
 * 14. Window Procedure
 * ============================================================================ */

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_SIZE: {
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        SetWindowPos(g_ctx.hEdit, NULL, 0, 0,
                    rcClient.right, rcClient.bottom,
                    SWP_NOZORDER);
        break;
    }

    case WM_COMMAND:
        HandleCommand(hWnd, LOWORD(wParam));
        break;

    case WM_CLOSE:
        if (CheckFileModified(hWnd))
            DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        if (g_ctx.hFont) {
            DeleteObject(g_ctx.hFont);
            g_ctx.hFont = NULL;
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
