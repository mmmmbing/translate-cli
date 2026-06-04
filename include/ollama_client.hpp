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
};