#include "commands.h"
#include "fileio.h"
#include "resource.h"
#include <cstdio>
#include <unordered_map>
#include <commctrl.h>
#include <Richedit.h>
#include <shellapi.h>
#include <datetimeapi.h>

extern VOID UpdateTitle(HWND);
extern VOID UpdateZoom();
extern VOID UpdateEncoding();
extern VOID Resize(LONG, LONG);
extern HMENU hFormatMenu;
extern HMENU hShowMenu;
extern HWND hWndWrapEdit;
extern HWND hWndNoWrapEdit;
extern HWND hWndStatus;

static CONST std::unordered_map<ENCODING, LPCWSTR> ENCODING_CAPTIONS = {
    {AUTODETECT, L"Tunnista koodaus automaattisesti"},
    {ANSI, L"ANSI"},
    {UTF8, L"UTF-8"},
    {UTF16LE, L"UTF-16 LE"},
};
static ENCODING encoding = UTF8;
static LPCWSTR FILTERS = L"Kaikki\0*.*\0Teksti\0*.txt\0";
static WCHAR openFile[MAX_PATH] = {0};
static INT zoom = 100;
static BOOL wrap = TRUE;
static BOOL statusBar = TRUE;

VOID New(HWND hWnd) {
    if (!SaveUnsavedChanges(hWnd))
        return;
    openFile[0] = '\0';
    SetWindowTextW(ActiveEdit(), L"");
    UpdateTitle(hWnd);
}

VOID NewWindow() {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    CreateProcessW(NULL, GetCommandLineW(), NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi);
}

static UINT_PTR CALLBACK OpenProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        for (CONST auto [id, caption] : ENCODING_CAPTIONS)
            SendDlgItemMessageW(hWnd, IDI_ENCODING_COMBOBOX, CB_INSERTSTRING, id, reinterpret_cast<LPARAM>(caption));
        SendDlgItemMessageW(hWnd, IDI_ENCODING_COMBOBOX, CB_SETCURSEL, 0, NULL);
        return TRUE;
    }
    case WM_NOTIFY: {
        CONST LPOFNOTIFYW notify = reinterpret_cast<LPOFNOTIFYW>(lParam);
        if (notify->hdr.code == CDN_FILEOK)
            encoding = static_cast<ENCODING>(SendDlgItemMessageW(hWnd, IDI_ENCODING_COMBOBOX, CB_GETCURSEL, NULL, NULL));
        break;
    }
    }
    return FALSE;
}

VOID Open(HWND hWnd) {
    if (!SaveUnsavedChanges(hWnd))
        return;

    WCHAR szFile[MAX_PATH];
    szFile[0] = '\0';
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.hInstance = GetModuleHandleW(NULL);
    ofn.lpstrFilter = FILTERS;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER | OFN_ENABLETEMPLATE;
    ofn.lpfnHook = static_cast<LPOFNHOOKPROC>(OpenProc);
    ofn.lpTemplateName = MAKEINTRESOURCEW(IDI_ENCODING_DIALOG);

    if (GetOpenFileNameW(&ofn) == FALSE)
        return;
    if (Read(hWnd, ofn.lpstrFile, encoding)) {
        wcscpy_s(openFile, MAX_PATH, ofn.lpstrFile);
        UpdateEncoding();
        UpdateTitle(hWnd);
    }
}

VOID Save(HWND hWnd) {
    if (openFile[0] == '\0') {
        SaveAs(hWnd);
        return;
    }
    if (Write(hWnd, openFile, encoding)) {
        SendMessageW(ActiveEdit(), EM_SETMODIFY, FALSE, NULL);
        UpdateTitle(hWnd);
    }
}

static UINT_PTR CALLBACK SaveAsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        for (INT i = 0; i < 3; ++i) {
            LPCWSTR caption = ENCODING_CAPTIONS.at(static_cast<ENCODING>(i + 1));
            SendDlgItemMessageW(hWnd, IDI_ENCODING_COMBOBOX, CB_INSERTSTRING, i, reinterpret_cast<LPARAM>(caption));
        }
        SendDlgItemMessageW(hWnd, IDI_ENCODING_COMBOBOX, CB_SETCURSEL, encoding - 1, NULL);
        return TRUE;
    }
    case WM_NOTIFY: {
        CONST LPOFNOTIFYW notify = reinterpret_cast<LPOFNOTIFYW>(lParam);
        if (notify->hdr.code == CDN_FILEOK)
            encoding = static_cast<ENCODING>(SendDlgItemMessageW(hWnd, IDI_ENCODING_COMBOBOX, CB_GETCURSEL, NULL, NULL) + 1);
        break;
    }
    }
    return FALSE;
}

