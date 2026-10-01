/*Copyright(C) Cream-CN 2025-2026*/ 
/*
 * Simple Notepad for Windows
 * Optimized for TCC (Tiny C Compiler)
 * Modified version for TinyCC
 * Performance optimizations:
 * - Minimized function calls in hot paths
 * - Stack allocation instead of heap where possible
 * - Reduced string operations
 * - Inline critical functions
 * - Optimized file I/O with larger buffers
 */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 1. Constants and Macros
 * ============================================================================ */

#define CLASS_NAME "SimpleNotepad"
#define APP_NAME "Simple Notepad"

// Control IDs
#define ID_EDIT 1001
#define ID_FILE_NEW 2001
#define ID_FILE_OPEN 2002
#define ID_FILE_SAVE 2003
#define ID_FILE_SAVEAS 2004
#define ID_FILE_EXIT 2005
#define ID_EDIT_UNDO 3001
#define ID_EDIT_CUT 3002
#define ID_EDIT_COPY 3003
#define ID_EDIT_PASTE 3004
#define ID_EDIT_DELETE 3005
#define ID_EDIT_SELECTALL 3006
#define ID_HELP_ABOUT 4001

// Performance tuning
#define FILE_READ_BUFFER_SIZE   (64 * 1024)  // 64KB buffer for file I/O
#define MAX_FILE_SIZE           (4 * 1024 * 1024)  // 4MB max file size
#define EDIT_BUFFER_INCREMENT   (64 * 1024)  // 64KB increment for edit operations

/* ============================================================================
 * 2. Compiler Detection
 * ============================================================================ */

#ifdef __TINYC__
    #define COMPILER_NAME "TCC (Tiny C Compiler)"
    #define COMPILER_VERSION_STR __TINYC_STR__
    // TCC-specific optimizations
    #define FORCE_INLINE static inline
#elif defined(_MSC_VER)
    #define COMPILER_NAME "Microsoft Visual C++"
    #define STRINGIFY(x) #x
    #define TOSTRING(x) STRINGIFY(x)
    #define COMPILER_VERSION_STR TOSTRING(_MSC_VER)
    #define FORCE_INLINE static __forceinline
#elif defined(__GNUC__)
    #define COMPILER_NAME "GCC (GNU Compiler Collection)"
    #define COMPILER_VERSION_STR __VERSION__
    #define FORCE_INLINE static inline __attribute__((always_inline))
#else
    #define COMPILER_NAME "Unknown Compiler"
    #define COMPILER_VERSION_STR "Unknown"
    #define FORCE_INLINE static inline
#endif

/* ============================================================================
 * 3. Data Structures and Global Variables
 * ============================================================================ */

typedef struct {
    HWND hWnd;
    HWND hEdit;
    char szFileName[MAX_PATH];
    BOOL bFileModified;
    HFONT hFont;
    char* pEditBuffer;     // Direct buffer access for faster operations
    int nBufferSize;
} AppContext;

static AppContext g_ctx = {0};
static char g_szTempBuffer[MAX_PATH];  // Reusable buffer to avoid allocations

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

/* ============================================================================
 * 5. Optimized Helper Functions (Force Inline for TCC)
 * ============================================================================ */

// Fast string length check without calling strlen when possible
FORCE_INLINE BOOL IsStringEmpty(const char* str)
{
    return str == NULL || str[0] == '\0';
}

// Fast filename extraction without strrchr overhead
FORCE_INLINE const char* GetFileName(const char* path)
{
    const char* p = path;
    const char* last = path;
    
    while (*p)
    {
        if (*p == '\\' || *p == '/')
            last = p + 1;
        p++;
    }
    return last;
}

