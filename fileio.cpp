#include "fileio.h"
#include "resource.h"

#include <commctrl.h>

#include <unordered_map>

extern HWND hwndEdit;
extern HWND hwndStatus;

enum ENCODING {
    ANSI = 1,
    UTF8,
    UTF16LE,
};
static ENCODING encoding = UTF8;
static const std::unordered_map<ENCODING, LPCWSTR> ENCODING_CAPTIONS = {
    {ANSI, L"ANSI"},
    {UTF8, L"UTF-8"},
    {UTF16LE, L"UTF-16 LE"},
};
static const LPCWSTR FILTERS = L"Kaikki\0*.*\0Teksti\0*.txt\0";
static const int AUTO_DETECT_ENCODING = 0;
static int selectedEncoding;
static HANDLE hFile;

static UINT_PTR CALLBACK OpenProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        SendDlgItemMessageW(hwnd, IDI_ENCODING_COMBOBOX, CB_INSERTSTRING, 0, (LPARAM) L"Tunnista koodaus automaattisesti");
        for (const auto [id, caption] : ENCODING_CAPTIONS)
            SendDlgItemMessageW(hwnd, IDI_ENCODING_COMBOBOX, CB_INSERTSTRING, id, (LPARAM) caption);
        SendDlgItemMessageW(hwnd, IDI_ENCODING_COMBOBOX, CB_SETCURSEL, 0, NULL);
        return TRUE;
    }
    case WM_NOTIFY: {
        LPOFNOTIFYW notify = (LPOFNOTIFYW) lParam;
        if (notify->hdr.code == CDN_FILEOK)
            selectedEncoding = SendDlgItemMessageW(hwnd, IDI_ENCODING_COMBOBOX, CB_GETCURSEL, NULL, NULL);
        break;
    }
    }
    return FALSE;
}

void Open(HWND hwnd) {
    WCHAR szFile[MAX_PATH];
    szFile[0] = 0;
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.hInstance = GetModuleHandleW(NULL);
    ofn.lpstrFilter = FILTERS;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER | OFN_ENABLETEMPLATE;
    ofn.lpfnHook = (LPOFNHOOKPROC) OpenProc;
    ofn.lpTemplateName = MAKEINTRESOURCEW(IDI_ENCODING_DIALOG);

    if (GetOpenFileNameW(&ofn) == FALSE)
        return;

    hFile = CreateFileW(ofn.lpstrFile, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hwnd, L"Tiedoston avaaminen epäonnistui.", L"Virhe", MB_ICONERROR);
        return;
    }

    constexpr int BUFFER_SIZE = 16384;
    CHAR buffer[BUFFER_SIZE];
    DWORD numberOfBytesRead;
    BOOL result = ReadFile(hFile, buffer, BUFFER_SIZE - 1, &numberOfBytesRead, NULL);
    CloseHandle(hFile);
    if (result == FALSE) {
        MessageBoxW(hwnd, L"Tiedoston lukeminen epäonnistui.", L"Virhe", MB_ICONERROR);
        return;
    }
    if (numberOfBytesRead == BUFFER_SIZE - 1)
        MessageBoxW(hwnd, L"Tiedoston koko ylittää puskurin koon.", L"Varoitus", MB_ICONWARNING);
    buffer[numberOfBytesRead] = 0;
    BOOL autoDetect = selectedEncoding == AUTO_DETECT_ENCODING;
    if (numberOfBytesRead == 0) {
        if (autoDetect)
            MessageBoxW(hwnd, L"Tyhjän tiedoston koodauksen tunnistus epäonnistui.", L"Varoitus", MB_ICONWARNING);
        return;
    }
    
    if (autoDetect) {
        if (numberOfBytesRead > 2 && buffer[0] == '\xEF' && buffer[1] == '\xBB' && buffer[2] == '\xBF') {
            encoding = UTF8;
        } else if (numberOfBytesRead > 1 && buffer[0] == '\xFF' && buffer[1] == '\xFE') {
            if (numberOfBytesRead > 2 && buffer[2] == '\x00')
                MessageBoxW(hwnd, L"UTF-32 LE -koodausta ei tueta.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF16LE;
        } else if (numberOfBytesRead > 1 && buffer[0] == '\xFE' && buffer[1] == '\xFF') {
            MessageBoxW(hwnd, L"UTF-16 BE -koodausta ei tueta.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF8;
        } else if (numberOfBytesRead > 3 && buffer[0] == '\x00' && buffer[1] == '\x00'
                && buffer[2] == '\xFE' && buffer[3] == '\xFF') {
            MessageBoxW(hwnd, L"UTF-32 BE -koodausta ei tueta.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF8;
        } else {
            // MessageBoxW(hwnd, L"Unicode BOM -automaattitunnistus epäonnistui.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF8;
        }
    } else {
        // Evil hack >:)
        encoding = (ENCODING) selectedEncoding;
    }
    SendMessageW(hwndStatus, SB_SETTEXTW, 4, (LPARAM) ENCODING_CAPTIONS.at(encoding));

    switch (encoding) {
    case ANSI: {
        SetWindowTextA(hwndEdit, buffer);
        break;
    }
    case UTF8: {
        DWORD flags = MB_ERR_INVALID_CHARS;
        const int utf16Length = MultiByteToWideChar(CP_UTF8, flags, buffer, -1, NULL, 0);
        if (utf16Length == 0) {
            MessageBoxW(hwnd, L"Tiedosto sisältää laittomia UTF-8 merkkejä.", L"Varoitus", MB_ICONWARNING);
            return;
        }
        LPWSTR wideBuffer = new WCHAR[utf16Length];
        MultiByteToWideChar(CP_UTF8, flags, buffer, -1, wideBuffer, utf16Length);
        SetWindowTextW(hwndEdit, wideBuffer);
        delete[] wideBuffer;
        break;
    }
    case UTF16LE: {
        SetWindowTextW(hwndEdit, (LPCWSTR) buffer);
        break;
    }
    }
}

static UINT_PTR CALLBACK SaveAsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        for (const auto [id, caption] : ENCODING_CAPTIONS)
            SendDlgItemMessageW(hwnd, IDI_ENCODING_COMBOBOX, CB_INSERTSTRING, id - 1, (LPARAM) caption);
        SendDlgItemMessageW(hwnd, IDI_ENCODING_COMBOBOX, CB_SETCURSEL, encoding - 1, NULL);
        return TRUE;
    }
    case WM_NOTIFY: {
        LPOFNOTIFYW notify = (LPOFNOTIFYW) lParam;
        if (notify->hdr.code == CDN_FILEOK)
            selectedEncoding = SendDlgItemMessageW(hwnd, IDI_ENCODING_COMBOBOX, CB_GETCURSEL, NULL, NULL) + 1;
        break;
    }
    }
    return FALSE;
}

