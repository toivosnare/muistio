#include "fileio.h"
#include "resource.h"

#include <cstdio>

#include <windows.h>
#include <commctrl.h>
#include <Richedit.h>
#include <shellapi.h>
#include <datetimeapi.h>

#define ID_EDIT 1
#define ID_STATUS 2

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

static VOID NewWindow() {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    CreateProcessW(NULL, GetCommandLineW(), NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi);
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
            case EM_REPLACESEL:
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
    AppendMenuW(hShowMenu, MF_STRING | MF_POPUP, (UINT_PTR) hZoomMenu, L"Zoomaus");
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
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hFileMenu, L"Tiedosto");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hEditMenu, L"Muokkaa");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hFormatMenu, L"Muotoile");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hShowMenu, L"Näytä");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hHelpMenu, L"Ohje");
    SetMenu(hWnd, hMenuBar);

    hWndWrapEdit = CreateWindowW(MSFTEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_EX_ZOOMABLE | ES_NOHIDESEL,
            0, 0, 0, 0, hWnd, (HMENU) ID_EDIT,
            NULL, NULL);
    SetWindowSubclass(hWndWrapEdit, EditProc, TRUE, NULL);
    hWndNoWrapEdit = CreateWindowW(MSFTEDIT_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | WS_HSCROLL | ES_EX_ZOOMABLE | ES_NOHIDESEL,
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

static VOID DateTime() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    CONST INT SIZE = 17;
    WCHAR time[SIZE];
    swprintf_s(time, SIZE, L"%u.%u %u.%u.%u", t.wHour, t.wMinute, t.wDay, t.wMonth, t.wYear);
    SendMessageW(ActiveEdit(), EM_REPLACESEL, TRUE, (LPARAM) time);
}

static VOID SelectFont(HWND hWnd) {
    CHARFORMATW format{};
    format.cbSize = sizeof(format);
    SendMessageW(ActiveEdit(), EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM) &format);
    LOGFONTW font{};
    CONST INT PIXELS_PER_INCH = GetDeviceCaps(GetDC(hWnd), LOGPIXELSY);
    CONST INT TWIPS_TO_INCH_RATIO = 72 * 20;
    font.lfHeight = -MulDiv(format.yHeight, PIXELS_PER_INCH, TWIPS_TO_INCH_RATIO);
    font.lfWeight = (format.dwEffects & CFM_BOLD) ? FW_BOLD : FW_NORMAL;
    font.lfItalic = (format.dwEffects & CFM_ITALIC) ? TRUE : FALSE;
    font.lfCharSet = format.bCharSet;
    font.lfPitchAndFamily = format.bPitchAndFamily;
    wcscpy_s(font.lfFaceName, LF_FACESIZE, format.szFaceName);

    CHOOSEFONTW cf{};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = hWnd;
    cf.lpLogFont = &font;
    cf.Flags = CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT;

    if (ChooseFontW(&cf) == TRUE) {
        format.dwMask = CFM_BOLD | CFM_CHARSET | CFM_FACE | CFM_ITALIC | CFM_SIZE;
        format.dwEffects = 0;
        if (font.lfWeight >= FW_BOLD) format.dwEffects |= CFE_BOLD;
        if (font.lfItalic) format.dwEffects |= CFE_ITALIC;
        format.yHeight = MulDiv(-font.lfHeight, TWIPS_TO_INCH_RATIO, PIXELS_PER_INCH);
        format.bCharSet = font.lfCharSet;
        format.bPitchAndFamily = font.lfPitchAndFamily;
        wcscpy_s(format.szFaceName, LF_FACESIZE, font.lfFaceName);
        SendMessageW(ActiveEdit(), EM_SETCHARFORMAT, SCF_ALL, (LPARAM) &format);
    }
}

static VOID About(HWND hWnd) {
    MSGBOXPARAMSW params{};
    params.cbSize = sizeof(params);
    params.hwndOwner = hWnd;
    params.hInstance = GetModuleHandleW(NULL);
    params.lpszText = L"Muistio (kurja versio)\nTS 2021";
    params.lpszCaption = L"Tietoja: Muistio";
    params.dwStyle = MB_USERICON;
    params.lpszIcon = MAKEINTRESOURCEW(IDI_ICON);
    MessageBoxIndirectW(&params);
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
        switch (LOWORD(wParam)) {
        case COMMAND_NEW:
            break;
        case COMMAND_NEWWINDOW:
            NewWindow();
            break;
        case COMMAND_OPEN:
            Open(hWnd);
            UpdateEncoding();
            break;
        case COMMAND_SAVE:
            break;
        case COMMAND_SAVEAS:
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
        case COMMAND_SELECTALL:
            SendMessageW(edit, EM_SETSEL, 0, -1);
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
            zoom = 100;
            UpdateZoom();
            break;
        case COMMAND_STATUSBAR:
            ToggleStatusBar(hWnd);
            break;
        case COMMAND_HELP:
            ShellExecuteW(hWnd, L"open", L"https://github.com/toivosnare/muistio", NULL, NULL, SW_SHOWNORMAL);
            break;
        case COMMAND_FEEDBACK:
            ShellExecuteW(hWnd, L"open", L"https://github.com/toivosnare/muistio/issues/new", NULL, NULL, SW_SHOWNORMAL);
            break;
        case COMMAND_ABOUT:
            About(hWnd);
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

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(hWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return msg.wParam;
}
