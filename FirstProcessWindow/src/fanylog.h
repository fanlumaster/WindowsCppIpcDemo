#pragma once
#include "spdlog/spdlog.h"
#include <spdlog/sinks/basic_file_sink.h>

inline std::string LogFilePath =
    "C:/Users/SonnyCalcr/EDisk/CppCodes/Win32Codes/"
    "Win32WebviewTemplate/log.txt";
inline auto logger = spdlog::basic_logger_mt("file_logger", LogFilePath);

void InitLog();