VOID SaveAs(HWND hWnd) {
    WCHAR szFile[MAX_PATH];
    szFile[0] = '\0';
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.hInstance = GetModuleHandleW(NULL);
    ofn.lpstrFilter = FILTERS;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER | OFN_ENABLETEMPLATE;
    ofn.lpfnHook = static_cast<LPOFNHOOKPROC>(SaveAsProc);
    ofn.lpTemplateName = MAKEINTRESOURCEW(IDI_ENCODING_DIALOG);

    if (GetSaveFileNameW(&ofn) == FALSE)
        return;
    if (Write(hWnd, ofn.lpstrFile, encoding)) {
        wcscpy_s(openFile, MAX_PATH, ofn.lpstrFile);
        SendMessageW(ActiveEdit(), EM_SETMODIFY, FALSE, NULL);
        UpdateEncoding();
        UpdateTitle(hWnd);
    }
}

VOID Quit(HWND hWnd) {
    SendMessageW(hWnd, WM_CLOSE, NULL, NULL);
}

VOID Undo() {
    CONST HWND edit = ActiveEdit();
    if (SendMessageW(edit, EM_CANUNDO, NULL, NULL))
        SendMessageW(edit, WM_UNDO, NULL, NULL);
}

VOID Cut() {
    SendMessageW(ActiveEdit(), WM_CUT, NULL, NULL);
}

VOID Copy() {
    SendMessageW(ActiveEdit(), WM_COPY, NULL, NULL);
}

VOID Paste() {
    SendMessageW(ActiveEdit(), WM_PASTE, NULL, NULL);
}

VOID Delete() {
    SendMessageW(ActiveEdit(), WM_CLEAR, NULL, NULL);
}

VOID BingSearch(HWND hWnd) {
    CONST HWND edit = ActiveEdit();
    DWORD start, end;
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    CONST DWORD selectionSize = end - start;
    if (selectionSize == 0)
        return;
    LPCWSTR selection = new WCHAR[selectionSize + 1];
    SendMessageW(edit, EM_GETSELTEXT, NULL, reinterpret_cast<LPARAM>(selection));
    
    CONST DWORD urlSize = selectionSize + 31;
    LPWSTR url = new WCHAR[urlSize];
    swprintf_s(url, urlSize, L"https://www.bing.com/search?q=%s", selection);
    ShellExecuteW(hWnd, L"open", url, NULL, NULL, SW_SHOWNORMAL);
    delete[] selection;
    delete[] url;
}

VOID SelectAll() {
    SendMessageW(ActiveEdit(), EM_SETSEL, 0, -1);
}

VOID DateTime() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    CONST INT SIZE = 17;
    WCHAR time[SIZE];
    swprintf_s(time, SIZE, L"%u.%u %u.%u.%u", t.wHour, t.wMinute, t.wDay, t.wMonth, t.wYear);
    SendMessageW(ActiveEdit(), EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(time));
}

