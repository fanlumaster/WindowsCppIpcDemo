#include "main.h"
#include "fanylog.h"
#include "my_webview.h"
#include <dwmapi.h>
#include <handleapi.h>
#include <intsafe.h>
#include <stdlib.h>
#include <synchapi.h>
#include <tchar.h>
#include <windows.h>
#include <winnt.h>
#include <winuser.h>
#include <wrl.h>
#include <wrl/client.h>

#pragma comment(lib, "dwmapi.lib")

using namespace Microsoft::WRL;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam)
{

    switch (message)
    {
    case WM_ERASEBKGND: { // Make the background dark
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);

        HBRUSH darkBrush = CreateSolidBrush(RGB(32, 32, 32));
        FillRect(hdc, &rc, darkBrush);
        DeleteObject(darkBrush);
        return 1;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance,
                     _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // Set DPI awareness to per-monitor awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    InitLog();

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, _T("Call to RegisterClassEx failed!"),
                   _T("Windows Desktop Guided Tour"), NULL);

        return 1;
    }

    HWND hWnd = CreateWindowEx(0,                    //
                               szWindowClass,        //
                               szWindowName,         //
                               WS_OVERLAPPEDWINDOW,  //
                               100,                  //
                               100,                  //
                               (108 + 15) * 1.5 * 6, //
                               (246 + 15) * 1.5 * 2, //
                               nullptr,              //
                               nullptr,              //
                               hInstance,            //
                               nullptr);             //

    if (!hWnd)
    {
        MessageBox(NULL,                            //
                   L"Call to CreateWindow failed!", //
                   L"Windows Desktop Guided Tour",  //
                   NULL);                           //
        return 1;
    }

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(             //
        hWnd,                          //
        DWMWA_USE_IMMERSIVE_DARK_MODE, //
        &useDarkMode,                  //
        sizeof(useDarkMode)            //
    );

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    PrepareWindowHtml();
    InitWebview(hWnd);

    // shared memory
    const wchar_t *sharedName =
        L"Local\\MySharedMemory"; // 使用 Local 避免权限问题
    const int bufferSize = 1024;

    HANDLE hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, bufferSize, sharedName);

    if (!hMapFile)
    {
        DWORD err = GetLastError();
        wchar_t buf[256];
        swprintf(buf, 256, L"CreateFileMapping failed: %lu", err);
        MessageBoxW(NULL, buf, L"Error", MB_OK);
    }

    // 映射视图
    void *pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, bufferSize);
    if (!pBuf)
    {
        MessageBoxW(NULL, L"MapViewOfFile failed", L"Error", MB_OK);
        CloseHandle(hMapFile);
        return 1;
    }

    // 写入数据
    const wchar_t *message = L"Hello from Process A!";
    memcpy(pBuf, message, (wcslen(message) + 1) * sizeof(wchar_t));

    // 第二个参数为 FALSE -> auto-reset
    HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, L"FanyNamedEvent");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hEvent);

    return (int)msg.wParam;
}