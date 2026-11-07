#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>
#include <string>
namespace bedrock_insight::levilamina {
class RestCapture;
using RestToken=std::shared_ptr<RestCapture>;
using RestSink=std::function<void(AnalyticsEvent)>;
RestToken start_rest_capture(RestSink) noexcept;
void stop_rest_capture(RestToken const&) noexcept;
void forget_rest(RestToken const&,std::string const&) noexcept;
}
