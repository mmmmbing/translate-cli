#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <CLI/CLI.hpp>
#include "logger.hpp"
#include "config_manager.hpp"
#include "ollama_client.hpp"
#include "file_utils.hpp"

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>      // _isatty / _fileno
#else
  #include <unistd.h> // isatty / fileno
#endif

// 在 Windows 上把控制台切换到 UTF-8 代码页,
// 解决 std::cout 输出中文字符串乱码的问题
static void enableUtf8Console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // stdout 写出的字节按 UTF-8 解释
    SetConsoleCP(CP_UTF8);       // stdin 读入按 UTF-8 解释
#endif
}

int main(int argc, char** argv) {
    enableUtf8Console();

    CLI::App app{"TranslateCLI - 基于 Ollama 的命令行翻译工具"};

    std::string input_text;
    std::string file_path;
    std::string target_lang = "Chinese";
    std::string custom_model;
    bool use_clipboard = false;

    app.add_option("text", input_text, "要翻译的文本");
    app.add_option("-f,--file", file_path, "从文件读取内容进行翻译");
    app.add_option("-l,--lang", target_lang, "目标语言 (默认: Chinese)");
    app.add_option("-m,--model", custom_model, "指定使用的模型 (覆盖配置文件)");
    app.add_flag("-c,--clipboard", use_clipboard, "从剪贴板读取内容进行翻译");

    // --- 配置管理开关 ---
    std::string set_model;
    std::string set_url;
    bool show_config = false;
    app.add_option("--set-model", set_model, "将默认模型写入配置文件后退出");
    app.add_option("--set-url",   set_url,   "将默认 Ollama URL 写入配置文件后退出");
    app.add_flag("--show-config", show_config, "打印当前生效的 ollama_url / model 后退出");

    CLI11_PARSE(app, argc, argv);

    // 0. 处理配置管理开关: 先于加载配置判断,
    //    以便配置文件不存在时也能正常创建.
    if (!set_model.empty() || !set_url.empty() || show_config) {
        AppConfig config = ConfigManager::Load();
        if (!set_model.empty()) {
            config.model = set_model;
            ConfigManager::Save(config);
            LOG_INFO("已将默认模型设置为: %s", set_model.c_str());
            return 0;
        }
        if (!set_url.empty()) {
            config.ollama_url = set_url;
            ConfigManager::Save(config);
            LOG_INFO("已将默认 Ollama URL 设置为: %s", set_url.c_str());
            return 0;
        }
        // --show-config
        LOG_INFO("ollama_url: %s", config.ollama_url.c_str());
        LOG_INFO("model: %s", config.model.c_str());
        return 0;
    }

    // 1. 加载配置
    AppConfig config = ConfigManager::Load();
    if (!custom_model.empty()) config.model = custom_model;

    // 2. 确定输入源
    std::string final_text = input_text;
    if (!file_path.empty()) {
        LOG_INFO("正在读取文件: %s", file_path.c_str());
        final_text = FileUtils::ReadFile(file_path);
    } else if (use_clipboard) {
        LOG_INFO("正在读取剪贴板内容...");
        final_text = FileUtils::GetClipboardContent();
    } else if (final_text.empty()) {
        // 仅当 位置参数 / -f / -c 都未指定时, 才尝试从 stdin 读取
        bool stdin_is_tty;
#ifdef _WIN32
        stdin_is_tty = (_isatty(_fileno(stdin)) != 0);
#else
        stdin_is_tty = (isatty(fileno(stdin)) != 0);
#endif
        if (!stdin_is_tty) {
            LOG_INFO("正在从 stdin 读取输入...");
            final_text = FileUtils::ReadStdin();
        }
    }

    // 3. 预处理
    final_text = FileUtils::Trim(final_text);
    if (final_text.empty()) {
        LOG_ERROR("没有检测到有效的待翻译内容！请通过位置参数、-f 文件 或 -c 剪贴板 提供文本。");
        return 1;
    }

    // 4. 初始化客户端并开始翻译
    OllamaClient client(config.ollama_url, config.model);

    LOG_INFO("开始翻译 (Model: %s, Lang: %s)...", config.model.c_str(), target_lang.c_str());
    std::cout << "---\n";

    // 流式回调: 直接打印到标准输出
    auto print_callback = [](const std::string& chunk) {
        std::cout << chunk << std::flush;
    };

    bool success = client.Translate(final_text, target_lang, print_callback);

    std::cout << "\n---\n";
    if (success) {
        LOG_INFO("翻译完成。");
    } else {
        LOG_ERROR("翻译过程中发生错误。");
        return 1;
    }

    return 0;
}
