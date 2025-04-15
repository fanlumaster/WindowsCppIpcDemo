#include "WebView2.h"
#include "boost/locale/encoding_utf.hpp"
#include "common_utils.h"
#include "my_webview.h"
#include "spdlog/spdlog.h"
#include <boost/locale.hpp>
#include <cwchar>
#include <filesystem>
#include <handleapi.h>
#include <string>
#include <wil/com.h>
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>

using namespace Microsoft::WRL;

std::wstring ReadHtmlFile(const std::wstring &filePath)
{
    std::wifstream file(filePath);
    if (!file)
    {
        // TODO: Log
        return L"";
    }
    // Use Boost Locale to handle UTF-8
    file.imbue(boost::locale::generator().generate("en_US.UTF-8"));
    std::wstringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int PrepareWindowHtml()
{
    std::wstring htmlPath =
        std::filesystem::current_path().wstring() + L"/html/index.html";
    ::HTMLString = ReadHtmlFile(htmlPath);
    std::wstring bodyPath =
        std::filesystem::current_path().wstring() + L"/html/body.html";
    ::BodyString = ReadHtmlFile(bodyPath);
    return 0;
}

void UpdateHtmlContentWithJavaScript(ComPtr<ICoreWebView2> webview,
                                     const std::wstring &newContent)
{
    if (webview != nullptr)
    {
        std::wstring script =
            L"document.body.innerHTML = `" + newContent + L"`;";
        webview->ExecuteScript(script.c_str(), nullptr);
    }
}

// Handle WebView2 controller creation
HRESULT OnControllerCreated(            //
    HWND hWnd,                          //
    HRESULT result,                     //
    ICoreWebView2Controller *controller //
)
{
    if (!controller || FAILED(result))
    {
        ShowErrorMessage(hWnd, L"Failed to create WebView2 controller.");
        return E_FAIL;
    }

    webviewController = controller;
    webviewController->get_CoreWebView2(webview.GetAddressOf());

    if (!webview)
    {
        ShowErrorMessage(hWnd, L"Failed to get WebView2 instance.");
        return E_FAIL;
    }

    // Configure WebView settings
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview->get_Settings(&settings)))
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreHostObjectsAllowed(TRUE);
    }

    // Configure virtual host path
    if (SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&webview3))))
    {
        webview3->SetVirtualHostNameToFolderMapping(         //
            L"appassets",                                    //
            ::LocalAssetsPath.c_str(),                       //
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS //
        );                                                   //
    }

    // Set transparent background
    if (SUCCEEDED(
            controller->QueryInterface(IID_PPV_ARGS(&webviewController2))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2->put_DefaultBackgroundColor(backgroundColor);
    }

    // Adjust to window size
    RECT bounds;
    GetClientRect(hWnd, &bounds);
    webviewController->put_Bounds(bounds);

    // Navigate to HTML
    HRESULT hr = webview->NavigateToString(HTMLString.c_str());
    if (FAILED(hr))
    {
        ShowErrorMessage(hWnd, L"Failed to navigate to string.");
    }

    RegisterWebMessageHandler(webview);

    // webview->OpenDevToolsWindow();

    return S_OK;
}

// Handle WebView2 environment creation
HRESULT OnEnvironmentCreated(HWND hWnd, HRESULT result,
                             ICoreWebView2Environment *env)
{
    if (FAILED(result) || !env)
    {
        ShowErrorMessage(hWnd, L"Failed to create WebView2 environment.");
        return result;
    }

    // Create WebView2 controller
    return env->CreateCoreWebView2Controller(                                //
        hWnd,                                                                //
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [hWnd](HRESULT result,                                           //
                   ICoreWebView2Controller *controller) -> HRESULT {         //
                return OnControllerCreated(hWnd, result, controller);        //
            })                                                               //
            .Get()                                                           //
    );                                                                       //
}

// Initialize WebView2
void InitWebview(HWND hWnd)
{
    CreateCoreWebView2EnvironmentWithOptions(                                 //
        nullptr,                                                              //
        nullptr,                                                              //
        nullptr,                                                              //
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>( //
            [hWnd](HRESULT result,                                            //
                   ICoreWebView2Environment *env) -> HRESULT {                //
                return OnEnvironmentCreated(hWnd, result, env);               //
            })                                                                //
            .Get()                                                            //
    );                                                                        //
}

void RegisterWebMessageHandler(ComPtr<ICoreWebView2> webview)
{
    webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [](ICoreWebView2 *sender,
               ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                wil::unique_cotaskmem_string message;
                args->TryGetWebMessageAsString(&message);
                spdlog::info("Received message: {}",
                             wstring_to_string(message.get()));
                const wchar_t *sharedName = L"Local\\MySharedMemory";
                const int bufferSize = 1024;

                // 打开共享内存
                HANDLE hMapFile =
                    OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, sharedName);
                if (!hMapFile)
                {
                    DWORD err = GetLastError();
                    wchar_t buf[256];
                    swprintf(buf, 256,
                             L"OpenFileMapping failed with error: %lu", err);
                    MessageBoxW(NULL, buf, L"Error", MB_OK);
                }

                // 映射视图
                void *pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
                                           bufferSize);
                if (!pBuf)
                {
                    DWORD err = GetLastError();
                    CloseHandle(hMapFile);
                }

                wcscpy_s(static_cast<wchar_t *>(pBuf),
                         bufferSize / sizeof(wchar_t), message.get());

                spdlog::info(
                    "写入共享内存完成：{}",
                    boost::locale::conv::utf_to_utf<std::string::value_type>(
                        message.get()));

                // 清理
                UnmapViewOfFile(pBuf);
                CloseHandle(hMapFile);

                HANDLE hEvent = OpenEventW(
                    EVENT_MODIFY_STATE, // 只需要修改事件状态（比如 SetEvent）
                    FALSE,              // 不继承句柄
                    L"FanyNamedEvent"   // 事件名称
                );
                if (!hEvent)
                {
                    // 错误处理
                    spdlog::info("Failed to open event");
                    return S_OK;
                }

                if (!SetEvent(hEvent))
                {
                    DWORD err = GetLastError();
                    spdlog::info("Failed to set event: {}", err);
                }
                CloseHandle(hEvent);
                return S_OK;
            })
            .Get(),
        nullptr);
}