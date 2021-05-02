#include <windows.h>

#define ID_EDIT 1

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

static HWND hwndEdit;

void Create(HWND hwnd) {
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

    HMENU hMenuBar = CreateMenu();
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hFileMenu, L"Tiedosto");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR) hEditMenu, L"Muokkaa");
    SetMenu(hwnd, hMenuBar);

    hwndEdit = CreateWindowW(L"Edit", NULL, 
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU) ID_EDIT,
            NULL, NULL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        Create(hwnd);
        break;
    case WM_SIZE:
        MoveWindow(hwndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        break;
    case WM_COMMAND:
        switch (wParam) {
        case COMMAND_NEW:
            break;
        case COMMAND_NEW_WINDOW:
            break;
        case COMMAND_OPEN:
            break;
        case COMMAND_SAVE:
            break;
        case COMMAND_SAVE_AS:
            break;
        case COMMAND_QUIT:
            PostQuitMessage(0);
            break;
        case COMMAND_UNDO:
            if (SendMessageW(hwndEdit, EM_CANUNDO, 0, 0))
                SendMessageW(hwndEdit, WM_UNDO, 0, 0);
            break;
        case COMMAND_CUT:
            SendMessageW(hwndEdit, WM_CUT, 0, 0);
            break;
        case COMMAND_COPY:
            SendMessageW(hwndEdit, WM_COPY, 0, 0);
            break;
        case COMMAND_PASTE:
            SendMessageW(hwndEdit, WM_PASTE, 0, 0);
            break;
        case COMMAND_DELETE:
            SendMessageW(hwndEdit, WM_CLEAR, 0, 0);
            break;
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return NULL;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
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
    return (int) msg.wParam;
}
