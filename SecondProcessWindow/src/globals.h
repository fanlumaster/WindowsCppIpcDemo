#pragma once

#include <Windows.h>
#include <string>
#include <windef.h>


#define WM_EXECUTESCRIPT (WM_USER + 100)

inline std::wstring global_script;

inline HWND global_hwnd;