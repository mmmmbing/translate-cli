#pragma once
#include <string>
#include <functional>

// 流式输出的回调函数类型
using StreamCallback = std::function<void(const std::string& chunk)>;

class TranslationClient {
public:
    virtual ~TranslationClient() = default;

    // 核心翻译方法
    // text: 待翻译文本
    // target_lang: 目标语言 (如 "Chinese", "English")
    // on_stream: 接收到每个 token 时的回调
    virtual bool Translate(const std::string& text, const std::string& target_lang, StreamCallback on_stream) = 0;
};