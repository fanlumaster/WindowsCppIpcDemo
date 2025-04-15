#include "fanylog.h"

void InitLog()
{
    spdlog::set_default_logger(::logger);
    spdlog::flush_on(spdlog::level::info);
}