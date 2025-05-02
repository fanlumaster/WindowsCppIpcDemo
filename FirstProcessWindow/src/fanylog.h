#pragma once
#include "spdlog/spdlog.h"
#include <spdlog/sinks/basic_file_sink.h>

inline std::string LogFilePath =
    R"(C:\Users\SonnyCalcr\EDisk\CppCodes\Win32Codes\WindowsCppIpcViaPipeDemo\FirstProcessWindow\log.txt)";
inline auto logger = spdlog::basic_logger_mt("file_logger", LogFilePath);

void InitLog();