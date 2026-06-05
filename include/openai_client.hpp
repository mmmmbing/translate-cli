#pragma once

#include "translation_client.hpp"
#include "config_manager.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <string>
#include <functional>

/**
 * @brief OpenAI / DeepSeek 格式客户端实现
 * 适用于 OpenAI, DeepSeek, Azure OpenAI 等兼容 OpenAI API 格式的服务。
 */
class OpenAIClient : public TranslationClient {
public:
    /**
     * @brief 构造函数
     * @param config 配置管理器引用，从中读取 API Key 和 URL
     */
    explicit OpenAIClient(const ConfigManager& config) {
        // 从配置中读取参数，如果未设置则使用默认值
        api_key_  = config.get("openai_api_key", "");
        base_url_ = config.get("openai_base_url", "https://api.openai.com");
        model_    = config.get("openai_model", "gpt-3.5-turbo");
    }

    // 仅声明一个兼容性构造函数 (供 main 中使用 url 直接构造)
    explicit OpenAIClient(const std::string& base_url)
        : api_key_(""), base_url_(base_url), model_("gpt-3.5-turbo") {}

    // 重写 TranslationClient::Translate
    bool Translate(const std::string& text,
                   const std::string& target_lang,
                   StreamCallback on_stream) override;

private:
    std::string api_key_;
    std::string base_url_;
    std::string model_;
};
