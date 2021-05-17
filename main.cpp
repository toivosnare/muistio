#include "fileio.h"

#include <cstdio>

#include <windows.h>
#include <commctrl.h>
#include <Richedit.h>
#include <shellapi.h>

#define ID_EDIT 1
#define ID_STATUS 2

// Commands IDs
#define COMMAND_NEW 1
#define COMMAND_NEW_WINDOW 2
#define COMMAND_OPEN 3
#define COMMAND_SAVE 4
#define COMMAND_SAVE_AS 5
#define COMMAND_QUIT 6

#define COMMAND_UNDO 7
#define COMMAND_CUT 8
#define COMMAND_COPY 9
#define COMMAND_PASTE 10
#define COMMAND_DELETE 11
#define COMMAND_BINGSEARCH 12

#define COMMAND_WORDWRAP 19

#define COMMAND_ZOOMIN 21
#define COMMAND_ZOOMOUT 22
#define COMMAND_ZOOMRESET 23
#define COMMAND_STATUSBAR 24

static HWND hWndWrapEdit;
static HWND hWndNoWrapEdit;
static HWND hWndStatus;
static HMENU hFormatMenu;
static HMENU hShowMenu;
static BOOL wrap = TRUE;
static INT zoom = 100;
static BOOL statusBar = TRUE;
static CONST INT STATUS_PART_AMOUNT = 5;
static CONST INT STATUS_PART_WIDTHS[STATUS_PART_AMOUNT] = {-1, 150, 50, 150, 100};

HWND ActiveEdit() {
    return wrap ? hWndWrapEdit : hWndNoWrapEdit;
}

static VOID UpdatePosition() {
    DWORD caret;
    HWND edit = ActiveEdit();
    SendMessageW(edit, EM_GETSEL, (WPARAM) &caret, NULL);
    LONG line = SendMessageW(edit, EM_LINEFROMCHAR, caret, NULL) + 1;
    LONG start = SendMessageW(edit, EM_LINEINDEX, line - 1, NULL);
    LONG column = caret - start + 1;
    CONST INT SIZE = 24;
    WCHAR text[SIZE];
    swprintf_s(text, SIZE, L"Rivi %d, Sarake %d", line, column);
    SendMessageW(hWndStatus, SB_SETTEXTW, 1, (LPARAM) text);
}

static VOID UpdateZoom() {
    SendMessageW(hWndWrapEdit, EM_SETZOOM, zoom, 100);
    SendMessageW(hWndNoWrapEdit, EM_SETZOOM, zoom, 100);
    CONST INT SIZE = 5;
    WCHAR text[SIZE];
    swprintf_s(text, SIZE, L"%d%%", zoom);
    SendMessageW(hWndStatus, SB_SETTEXTW, 2, (LPARAM) text);
}

static VOID UpdateEncoding() {
    SendMessageW(hWndStatus, SB_SETTEXTW, 4, (LPARAM) GetEncoding());
}

static VOID Zoom(BOOL up) {
    if (up && zoom < 500) {
        zoom += 10;
    } else if (!up && zoom > 10) {
        zoom -= 10;
    } else {
        return;
    }
    UpdateZoom();
}

static LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uIdSubclass == wrap) {
        switch (uMsg) {
            case WM_MOUSEWHEEL: {
                CONST SHORT delta = GET_WHEEL_DELTA_WPARAM(wParam);
                CONST USHORT flags = GET_KEYSTATE_WPARAM(wParam);
                if (flags & MK_CONTROL) {
                    Zoom(delta > 0);
                    return 0;
                }
            }
            case WM_SETCURSOR:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_SYSCHAR:
            case WM_SETTEXT:
                SendMessageW(wrap ? hWndNoWrapEdit : hWndWrapEdit, uMsg, wParam, lParam);
                break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static VOID Create(HWND hWnd) {
    HMENU hFileMenu = CreateMenu();
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_NEW, L"Uusi\tCtrl+N");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_NEW_WINDOW, L"Uusi ikkuna\tCtrl+Vaihto+N");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_OPEN, L"Avaa...\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_SAVE, L"Tallenna\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_SAVE_AS, L"Tallenna nimellä...\tCtrl+Vaihto+S");
    AppendMenuW(hFileMenu, MF_SEPARATOR, NULL, NULL);
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_QUIT, L"Lopeta");

    HMENU hEditMenu = CreateMenu();
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_UNDO, L"Kumoa\tCtrl+Z");
    AppendMenuW(hEditMenu, MF_SEPARATOR, NULL, NULL);
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_CUT, L"Leikkaa\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_COPY, L"Kopioi\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_PASTE, L"Liitä\tCtrl+V");
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_DELETE, L"Poista\tDel");
    AppendMenuW(hEditMenu, MF_SEPARATOR, NULL, NULL);
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_BINGSEARCH, L"Bing-haku...\tCtrl+E");

    hFormatMenu = CreateMenu();
    AppendMenuW(hFormatMenu, MF_STRING, COMMAND_WORDWRAP, L"Automaattinen rivitys");
    CheckMenuItem(hFormatMenu, COMMAND_WORDWRAP, MF_CHECKED);

    hShowMenu = CreateMenu();
    HMENU hZoomMenu = CreatePopupMenu();
    AppendMenuW(hShowMenu, MF_STRING | MF_POPUP, (UINT_PTR) hZoomMenu, L"Zoomaus");
    AppendMenuW(hZoomMenu, MF_STRING, COMMAND_ZOOMIN, L"Lähennä\tCtrl++");
    AppendMenuW(hZoomMenu, MF_STRING, COMMAND_ZOOMOUT, L"Loitonna\tCtrl+-");
    AppendMenuW(hZoomMenu, MF_STRING, COMMAND_ZOOMRESET, L"Palauta oletuszoomaus\tCtrl+0");
    AppendMenuW(hShowMenu, MF_STRING, COMMAND_STATUSBAR, L"Tilarivi");
    CheckMenuItem(hShowMenu, COMMAND_STATUSBAR, MF_CHECKED);

    HMENU hMenuBar = CreateMenu();
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hFileMenu, L"Tiedosto");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hEditMenu, L"Muokkaa");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hFormatMenu, L"Muotoile");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hShowMenu, L"Näytä");
    SetMenu(hWnd, hMenuBar);

    hWndWrapEdit = CreateWindowW(MSFTEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_EX_ZOOMABLE,
            0, 0, 0, 0, hWnd, (HMENU) ID_EDIT,
            NULL, NULL);
    SetWindowSubclass(hWndWrapEdit, EditProc, TRUE, NULL);
    hWndNoWrapEdit = CreateWindowW(MSFTEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | WS_HSCROLL | ES_EX_ZOOMABLE,
            0, 0, 0, 0, hWnd, (HMENU) ID_EDIT,
            NULL, NULL);
    SetWindowSubclass(hWndNoWrapEdit, EditProc, FALSE, NULL);
    ShowWindow(hWndNoWrapEdit, SW_HIDE);

    hWndStatus = CreateWindowW(STATUSCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hWnd, (HMENU) ID_STATUS,
            NULL, NULL);
}

static VOID Resize(LONG width, LONG height) {
    if (statusBar) {
        INT statusPartEdges[STATUS_PART_AMOUNT];
        INT x = width;
        for (INT i = STATUS_PART_AMOUNT - 1; i > -1; --i) {
            statusPartEdges[i] = x;
            x -= STATUS_PART_WIDTHS[i];
        }
        SendMessageW(hWndStatus, SB_SETPARTS, STATUS_PART_AMOUNT, (LPARAM) statusPartEdges);
        UpdatePosition();
        UpdateZoom();
        SendMessageW(hWndStatus, SB_SETTEXTW, 3, (LPARAM) L"Windows (CRLF)");
        UpdateEncoding();

        SendMessageW(hWndStatus, WM_SIZE, 0, 0);
        RECT rect;
        GetWindowRect(hWndStatus, &rect);
        CONST LONG statusHeight = rect.bottom - rect.top;
        height -= statusHeight;
    }
    MoveWindow(ActiveEdit(), 0, 0, width, height, TRUE);
}

