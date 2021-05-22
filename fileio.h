#include <windows.h>

enum ENCODING {
    AUTODETECT,
    ANSI,
    UTF8,
    UTF16LE
};

BOOL Read(HWND hWnd, LPCWSTR path, ENCODING &encoding);
BOOL Write(HWND hWnd, LPCWSTR path, ENCODING encoding);