void SaveAs(HWND hwnd) {
    WCHAR szFile[MAX_PATH];
    szFile[0] = 0;
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.hInstance = GetModuleHandleW(NULL);
    ofn.lpstrFilter = FILTERS;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_ENABLEHOOK | OFN_EXPLORER | OFN_ENABLETEMPLATE;
    ofn.lpfnHook = (LPOFNHOOKPROC) SaveAsProc;
    ofn.lpTemplateName = MAKEINTRESOURCEW(IDI_ENCODING_DIALOG);

    if (GetSaveFileNameW(&ofn) == FALSE)
        return;

    hFile = CreateFileW(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hwnd, L"Tiedoston avaaminen epäonnistui.", L"Virhe", MB_ICONERROR);
        return;
    }

    encoding = (ENCODING) selectedEncoding;
    SendMessageW(hwndStatus, SB_SETTEXTW, 4, (LPARAM) ENCODING_CAPTIONS.at(encoding));

    switch (encoding) {
    case ANSI: {
        const int length = GetWindowTextLengthA(hwndEdit) + 1;
        LPSTR text = new CHAR[length];
        GetWindowTextA(hwndEdit, text, length);
        DWORD numberOfBytesWritten;
        BOOL result = WriteFile(hFile, text, length, &numberOfBytesWritten, NULL);
        if (result == FALSE || numberOfBytesWritten != length)
            MessageBoxW(hwnd, L"Tiedoston kirjoittaminen epäonnistui.", L"Virhe", MB_ICONERROR);
        delete[] text;
        break;
    }
    case UTF8: {
        const int utf16Length = GetWindowTextLengthW(hwndEdit) + 1;
        LPWSTR utf16Text = new WCHAR[utf16Length];
        GetWindowTextW(hwndEdit, utf16Text, utf16Length);
        const int flags = WC_ERR_INVALID_CHARS;
        const int utf8Length = WideCharToMultiByte(CP_UTF8, flags, utf16Text, -1, NULL, 0, NULL, NULL);
        if (utf8Length == 0) {
            MessageBoxW(hwnd, L"Tiedosto sisältää laittomia merkkejä.", L"Virhe", MB_ICONERROR);
            CloseHandle(hFile);
            delete[] utf16Text;
            return;
        }
        LPSTR utf8Text = new CHAR[utf8Length];
        WideCharToMultiByte(CP_UTF8, flags, utf16Text, -1, utf8Text, utf8Length, NULL, NULL);
        DWORD numberOfBytesWritten;
        BOOL result = WriteFile(hFile, utf8Text, utf8Length, &numberOfBytesWritten, NULL);
        if (result == FALSE || numberOfBytesWritten != utf8Length)
            MessageBoxW(hwnd, L"Tiedoston kirjoittaminen epäonnistui.", L"Virhe", MB_ICONERROR);
        delete[] utf16Text;
        delete[] utf8Text;
        break;
    }
    case UTF16LE: {
        const int length = GetWindowTextLengthW(hwndEdit) + 1;
        LPWSTR text = new WCHAR[length];
        GetWindowTextW(hwndEdit, text, length);
        DWORD numberOfBytesWritten;
        BOOL result = WriteFile(hFile, text, length * sizeof(WCHAR), &numberOfBytesWritten, NULL);
        if (result == FALSE || numberOfBytesWritten != length * sizeof(WCHAR))
            MessageBoxW(hwnd, L"Tiedoston kirjoittaminen epäonnistui.", L"Virhe", MB_ICONERROR);
        delete[] text;
        break;
    }
    }
    CloseHandle(hFile);
}