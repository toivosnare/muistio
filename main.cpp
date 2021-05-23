#include "commands.h"
#include "resource.h"
#include <cstdio>
#include <windows.h>
#include <Richedit.h>
#include <commctrl.h>

HWND hWndWrapEdit;
HWND hWndNoWrapEdit;
HWND hWndStatus;
HMENU hFormatMenu;
HMENU hShowMenu;
static CONST INT STATUS_PART_AMOUNT = 5;
static CONST INT STATUS_PART_WIDTHS[STATUS_PART_AMOUNT] = {-1, 150, 50, 150, 100};

VOID UpdateTitle(HWND hWnd) {
    CONST BOOL modified = SendMessageW(ActiveEdit(), EM_GETMODIFY, NULL, NULL);
    CONST UINT size = MAX_PATH + 12;
    WCHAR title[size];
    swprintf_s(title, size, L"%s%s - Muistio", modified ? L"*" : L"", GetFileName());
    SetWindowTextW(hWnd, title);
}

static VOID UpdatePosition() {
    DWORD caret = 0;
    CONST HWND edit = ActiveEdit();
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&caret), NULL);
    CONST LONG line = SendMessageW(edit, EM_LINEFROMCHAR, caret, NULL) + 1;
    CONST LONG start = SendMessageW(edit, EM_LINEINDEX, line - 1, NULL);
    CONST LONG column = caret - start + 1;
    CONST UINT size = 24;
    WCHAR text[size];
    swprintf_s(text, size, L"Rivi %d, Sarake %d", line, column);
    SendMessageW(hWndStatus, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(text));
}

VOID UpdateZoom() {
    CONST INT zoom = GetZoom();
    SendMessageW(ActiveEdit(), EM_SETZOOM, zoom, 100);
    CONST UINT size = 5;
    WCHAR text[size];
    swprintf_s(text, size, L"%d%%", zoom);
    SendMessageW(hWndStatus, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(text));
}

VOID UpdateEncoding() {
    SendMessageW(hWndStatus, SB_SETTEXTW, 4, reinterpret_cast<LPARAM>(GetEncoding()));
}

static LRESULT CALLBACK EditProc(HWND hWnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_MOUSEWHEEL) {
        CONST SHORT delta = GET_WHEEL_DELTA_WPARAM(wParam);
        CONST USHORT flags = GET_KEYSTATE_WPARAM(wParam);
        if (flags & MK_CONTROL)
            Zoom(delta > 0);
        return 0;
    } else if (uMsg == WM_CHAR) {
        UpdateTitle(GetParent(hWnd));
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static VOID Create(HWND hWnd) {
    HMENU hFileMenu = CreateMenu();
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_NEW, L"Uusi\tCtrl+N");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_NEWWINDOW, L"Uusi ikkuna\tCtrl+Vaihto+N");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_OPEN, L"Avaa...\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_SAVE, L"Tallenna\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, COMMAND_SAVEAS, L"Tallenna nimellä...\tCtrl+Vaihto+S");
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
    AppendMenuW(hEditMenu, MF_SEPARATOR, NULL, NULL);
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_SELECTALL, L"Valitse kaikki\tCtrl+A");
    AppendMenuW(hEditMenu, MF_STRING, COMMAND_DATETIME, L"Aika ja päivämäärä\tF5");

    hFormatMenu = CreateMenu();
    AppendMenuW(hFormatMenu, MF_STRING, COMMAND_WORDWRAP, L"Automaattinen rivitys");
    AppendMenuW(hFormatMenu, MF_STRING, COMMAND_SELECTFONT, L"Fontti...");
    CheckMenuItem(hFormatMenu, COMMAND_WORDWRAP, MF_CHECKED);

    hShowMenu = CreateMenu();
    HMENU hZoomMenu = CreatePopupMenu();
    AppendMenuW(hShowMenu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(hZoomMenu), L"Zoomaus");
    AppendMenuW(hZoomMenu, MF_STRING, COMMAND_ZOOMIN, L"Lähennä\tCtrl++");
    AppendMenuW(hZoomMenu, MF_STRING, COMMAND_ZOOMOUT, L"Loitonna\tCtrl+-");
    AppendMenuW(hZoomMenu, MF_STRING, COMMAND_ZOOMRESET, L"Palauta oletuszoomaus\tCtrl+0");
    AppendMenuW(hShowMenu, MF_STRING, COMMAND_STATUSBAR, L"Tilarivi");
    CheckMenuItem(hShowMenu, COMMAND_STATUSBAR, MF_CHECKED);

    HMENU hHelpMenu = CreateMenu();
    AppendMenuW(hHelpMenu, MF_STRING, COMMAND_HELP, L"Näytä ohje");
    AppendMenuW(hHelpMenu, MF_STRING, COMMAND_FEEDBACK, L"Lähetä palautetta");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, NULL, NULL);
    AppendMenuW(hHelpMenu, MF_STRING, COMMAND_ABOUT, L"Tietoja muistiosta");

    HMENU hMenuBar = CreateMenu();
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), L"Tiedosto");
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hEditMenu), L"Muokkaa");
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFormatMenu), L"Muotoile");
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hShowMenu), L"Näytä");
    AppendMenuW(hMenuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelpMenu), L"Ohje");
    SetMenu(hWnd, hMenuBar);

    hWndWrapEdit = CreateWindowW(MSFTEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_NOHIDESEL,
            0, 0, 0, 0, hWnd, 0,
            NULL, NULL);
    SetWindowSubclass(hWndWrapEdit, EditProc, TRUE, NULL);
    hWndNoWrapEdit = CreateWindowW(MSFTEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_NOHIDESEL | WS_HSCROLL,
            0, 0, 0, 0, hWnd, 0,
            NULL, NULL);
    SetWindowSubclass(hWndNoWrapEdit, EditProc, FALSE, NULL);
    ShowWindow(hWndNoWrapEdit, SW_HIDE);

    hWndStatus = CreateWindowW(STATUSCLASSNAMEW, NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hWnd, 0,
            NULL, NULL);
    Init(hWnd);
}

