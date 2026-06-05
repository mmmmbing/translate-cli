#include "ollama_client.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include "logger.hpp"
#include <sstream>

using json = nlohmann::json;

OllamaClient::OllamaClient(const std::string& base_url, const std::string& model_name)
    : base_url_(base_url), model_name_(model_name) {}

// 从一段原始字节中切出 "完整的" UTF-8 字符序列,
// 遇到结尾被截断的字节 (不构成完整 UTF-8 字符) 时丢弃, 等待下一次回调补齐.
// 切出来的每一段都保证是合法 UTF-8 字符串, 可安全作为 std::string 输出或交给 nlohmann::json.
static void splitUtf8Chunks(const std::string& buf,
                            std::string& out_complete,
                            std::string& out_pending) {
    out_complete.clear();
    out_pending.clear();
    size_t i = 0;
    const size_t n = buf.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(buf[i]);
        size_t need = 1;
        if      (c < 0x80) need = 1;  // ASCII
        else if ((c & 0xE0) == 0xC0) need = 2;
        else if ((c & 0xF0) == 0xE0) need = 3;
        else if ((c & 0xF8) == 0xF0) need = 4;
        else {
            // 非法起始字节: 原样追加该单字节, 保留用户可见字符 (? , . 等)
            out_complete.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        if (i + need > n) {
            // 末尾的字节不完整, 留给下一次回调
            out_pending.assign(buf, i, n - i);
            break;
        }
        // 校验后续字节是否都是 10xxxxxx
        bool ok = true;
        for (size_t k = 1; k < need; ++k) {
            unsigned char cc = static_cast<unsigned char>(buf[i + k]);
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
        }
        if (!ok) {
            // 非法序列: 原样追加首字节, 避免吞掉 ? , . 等可见 ASCII
            out_complete.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        out_complete.append(buf, i, need);
        i += need;
    }
}

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
    //
    // 关键修复:
    //  - Ollama 的 stream 返回是按 '\n' 分割的多个 JSON 对象, 但 callback
    //    不保证按行对齐, 我们需要按 '\n' 切分并把残余字节留给下一次回调.
    //  - response 字段如果含有不完整 UTF-8 序列 (Ollama 在 token 边界经常
    //    出现 1-3 个字节被切到下一个 chunk), nlohmann::json 在 get<std::string>
    //    时会抛 type_error.316. 我们:
    //      a) 按字节把不完整 UTF-8 字符放到 line_pending_ 里等到下次补齐
    //      b) 用 json::parse 的 allow_exceptions=false 解析单行
    //      c) 即便字段不合法, 也尽量从原始字节里"安全地"切出 UTF-8 段
    line_pending_.clear();
    cpr::Session session;
    session.SetUrl(cpr::Url{base_url_ + "/api/generate"});
    session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
    session.SetBody(cpr::Body{payload.dump()});
    session.SetOption(cpr::WriteCallback{
        [this, &on_stream](const std::string& data, intptr_t /*userdata*/) -> bool {
            line_pending_ += data;

            size_t pos;
            while ((pos = line_pending_.find('\n')) != std::string::npos) {
                std::string line = line_pending_.substr(0, pos);
                line_pending_.erase(0, pos + 1);
                if (line.empty()) continue;

                // 解析 JSON, 失败时也不抛异常
                json j = json::parse(line, nullptr, false);
                if (j.is_discarded() || !j.is_object()) continue;

                if (j.contains("response") && j["response"].is_string()) {
                    std::string token = j["response"].get<std::string>();
                    std::string safe, pending;
                    splitUtf8Chunks(token, safe, pending);
                    if (!safe.empty()) on_stream(safe);
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
