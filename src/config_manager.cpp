#include "config_manager.hpp"
#include <fstream>
#include <cstdlib> // getenv
#include <nlohmann/json.hpp> // CMake FetchContent 会自动引入
#include "logger.hpp"

using json = nlohmann::json;

std::string ConfigManager::GetConfigPath() {
    // 简单起见，保存在用户主目录下的 .translate_cli_config.json
    auto home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE"); // Windows fallback
    return std::string(home ? home : ".") + "/.translate_cli_config.json";
}

AppConfig ConfigManager::Load() {
    std::string path = GetConfigPath();
    AppConfig config;

    if (!std::filesystem::exists(path)) {
        LOG_WARN("配置文件不存在，将使用默认配置并创建新文件。");
        Save(config);
        return config;
    }

    try {
        std::ifstream f(path);
        json j = json::parse(f);
        if (j.contains("ollama_url")) config.ollama_url = j["ollama_url"];
        if (j.contains("model")) config.model = j["model"];
        LOG_INFO("配置加载成功: Model=%s", config.model.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("读取配置文件失败: %s", e.what());
    }
    return config;
}

void ConfigManager::Save(const AppConfig& config) {
    json j;
    j["ollama_url"] = config.ollama_url;
    j["model"] = config.model;

    std::ofstream f(GetConfigPath());
    f << j.dump(4);
    LOG_INFO("配置已保存至: %s", GetConfigPath().c_str());
}