// Fast buffer copy with size limit
FORCE_INLINE void SafeStrCopy(char* dest, const char* src, int maxLen)
{
    int i = 0;
    while (src[i] && i < maxLen - 1)
    {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* ============================================================================
 * 6. Entry Point: WinMain
 * ============================================================================ */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Register and create window
    if (!InitApplication(hInstance) || !InitInstance(hInstance, nCmdShow))
        return 0;
    
    // Message loop - optimized with direct access
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

/* ============================================================================
 * 7. Application Initialization
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
    
    // Initialize buffer
    g_ctx.pEditBuffer = NULL;
    g_ctx.nBufferSize = 0;
    
    ShowWindow(g_ctx.hWnd, nCmdShow);
    UpdateWindow(g_ctx.hWnd);
    
    return TRUE;
}

/* ============================================================================
 * 8. UI Creation
 * ============================================================================ */

static BOOL CreateMainMenu(HWND hWnd)
{
    HMENU hMenu, hFileMenu, hEditMenu, hHelpMenu;
    
    hMenu = CreateMenu();
    if (!hMenu) return FALSE;
    
    // File menu
    hFileMenu = CreatePopupMenu();
    if (!hFileMenu) { DestroyMenu(hMenu); return FALSE; }
    
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_NEW, "&New\tCtrl+N");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPEN, "&Open\tCtrl+O");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVE, "&Save\tCtrl+S");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVEAS, "Save &As...");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_EXIT, "E&xit");
    
    // Edit menu
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
    
    // Help menu
    hHelpMenu = CreatePopupMenu();
    if (!hHelpMenu) { DestroyMenu(hMenu); DestroyMenu(hFileMenu); DestroyMenu(hEditMenu); return FALSE; }
    
    AppendMenu(hHelpMenu, MF_STRING, ID_HELP_ABOUT, "&About");
    
    // Assemble
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "&File");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, "&Edit");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, "&Help");
    
    SetMenu(hWnd, hMenu);
    return TRUE;
}