VOID Resize(LONG width, LONG height) {
    if (StatusbarActivated()) {
        INT statusPartEdges[STATUS_PART_AMOUNT];
        INT x = width;
        for (INT i = STATUS_PART_AMOUNT - 1; i > -1; --i) {
            statusPartEdges[i] = x;
            x -= STATUS_PART_WIDTHS[i];
        }
        SendMessageW(hWndStatus, SB_SETPARTS, STATUS_PART_AMOUNT, reinterpret_cast<LPARAM>(statusPartEdges));
        UpdatePosition();
        UpdateZoom();
        SendMessageW(hWndStatus, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(L"Windows (CRLF)"));
        UpdateEncoding();

        SendMessageW(hWndStatus, WM_SIZE, 0, 0);
        RECT rect;
        GetWindowRect(hWndStatus, &rect);
        CONST LONG statusHeight = rect.bottom - rect.top;
        height -= statusHeight;
    }
    MoveWindow(ActiveEdit(), 0, 0, width, height, TRUE);
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
        switch (LOWORD(wParam)) {
        case COMMAND_NEW:
            New(hWnd);
            break;
        case COMMAND_NEWWINDOW:
            NewWindow();
            break;
        case COMMAND_OPEN:
            Open(hWnd);
            break;
        case COMMAND_SAVE:
            Save(hWnd);
            break;
        case COMMAND_SAVEAS:
            SaveAs(hWnd);
            break;
        case COMMAND_QUIT:
            Quit(hWnd);
            break;
        case COMMAND_UNDO:
            Undo();
            break;
        case COMMAND_CUT:
            Cut();
            break;
        case COMMAND_COPY:
            Copy();
            break;
        case COMMAND_PASTE:
            Paste();
            break;
        case COMMAND_DELETE:
            Delete();
            break;
        case COMMAND_BINGSEARCH:
            BingSearch(hWnd);
            break;
        case COMMAND_SELECTALL:
            SelectAll();
            break;
        case COMMAND_DATETIME:
            DateTime();
            break;
        case COMMAND_WORDWRAP:
            ToggleWordWrap(hWnd);
            break;
        case COMMAND_SELECTFONT:
            SelectFont(hWnd);
            break;
        case COMMAND_ZOOMIN:
            Zoom(TRUE);
            break;
        case COMMAND_ZOOMOUT:
            Zoom(FALSE);
            break;
        case COMMAND_ZOOMRESET:
            ResetZoom();
            break;
        case COMMAND_STATUSBAR:
            ToggleStatusBar(hWnd);
            break;
        case COMMAND_HELP:
            Help(hWnd);
            break;
        case COMMAND_FEEDBACK:
            Feedback(hWnd);
            break;
        case COMMAND_ABOUT:
            About(hWnd);
            break;
        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }
        break;
    }
    case WM_CLOSE:
        if (!SaveUnsavedChanges(hWnd))
            break;
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

static INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    HMODULE module = LoadLibraryW(L"msftedit.dll");
    if (module == NULL) {
        MessageBoxW(NULL, L"DLL-moduulien lataaminen epäonnistui", L"Virhe", MB_ICONERROR);
        return 1;
    }
    HACCEL hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDI_ACCEL));

    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszClassName = L"Muistio";
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(wc.lpszClassName, L"Muistio",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            220, 220, 640, 480, 0, 0, hInstance, 0);

    MSG msg{};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return msg.wParam;
}
