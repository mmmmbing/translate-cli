#pragma once
#include <string>
#include <filesystem>
#include <unordered_map>

struct AppConfig {
    std::string ollama_url = "http://localhost:11434";
    std::string model = "qwen2.5:7b"; // 默认模型
};

class ConfigManager {
public:
    static AppConfig Load();
    static void Save(const AppConfig& config);

    // 简单的 key-value 访问接口 (供 OpenAIClient 等使用)
    std::string get(const std::string& key, const std::string& default_val = "") const {
        auto it = data.find(key);
        if (it != data.end()) {
            return it->second;
        }
        return default_val;
    }

    // 设置 key-value
    void set(const std::string& key, const std::string& value) {
        data[key] = value;
    }

private:
    static std::string GetConfigPath();
    std::unordered_map<std::string, std::string> data;
};