static VOID ToggleWordWrap(HWND hWnd) {
    RECT rect;
    UINT state = GetMenuState(hFormatMenu, COMMAND_WORDWRAP, MF_BYCOMMAND);
    if (state == MF_CHECKED) {
        CheckMenuItem(hFormatMenu, COMMAND_WORDWRAP, MF_UNCHECKED);
        wrap = FALSE;
        GetWindowRect(hWndWrapEdit, &rect);
        ShowWindow(hWndWrapEdit, SW_HIDE);
        ShowWindow(hWndNoWrapEdit, SW_SHOW);
        MoveWindow(hWndNoWrapEdit, 0, 0, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    } else {
        CheckMenuItem(hFormatMenu, COMMAND_WORDWRAP, MF_CHECKED);
        wrap = TRUE;
        GetWindowRect(hWndNoWrapEdit, &rect);
        ShowWindow(hWndNoWrapEdit, SW_HIDE);
        ShowWindow(hWndWrapEdit, SW_SHOW);
        MoveWindow(hWndWrapEdit, 0, 0, rect.right - rect.left, rect.bottom - rect.top, TRUE);
    }
}

static VOID ToggleStatusBar(HWND hWnd) {
    UINT state = GetMenuState(hShowMenu, COMMAND_STATUSBAR, MF_BYCOMMAND);
    if (state == MF_CHECKED) {
        CheckMenuItem(hShowMenu, COMMAND_STATUSBAR, MF_UNCHECKED);
        statusBar = FALSE;
        ShowWindow(hWndStatus, SW_HIDE);
    } else {
        CheckMenuItem(hShowMenu, COMMAND_STATUSBAR, MF_CHECKED);
        statusBar = TRUE;
        ShowWindow(hWndStatus, SW_SHOW);
    }

    RECT rect;
    GetClientRect(hWnd, &rect);
    Resize(rect.right - rect.left, rect.bottom - rect.top);
}

static VOID BingSearch(HWND hWnd) {
    HWND edit = ActiveEdit();
    DWORD start, end;
    SendMessageW(edit, EM_GETSEL, (WPARAM) &start, (LPARAM) &end);
    CONST DWORD selectionSize = end - start;
    if (selectionSize == 0)
        return;
    LPCWSTR selection = new WCHAR[selectionSize + 1];
    SendMessageW(edit, EM_GETSELTEXT, NULL, (LPARAM) selection);
    
    CONST DWORD urlSize = selectionSize + 31;
    LPWSTR url = new WCHAR[urlSize];
    swprintf_s(url, urlSize, L"https://www.bing.com/search?q=%s", selection);
    ShellExecuteW(hWnd, L"open", url, NULL, NULL, SW_SHOWNORMAL);
    delete[] selection;
    delete[] url;
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        Create(hWnd);
        break;
    case WM_SIZE: {
        Resize(LOWORD(lParam), HIWORD(lParam));
        break;
    }
    case WM_COMMAND: {
        if (HIWORD(wParam) == EN_UPDATE) {
            UpdatePosition();
            break;
        }
        HWND edit = ActiveEdit();
        switch (wParam) {
        case COMMAND_NEW:
            break;
        case COMMAND_NEW_WINDOW:
            break;
        case COMMAND_OPEN:
            Open(hWnd);
            UpdateEncoding();
            break;
        case COMMAND_SAVE:
            break;
        case COMMAND_SAVE_AS:
            SaveAs(hWnd);
            UpdateEncoding();
            break;
        case COMMAND_QUIT:
            PostQuitMessage(0);
            break;
        case COMMAND_UNDO:
            if (SendMessageW(edit, EM_CANUNDO, 0, 0))
                SendMessageW(edit, WM_UNDO, 0, 0);
            break;
        case COMMAND_CUT:
            SendMessageW(edit, WM_CUT, 0, 0);
            break;
        case COMMAND_COPY:
            SendMessageW(edit, WM_COPY, 0, 0);
            break;
        case COMMAND_PASTE:
            SendMessageW(edit, WM_PASTE, 0, 0);
            break;
        case COMMAND_DELETE:
            SendMessageW(edit, WM_CLEAR, 0, 0);
            break;
        case COMMAND_BINGSEARCH:
            BingSearch(hWnd);
            break;
        case COMMAND_WORDWRAP:
            ToggleWordWrap(hWnd);
            break;
        case COMMAND_ZOOMIN:
            Zoom(TRUE);
            break;
        case COMMAND_ZOOMOUT:
            Zoom(FALSE);
            break;
        case COMMAND_ZOOMRESET:
            zoom = 100;
            UpdateZoom();
            break;
        case COMMAND_STATUSBAR:
            ToggleStatusBar(hWnd);
            break;
        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return NULL;
}

static INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    HMODULE module = LoadLibraryW(L"msftedit.dll");
    if (module == NULL) {
        MessageBoxW(NULL, L"DLL-moduulien lataaminen epäonnistui", L"Virhe", MB_ICONERROR);
        return 1;
    }

    WNDCLASSW wc = {};
    wc.lpszClassName = L"Muistio";
    wc.hInstance     = hInstance;
    wc.lpfnWndProc   = WindowProc;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    RegisterClassW(&wc);

    CreateWindowW(wc.lpszClassName, L"Muistio",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            220, 220, 640, 480, 0, 0, hInstance, 0);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return msg.wParam;
}
