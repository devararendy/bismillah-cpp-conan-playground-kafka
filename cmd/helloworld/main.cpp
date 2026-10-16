#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/JsonSink.h" // Required for JSON sinks
#include <string>

int main() {
    // 1. Start Quill's background formatting worker thread
    quill::BackendOptions backend_options;
    quill::Backend::start(backend_options);

    // 2. Create a JSON sink targeting the console (stdout)
    // Pass empty PatternFormatterOptions since the structure handles layout format
    auto json_sink = quill::Frontend::create_or_get_sink<quill::JsonConsoleSink>("json_stdout");
    
    quill::Logger* logger = quill::Frontend::create_or_get_logger(
        "json_logger", 
        std::move(json_sink), 
        quill::PatternFormatterOptions{"", "%H:%M:%S.%Qns", quill::Timezone::GmtTime}
    );

    // 3. Test structured variables
    int user_id = 42;
    std::string action = "login_success";

    // 4. Log structured data using Zerolog style named placeholders
    // The keys inside {} map directly into the output JSON string fields!
    LOG_INFO(logger, "User interaction recorded", user_id, action);
    
    // Alternative approach: Map arbitrary inline keys directly using variables
    int latency_ms = 4;
    LOG_INFO(logger, "Process telemetry summary {latency} {status}", latency_ms, "ok");

    return 0;
}

