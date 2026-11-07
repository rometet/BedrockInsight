#pragma once
#include "bedrock_insight/core/event.hpp"
#include <functional>
#include <memory>
namespace bedrock_insight::levilamina {
class ShieldEvidenceCapture;
using ShieldEvidenceToken=std::shared_ptr<ShieldEvidenceCapture>;
using ShieldEvidenceSink=std::function<void(AnalyticsEvent)>;
// Only exact process-local BEDROCKINSIGHT_SHIELD_EVIDENCE=1 enables the hook.
// Disabled startup returns a dormant owned token, not a registration failure.
ShieldEvidenceToken start_shield_evidence(ShieldEvidenceSink) noexcept;
void stop_shield_evidence(ShieldEvidenceToken const&) noexcept;
bool shield_evidence_enabled(ShieldEvidenceToken const&) noexcept;
}
