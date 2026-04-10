#pragma once

#include <boost/json.hpp>
#include <string_view>

namespace logging {

void InitBoostLog();

void LogMessage(std::string_view message, boost::json::value data = {});

}  // namespace logging