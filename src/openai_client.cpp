#include "openai_client.hpp"
#include "logger.hpp"
#include <sstream>

using json = nlohmann::json;

bool OpenAIClient::Translate(const std::string& text,
                             const std::string& target_lang,
                             StreamCallback on_stream) {
    // 1. 组装 OpenAI 专属的 JSON 格式
    std::string system_prompt =
        "You are a professional translator. Translate the following text into "
        + target_lang + ". Do not output any explanation, just the translation.";

    json payload = {
        {"model", model_},
        {"messages", {
            {{"role", "system"}, {"content", system_prompt}},
            {{"role", "user"},   {"content", text}}
        }},
        {"stream", true}
    };

    // 2. 发送请求 (POST + 流式回调)
    auto response = cpr::Post(
        cpr::Url{base_url_ + "/v1/chat/completions"},
        cpr::Header{
            {"Content-Type",  "application/json"},
            {"Authorization", "Bearer " + api_key_}
        },
        cpr::Body{payload.dump()},
        cpr::WriteCallback{[&on_stream](const std::string& data, intptr_t /*userdata*/) -> bool {
            // OpenAI 兼容接口的 SSE 格式: data: {json}\n\n
            std::istringstream stream(data);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.rfind("data: ", 0) == 0) {
                    std::string json_str = line.substr(6);
                    if (json_str == "[DONE]") continue;
                    try {
                        json j = json::parse(json_str);
                        if (j.contains("choices") && !j["choices"].empty()) {
                            auto& choice = j["choices"][0];
                            if (choice.contains("delta") &&
                                choice["delta"].contains("content")) {
                                std::string token = choice["delta"]["content"];
                                on_stream(token);
                            }
                        }
                    } catch (...) {
                        // 忽略单行解析失败
                    }
                }
            }
            return true;
        }}
    );

    if (response.status_code != 200) {
        LOG_ERROR("OpenAI 请求失败! 状态码: %d, 响应: %s",
                  response.status_code, response.text.c_str());
        return false;
    }
    return true;
}
