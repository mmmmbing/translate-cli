#pragma once
#include "translation_client.hpp"
#include <string>

class OllamaClient : public TranslationClient {
public:
    OllamaClient(const std::string& base_url, const std::string& model_name);

    bool Translate(const std::string& text, const std::string& target_lang, StreamCallback on_stream) override;

private:
    std::string base_url_;
    std::string model_name_;
    // 流式回调间保存尚未消费完的 JSON 行残余字节
    std::string line_pending_;
};