#include "common_utils.h"
#include "my_webview.h"
#include "main.h"
#include "fanylog.h"
#include "globals.h"
#include "spdlog/spdlog.h"
#include <boost/locale.hpp>
#include <dwmapi.h>
#include <intsafe.h>
#include <stdlib.h>
#include <string>
#include <synchapi.h>
#include <tchar.h>
#include <windows.h>
#include <winnt.h>
#include <winuser.h>
#include <wrl.h>
#include <wrl/client.h>
#include "translate_api.h"

#pragma comment(lib, "dwmapi.lib")

using namespace Microsoft::WRL;

HANDLE hPipe;

void EventLoopThread()
{
    while (true)
    {
        // 等待客户端连接
        BOOL connected = ConnectNamedPipe(hPipe, NULL);
        if (connected)
        {
            wchar_t buffer[1024];
            DWORD bytesRead;

            // 持续读取数据
            while (true)
            {
                // 从管道读取数据
                BOOL readResult =
                    ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL);
                if (!readResult || bytesRead == 0)
                {
                    break; // 连接断开或无数据
                }

                // 输出接收到的数据
                spdlog::info(
                    "Received: {}",
                    boost::locale::conv::utf_to_utf<std::string::value_type>(
                        buffer));
                std::wstring receivedStr(buffer, bytesRead / sizeof(wchar_t));
                UpdateHtmlContentWithJavaScript(webview, receivedStr);
            }

            // 关闭当前连接
            DisconnectNamedPipe(hPipe);
        }
        else
        {
            spdlog::error("Failed to connect to pipe!");
        }
    }
    //
    // ==================================================================================
    //

    /*
    while (true)
    {
        DWORD result = WaitForSingleObject(hEvent, INFINITE);
        if (result == WAIT_OBJECT_0)
        {
            spdlog::info("EventLoopThread: Event triggered !");
            const wchar_t *sharedName = L"Local\\MySharedMemory";
            const int bufferSize = 1024;
            HANDLE hMapFile =
                OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, sharedName);
            if (!hMapFile)
            {
                DWORD err = GetLastError();
                wchar_t buf[256];
                swprintf(buf, 256, L"OpenFileMapping failed with error: %lu",
                         err);
                MessageBoxW(NULL, buf, L"Error", MB_OK);
            }

            // 映射视图
            void *pBuf =
                MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, bufferSize);
            if (!pBuf)
            {
                DWORD err = GetLastError();
                wchar_t buf[256];
                CloseHandle(hMapFile);
            }

            // 读取数据
            wchar_t *received = static_cast<wchar_t *>(pBuf);
            spdlog::info(
                "Received: {}",
                boost::locale::conv::utf_to_utf<std::string::value_type>(
                    received));

            std::wstring receivedStr(received);
            UpdateHtmlContentWithJavaScript(webview, receivedStr);

            // 清理
            UnmapViewOfFile(pBuf);
            CloseHandle(hMapFile);
            // 自动重置事件不需要 ResetEvent，直接继续下一轮等待
        }
        else
        {
            break;
        }
    }

    CloseHandle(hEvent);
    */
}

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
    case WM_EXECUTESCRIPT: {
        if (webview)
        {
            spdlog::info("WM_EXECUTESCRIPT");
            spdlog::info("script is really executed: {}",
                         wstring_to_string(global_script));
            webview->ExecuteScript(
                global_script.c_str(),
                Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                    [](HRESULT errorCode, LPCWSTR result) -> HRESULT {
                        if (FAILED(errorCode))
                        {
                            spdlog::info("Script execution failed.");
                        }
                        else
                        {
                            spdlog::info("Script execution succeeded.");
                        }
                        return S_OK;
                    })
                    .Get());
        }
        return 0;
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

    ::global_hwnd = hWnd;
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

    //
    // Ipc
    //
    // 创建命名管道
    ::hPipe = CreateNamedPipe(
        LR"(\\.\pipe\MyPipe)",                           // 管道名称
        PIPE_ACCESS_DUPLEX,                              // 双向管道
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, // 数据传输模式
        1,                                               // 最大实例数
        1024,                                            // 输出缓冲区大小
        1024,                                            // 输入缓冲区大小
        0,                                               // 默认超时时间
        NULL                                             // 默认安全属性
    );

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        // TODO: log
        return 1;
    }

    std::thread listener(EventLoopThread);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    listener.join();
    // 关闭管道
    CloseHandle(hPipe);

    return (int)msg.wParam;
}