static HFONT CreateNotepadFont(void)
{
    // Try fonts in order - TCC optimized with direct array access
    static const struct {
        const char* faceName;
        int height;
        int width;
    } fonts[] = {
        {"System", 14, 8},
        {"Fixedsys", 14, 8},
        {"Courier", 14, 8}
    };
    
    int i;
    HFONT hFont;
    
    for (i = 0; i < 3; i++)
    {
        hFont = CreateFont(
            fonts[i].height, fonts[i].width, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE,
            ANSI_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            FIXED_PITCH | FF_MODERN,
            fonts[i].faceName);
        
        if (hFont)
            return hFont;
    }
    
    return NULL;
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
 * 9. Optimized File Operations
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
    
    if (dwSize == 0)
    {
        SetWindowText(g_ctx.hEdit, "");
        success = TRUE;
    }
    else if (dwSize < MAX_FILE_SIZE)
    {
        // Allocate buffer with extra byte for null terminator
        pBuffer = (char*)malloc(dwSize + 1);
        if (pBuffer)
        {
            // Read entire file in one operation - faster than multiple reads
            if (ReadFile(hFile, pBuffer, dwSize, &dwRead, NULL) && dwRead == dwSize)
            {
                pBuffer[dwRead] = '\0';
                SetWindowText(g_ctx.hEdit, pBuffer);
                success = TRUE;
            }
            free(pBuffer);
        }
    }
    else
    {
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
    
    if (pBuffer)
    {
        GetWindowText(g_ctx.hEdit, pBuffer, len);
        dwSize = (DWORD)strlen(pBuffer);
        
        // Write in one operation
        if (WriteFile(hFile, pBuffer, dwSize, &dwWritten, NULL) && dwWritten == dwSize)
            success = TRUE;
        
        free(pBuffer);
    }
    
    CloseHandle(hFile);
    return success;
}

/* ============================================================================
 * 10. File Menu Actions
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
    
    // Use pre-allocated buffer to avoid alloca overhead
    g_szTempBuffer[0] = '\0';
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = g_szTempBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = "Open File";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = NULL;
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;
    
    if (!GetOpenFileName(&ofn))
        return FALSE;
    
    if (!ReadFileContent(hWnd, g_szTempBuffer))
    {
        MessageBox(hWnd, "Cannot open file!", "Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    SafeStrCopy(g_ctx.szFileName, g_szTempBuffer, MAX_PATH);
    g_ctx.bFileModified = FALSE;
    UpdateWindowTitle(hWnd);
    return TRUE;
}

static BOOL DoFileSave(HWND hWnd)
{
    if (IsStringEmpty(g_ctx.szFileName))
        return DoFileSaveAs(hWnd);
    
    if (!WriteFileContent(hWnd, g_ctx.szFileName))
    {
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
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.hInstance = NULL;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = g_szTempBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = "Save File";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.nFileOffset = 0;
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = "txt";
    ofn.lCustData = 0;
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;
    
    if (!GetSaveFileName(&ofn))
        return FALSE;
    
    SafeStrCopy(g_ctx.szFileName, g_szTempBuffer, MAX_PATH);
    return DoFileSave(hWnd);
}

/* ============================================================================
 * 11. Utility Functions
 * ============================================================================ */

static void UpdateWindowTitle(HWND hWnd)
{
    const char* pFileName;
    char szTitle[MAX_PATH + 32];
    
    if (IsStringEmpty(g_ctx.szFileName))
    {
        pFileName = "Untitled";
    }
    else
    {
        pFileName = GetFileName(g_ctx.szFileName);
    }
    
    // Fast string building without sprintf overhead
    char* p = szTitle;
    const char* s = pFileName;
    while (*s && p < szTitle + MAX_PATH - 16)
        *p++ = *s++;
    
    if (g_ctx.bFileModified)
        *p++ = '*';
    
    *p++ = ' ';
    *p++ = '-';
    *p++ = ' ';
    
    s = APP_NAME;
    while (*s && p < szTitle + MAX_PATH - 1)
        *p++ = *s++;
    
    *p = '\0';
    
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
    
    // Use wsprintf for faster formatting
    wsprintf(szAbout,
        "Simple Notepad\n"
        "Version 1.0\n\n"
        "Compiler Information:\n"
        "  Compiler: " COMPILER_NAME " (" COMPILER_VERSION_STR ")\n"
        "  Build Date: " __DATE__ "\n"
        "  Build Time: " __TIME__ "\n\n"
        "Developed with Win32 API\n"
        "Optimized for TCC");
    
    MessageBox(hWnd, szAbout, "About", MB_OK | MB_ICONINFORMATION);
}

/* ============================================================================
 * 12. Command Handler
 * ============================================================================ */

static void HandleCommand(HWND hWnd, int wmId)
{
    // Use switch with fall-through optimization for similar commands
    switch (wmId)
    {
    case ID_FILE_NEW:       DoFileNew(hWnd); break;
    case ID_FILE_OPEN:      DoFileOpen(hWnd); break;
    case ID_FILE_SAVE:      DoFileSave(hWnd); break;
    case ID_FILE_SAVEAS:    DoFileSaveAs(hWnd); break;
    case ID_FILE_EXIT:      PostMessage(hWnd, WM_CLOSE, 0, 0); break;
        
    // Edit commands - direct send with minimal overhead
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
 * 13. Window Procedure
 * ============================================================================ */

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        {
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            SetWindowPos(g_ctx.hEdit, NULL, 0, 0, 
                        rcClient.right, rcClient.bottom,
                        SWP_NOZORDER);
        }
        break;
        
    case WM_COMMAND:
        HandleCommand(hWnd, LOWORD(wParam));
        break;
        
    case WM_CLOSE:
        if (CheckFileModified(hWnd))
            DestroyWindow(hWnd);
        break;
        
    case WM_DESTROY:
        if (g_ctx.hFont)
        {
            DeleteObject(g_ctx.hFont);
            g_ctx.hFont = NULL;
        }
        if (g_ctx.pEditBuffer)
        {
            free(g_ctx.pEditBuffer);
            g_ctx.pEditBuffer = NULL;
        }
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    
    return 0;
}
