#include "ollama_client.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include "logger.hpp"
#include <sstream>

using json = nlohmann::json;

OllamaClient::OllamaClient(const std::string& base_url, const std::string& model_name)
    : base_url_(base_url), model_name_(model_name) {}

bool OllamaClient::Translate(const std::string& text, const std::string& target_lang, StreamCallback on_stream) {
    // 1. 构建 Prompt
    std::string prompt = "Please translate the following text into " + target_lang +
                         ". Only output the translation result, do not explain.\n\nText: " + text;

    // 2. 构建 JSON Body (符合 Ollama API 规范)
    json payload = {
        {"model",  model_name_},
        {"prompt", prompt},
        {"stream", true} // 关键: 开启流式输出
    };

    LOG_INFO("正在向 Ollama 发送请求...");

    // 3. 构造 Session 并设置流式回调
    // cpr::WriteCallback 的签名: bool(std::string data, intptr_t userdata)
    cpr::Session session;
    session.SetUrl(cpr::Url{base_url_ + "/api/generate"});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
    session.SetBody(cpr::Body{payload.dump()});
    session.SetOption(cpr::WriteCallback{
        [&on_stream](const std::string& data, intptr_t /*userdata*/) -> bool {
            // Ollama 返回的是多个 JSON 对象拼接的字符串, 需要按行分割
            std::istringstream stream(data);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.empty()) continue;
                try {
                    json j = json::parse(line);
                    if (j.contains("response")) {
                        std::string token = j["response"].get<std::string>();
                        if (!token.empty()) on_stream(token);
                    }
                } catch (...) {
                    // 忽略解析错误, 防止非 JSON 数据导致崩溃
                }
            }
            return true; // 继续接收
        }
    });

    cpr::Response response = session.Post();

    if (response.status_code != 200) {
        LOG_ERROR("Ollama 请求失败! 状态码: %d, 响应: %s",
                  response.status_code, response.text.c_str());
        return false;
    }

    return true;
}
