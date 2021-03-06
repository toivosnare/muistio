#include "fileio.h"
#include <new>

extern HWND ActiveEdit();

BOOL Read(HWND hWnd, LPCWSTR path, ENCODING &encoding) {
    CONST HANDLE hFile = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hWnd, L"Tiedoston avaaminen epäonnistui.", L"Virhe", MB_ICONERROR);
        return FALSE;
    }

    CONST UINT size = 16384;
    CHAR buffer[size];
    DWORD numberOfBytesRead;
    CONST BOOL result = ReadFile(hFile, buffer, size - 1, &numberOfBytesRead, NULL);
    CloseHandle(hFile);
    if (result == FALSE) {
        MessageBoxW(hWnd, L"Tiedoston lukeminen epäonnistui.", L"Virhe", MB_ICONERROR);
        return FALSE;
    }
    if (numberOfBytesRead == size - 1)
        MessageBoxW(hWnd, L"Tiedoston koko ylittää puskurin koon.", L"Varoitus", MB_ICONWARNING);
    buffer[numberOfBytesRead] = '\0';
    if (numberOfBytesRead == 0) {
        if (encoding == AUTODETECT) {
            MessageBoxW(hWnd, L"Tyhjän tiedoston koodauksen tunnistus epäonnistui.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF8;
        }
        return TRUE;
    }
    
    if (encoding == AUTODETECT) {
        if (numberOfBytesRead > 2 && buffer[0] == '\xEF' && buffer[1] == '\xBB' && buffer[2] == '\xBF') {
            encoding = UTF8;
        } else if (numberOfBytesRead > 1 && buffer[0] == '\xFF' && buffer[1] == '\xFE') {
            if (numberOfBytesRead > 2 && buffer[2] == '\x00')
                MessageBoxW(hWnd, L"UTF-32 LE -koodausta ei tueta.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF16LE;
        } else if (numberOfBytesRead > 1 && buffer[0] == '\xFE' && buffer[1] == '\xFF') {
            MessageBoxW(hWnd, L"UTF-16 BE -koodausta ei tueta.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF8;
        } else if (numberOfBytesRead > 3 && buffer[0] == '\x00' && buffer[1] == '\x00'
                && buffer[2] == '\xFE' && buffer[3] == '\xFF') {
            MessageBoxW(hWnd, L"UTF-32 BE -koodausta ei tueta.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF8;
        } else {
            // MessageBoxW(hwnd, L"Unicode BOM -automaattitunnistus epäonnistui.", L"Varoitus", MB_ICONWARNING);
            encoding = UTF8;
        }
    }
    CONST HWND edit = ActiveEdit();
    switch (encoding) {
    case ANSI: {
        SetWindowTextA(edit, buffer);
        break;
    }
    case UTF8: {
        CONST DWORD flags = MB_ERR_INVALID_CHARS;
        CONST INT utf16Length = MultiByteToWideChar(CP_UTF8, flags, buffer, -1, NULL, 0);
        if (utf16Length == 0) {
            MessageBoxW(hWnd, L"Tiedosto sisältää laittomia UTF-8 merkkejä.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        LPWSTR wideBuffer = new (std::nothrow) WCHAR[utf16Length];
        if (!wideBuffer) {
            MessageBoxW(hWnd, L"Puskurin allokointi epäonnistui.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        MultiByteToWideChar(CP_UTF8, flags, buffer, -1, wideBuffer, utf16Length);
        SetWindowTextW(edit, wideBuffer);
        delete[] wideBuffer;
        break;
    }
    case UTF16LE: {
        SetWindowTextW(edit, reinterpret_cast<LPCWSTR>(buffer));
        break;
    }
    }
    return TRUE;
}

BOOL Write(HWND hWnd, LPCWSTR path, ENCODING encoding) {
    CONST HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(hWnd, L"Tiedoston avaaminen epäonnistui.", L"Virhe", MB_ICONERROR);
        return FALSE;
    }

    CONST HWND edit = ActiveEdit();
    switch (encoding) {
    case ANSI: {
        CONST INT length = GetWindowTextLengthA(edit) + 1;
        LPSTR text = new (std::nothrow) CHAR[length];
        if (!text) {
            MessageBoxW(hWnd, L"Puskurin allokointi epäonnistui.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        GetWindowTextA(edit, text, length);
        DWORD numberOfBytesWritten;
        CONST BOOL result = WriteFile(hFile, text, length, &numberOfBytesWritten, NULL);
        delete[] text;
        CloseHandle(hFile);
        if (result == FALSE || numberOfBytesWritten != length) {
            MessageBoxW(hWnd, L"Tiedoston kirjoittaminen epäonnistui.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        break;
    }
    case UTF8: {
        CONST INT utf16Length = GetWindowTextLengthW(edit) + 1;
        LPWSTR utf16Text = new (std::nothrow) WCHAR[utf16Length];
        if (!utf16Text) {
            MessageBoxW(hWnd, L"Puskurin allokointi epäonnistui.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        GetWindowTextW(edit, utf16Text, utf16Length);
        CONST DWORD flags = WC_ERR_INVALID_CHARS;
        INT utf8Length = WideCharToMultiByte(CP_UTF8, flags, utf16Text, -1, NULL, 0, NULL, NULL);
        if (utf8Length == 0) {
            MessageBoxW(hWnd, L"Tiedosto sisältää laittomia merkkejä.", L"Virhe", MB_ICONERROR);
            CloseHandle(hFile);
            delete[] utf16Text;
            return FALSE;
        }
        // Make space for the leading BOM.
        utf8Length += 3;
        LPSTR utf8Text = new (std::nothrow) CHAR[utf8Length];
        if (!utf8Text) {
            MessageBoxW(hWnd, L"Puskurin allokointi epäonnistui.", L"Virhe", MB_ICONERROR);
            CloseHandle(hFile);
            delete[] utf16Text;
            return FALSE;
        }
        utf8Text[0] = '\xEF';
        utf8Text[1] = '\xBB';
        utf8Text[2] = '\xBF';
        WideCharToMultiByte(CP_UTF8, flags, utf16Text, -1, &utf8Text[3], utf8Length - 3, NULL, NULL);
        DWORD numberOfBytesWritten;
        CONST BOOL result = WriteFile(hFile, utf8Text, utf8Length, &numberOfBytesWritten, NULL);
        delete[] utf16Text;
        delete[] utf8Text;
        CloseHandle(hFile);
        if (result == FALSE || numberOfBytesWritten != utf8Length) {
            MessageBoxW(hWnd, L"Tiedoston kirjoittaminen epäonnistui.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        break;
    }
    case UTF16LE: {
        // Add space for the leading BOM and terminating null character.
        CONST INT length = GetWindowTextLengthW(edit) + 2;
        LPWSTR text = new (std::nothrow) WCHAR[length];
        if (!text) {
            MessageBoxW(hWnd, L"Puskurin allokointi epäonnistui.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        text[0] = L'\xFEFF';
        GetWindowTextW(edit, &text[1], length - 1);
        DWORD numberOfBytesWritten;
        CONST BOOL result = WriteFile(hFile, text, length * sizeof(WCHAR), &numberOfBytesWritten, NULL);
        delete[] text;
        CloseHandle(hFile);
        if (result == FALSE || numberOfBytesWritten != length * sizeof(WCHAR)) {
            MessageBoxW(hWnd, L"Tiedoston kirjoittaminen epäonnistui.", L"Virhe", MB_ICONERROR);
            return FALSE;
        }
        break;
    }
    }
    return TRUE;
}