VOID ToggleWordWrap(HWND hWnd) {
    RECT rect;
    CONST UINT state = GetMenuState(hFormatMenu, COMMAND_WORDWRAP, MF_BYCOMMAND);
    if (state == MF_CHECKED) {
        CheckMenuItem(hFormatMenu, COMMAND_WORDWRAP, MF_UNCHECKED);
        wrap = FALSE;
        GetWindowRect(hWndWrapEdit, &rect);
        MoveWindow(hWndNoWrapEdit, 0, 0, rect.right - rect.left, rect.bottom - rect.top, TRUE);
        CONST INT LENGTH = GetWindowTextLengthW(hWndWrapEdit) + 1;
        LPWSTR text = new WCHAR[LENGTH];
        GetWindowTextW(hWndWrapEdit, text, LENGTH);
        SetWindowTextW(hWndNoWrapEdit, text);
        delete[] text;
        DWORD d0, d1;
        SendMessageW(hWndWrapEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&d0), reinterpret_cast<LPARAM>(&d1));
        SendMessageW(hWndNoWrapEdit, EM_SETSEL, d0, d1);
        CONST BOOL modified = SendMessageW(hWndWrapEdit, EM_GETMODIFY, NULL, NULL);
        SendMessageW(hWndNoWrapEdit, EM_SETMODIFY, modified, NULL);
        SendMessageW(hWndWrapEdit, EM_GETZOOM, reinterpret_cast<WPARAM>(&d0), reinterpret_cast<LPARAM>(&d1));
        SendMessageW(hWndNoWrapEdit, EM_SETZOOM, d0, d1);
        CHARFORMATW format;
        format.cbSize = sizeof(format);
        SendMessageW(hWndWrapEdit, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
        SendMessageW(hWndNoWrapEdit, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
        ShowWindow(hWndWrapEdit, SW_HIDE);
        ShowWindow(hWndNoWrapEdit, SW_SHOW);
    } else {
        CheckMenuItem(hFormatMenu, COMMAND_WORDWRAP, MF_CHECKED);
        wrap = TRUE;
        GetWindowRect(hWndNoWrapEdit, &rect);
        MoveWindow(hWndWrapEdit, 0, 0, rect.right - rect.left, rect.bottom - rect.top, TRUE);
        CONST INT LENGTH = GetWindowTextLengthW(hWndNoWrapEdit) + 1;
        LPWSTR text = new WCHAR[LENGTH];
        GetWindowTextW(hWndNoWrapEdit, text, LENGTH);
        SetWindowTextW(hWndWrapEdit, text);
        delete[] text;
        DWORD d0, d1;
        SendMessageW(hWndNoWrapEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&d0), reinterpret_cast<LPARAM>(&d1));
        SendMessageW(hWndWrapEdit, EM_SETSEL, d0, d1);
        CONST BOOL modified = SendMessageW(hWndNoWrapEdit, EM_GETMODIFY, NULL, NULL);
        SendMessageW(hWndWrapEdit, EM_SETMODIFY, modified, NULL);
        SendMessageW(hWndNoWrapEdit, EM_GETZOOM, reinterpret_cast<WPARAM>(&d0), reinterpret_cast<LPARAM>(&d1));
        SendMessageW(hWndWrapEdit, EM_SETZOOM, d0, d1);
        CHARFORMATW format;
        format.cbSize = sizeof(format);
        SendMessageW(hWndNoWrapEdit, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
        SendMessageW(hWndWrapEdit, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
        ShowWindow(hWndNoWrapEdit, SW_HIDE);
        ShowWindow(hWndWrapEdit, SW_SHOW);
    }
}

VOID SelectFont(HWND hWnd) {
    CHARFORMATW format{};
    format.cbSize = sizeof(format);
    SendMessageW(ActiveEdit(), EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
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
        SendMessageW(ActiveEdit(), EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
    }
}

VOID Zoom(BOOL up) {
    if (up && zoom < 500)
        zoom += 10;
    else if (!up && zoom > 10)
        zoom -= 10;
    else
        return;
    UpdateZoom();
}

VOID ResetZoom() {
    zoom = 100;
    UpdateZoom();
}

VOID ToggleStatusBar(HWND hWnd) {
    CONST UINT state = GetMenuState(hShowMenu, COMMAND_STATUSBAR, MF_BYCOMMAND);
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

VOID Help(HWND hWnd) {
    ShellExecuteW(hWnd, L"open", L"https://github.com/toivosnare/muistio", NULL, NULL, SW_SHOWNORMAL);
}

VOID Feedback(HWND hWnd) {
    ShellExecuteW(hWnd, L"open", L"https://github.com/toivosnare/muistio/issues/new", NULL, NULL, SW_SHOWNORMAL);
}

VOID About(HWND hWnd) {
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

LPCWSTR GetFileName() {
    return openFile[0] != '\0' ? openFile : L"Nimetön";
}

INT GetZoom() {
    return zoom;
}

LPCWSTR GetEncoding() {
    return ENCODING_CAPTIONS.at(encoding);
}

HWND ActiveEdit() {
    return wrap ? hWndWrapEdit : hWndNoWrapEdit;
}

BOOL StatusbarActivated() {
    return statusBar;
}

BOOL SaveUnsavedChanges(HWND hWnd) {
    CONST BOOL modified = SendMessageW(ActiveEdit(), EM_GETMODIFY, NULL, NULL);
    if (modified) {
        CONST INT SIZE = 100;
        WCHAR text[SIZE];
        swprintf_s(text, SIZE, L"Haluatko tallentaa kohteeseen %s tehdyt muutokset?", GetFileName());
        CONST INT result = MessageBoxW(hWnd, text, L"Muistio", MB_YESNOCANCEL);
        if (result == IDCANCEL)
            return FALSE;
        else if (result == IDYES)
            Save(hWnd);
    }
    return TRUE;
}

VOID Init(HWND hWnd) {
    CONST LPWSTR commandLine = GetCommandLineW();
    INT argc;
    CONST LPWSTR *argv = CommandLineToArgvW(commandLine, &argc);
    if (argc > 2) {
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        for (INT i = 2; i < argc; ++i) {
            CONST UINT SIZE = wcslen(argv[0]) + wcslen(argv[i]) + 2;
            LPWSTR command = new WCHAR[SIZE];
            swprintf_s(command, SIZE, L"%s %s", argv[0], argv[i]);
            CreateProcessW(NULL, command, NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi);
            delete[] command;
        }
    }
    if (argc > 1 && Read(hWnd, argv[1], encoding)) {
        wcscpy_s(openFile, MAX_PATH, argv[1]);
        UpdateEncoding();
    }
    UpdateTitle(hWnd);
}
