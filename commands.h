#include <windows.h>

VOID New(HWND);
VOID NewWindow();
VOID Open(HWND);
BOOL Save(HWND);
BOOL SaveAs(HWND);
VOID Quit(HWND);
VOID Undo();
VOID Cut();
VOID Copy();
VOID Paste();
VOID Delete();
VOID BingSearch(HWND);
VOID SelectAll();
VOID DateTime();
VOID ToggleWordWrap(HWND);
VOID SelectFont(HWND);
VOID Zoom(BOOL);
VOID ResetZoom();
VOID ToggleStatusBar(HWND);
VOID Help(HWND);
VOID Feedback(HWND);
VOID About(HWND);

LPCWSTR GetFileName();
INT GetZoom();
LPCWSTR GetEncoding();
HWND ActiveEdit();
BOOL StatusbarActivated();
BOOL SaveUnsavedChanges(HWND);
VOID Init(HWND hWnd);
