#include "file_utils.hpp"
#include "logger.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio> // 用于 popen
#endif

// --- 文件读取实现 ---
// 内部函数: 读取原始字节流 (含 BOM)
static std::string readRawFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return "";

    auto size = file.tellg();
    if (size <= 0) return "";
    file.seekg(0, std::ios::beg);
    std::string content(static_cast<size_t>(size), '\0');
    file.read(&content[0], size);
    return content;
}

// 内部函数: 将 UTF-16 LE 字节流转为 UTF-8
static std::string utf16leToUtf8(const std::string& raw) {
    if (raw.size() < 2) return "";
    std::wstring wide;
    wide.reserve(raw.size() / 2);
    for (size_t i = 0; i + 1 < raw.size(); i += 2) {
        unsigned char lo = static_cast<unsigned char>(raw[i]);
        unsigned char hi = static_cast<unsigned char>(raw[i + 1]);
        wchar_t wc = static_cast<wchar_t>(lo | (hi << 8));
        wide.push_back(wc);
    }
    // Windows 平台: WideCharToMultiByte
    if (wide.empty()) return "";
#ifdef _WIN32
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(),
                                       nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) return "";
    std::string utf8(utf8_len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(),
                        &utf8[0], utf8_len, nullptr, nullptr);
    return utf8;
#else
    // Linux: 使用 codecvt 简化, 此处仅返回空 (实际工程可用 iconv)
    return "";
#endif
}

// 内部函数: 将 UTF-16 BE 字节流转为 UTF-8
static std::string utf16beToUtf8(const std::string& raw) {
    if (raw.size() < 2) return "";
    std::wstring wide;
    wide.reserve(raw.size() / 2);
    for (size_t i = 0; i + 1 < raw.size(); i += 2) {
        unsigned char hi = static_cast<unsigned char>(raw[i]);
        unsigned char lo = static_cast<unsigned char>(raw[i + 1]);
        wchar_t wc = static_cast<wchar_t>(lo | (hi << 8));
        wide.push_back(wc);
    }
    if (wide.empty()) return "";
#ifdef _WIN32
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(),
                                       nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) return "";
    std::string utf8(utf8_len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(),
                        &utf8[0], utf8_len, nullptr, nullptr);
    return utf8;
#else
    return "";
#endif
}

std::string FileUtils::ReadFile(const std::string& filepath) {
    std::string content = readRawFile(filepath);
    if (content.empty()) {
        LOG_ERROR("无法打开或读取空文件: %s", filepath.c_str());
        return "";
    }

    // BOM 检测并转换为 UTF-8
    // UTF-8 BOM: 0xEF 0xBB 0xBF
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        LOG_INFO("检测到 UTF-8 BOM, 已自动去除");
        return content.substr(3);
    }
    // UTF-16 LE BOM: 0xFF 0xFE
    if (content.size() >= 2 &&
        static_cast<unsigned char>(content[0]) == 0xFF &&
        static_cast<unsigned char>(content[1]) == 0xFE) {
        LOG_INFO("检测到 UTF-16 LE BOM, 已自动转换为 UTF-8");
        return utf16leToUtf8(content.substr(2));
    }
    // UTF-16 BE BOM: 0xFE 0xFF
    if (content.size() >= 2 &&
        static_cast<unsigned char>(content[0]) == 0xFE &&
        static_cast<unsigned char>(content[1]) == 0xFF) {
        LOG_INFO("检测到 UTF-16 BE BOM, 已自动转换为 UTF-8");
        return utf16beToUtf8(content.substr(2));
    }

    // 无 BOM, 假定为 UTF-8 (或与终端代码页相同, 因为 LLM 接收的字节序列会原样发送)
    return content;
}

// --- 剪贴板获取实现 ---
std::string FileUtils::GetClipboardContent() {
    std::string result;

#ifdef _WIN32
    if (!OpenClipboard(nullptr)) {
        LOG_WARN("无法打开剪贴板");
        return "";
    }
    // 优先尝试 CF_UNICODETEXT (支持中文等多字节字符),
    // 失败时回退到 CF_TEXT 保持向后兼容.
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pwszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pwszText) {
            // 用 lstrlenW 求字符数, 避免依赖 wstr 是否 '\0' 结尾
            int wlen = lstrlenW(pwszText);
            if (wlen > 0) {
                int utf8_len = WideCharToMultiByte(CP_UTF8, 0, pwszText, wlen,
                                                   nullptr, 0, nullptr, nullptr);
                if (utf8_len > 0) {
                    result.resize(static_cast<size_t>(utf8_len));
                    WideCharToMultiByte(CP_UTF8, 0, pwszText, wlen,
                                        &result[0], utf8_len, nullptr, nullptr);
                }
            }
            GlobalUnlock(hData);
        }
    } else {
        // 回退到 ANSI 文本
        HANDLE hAnsi = GetClipboardData(CF_TEXT);
        if (hAnsi) {
            char* pszText = static_cast<char*>(GlobalLock(hAnsi));
            if (pszText) {
                result = std::string(pszText);
                GlobalUnlock(hAnsi);
            }
        }
    }
    CloseClipboard();
#else
    // Linux/macOS 通用方案：调用 pbpaste 或 xclip
    FILE* pipe = popen("pbpaste 2>/dev/null || xclip -selection clipboard -o 2>/dev/null", "r");
    if (!pipe) {
        LOG_WARN("无法执行剪贴板命令");
        return "";
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
#endif

    return result;
}

// --- 字符串修剪实现 ---
std::string FileUtils::Trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    return (start < end ? std::string(start, end) : std::string());
}

// --- stdin 读取实现 ---
std::string FileUtils::ReadStdin() {
    // 不跳过空白字符, 这样可以原样保留中文 / 多行 / 行尾空白
    std::cin >> std::noskipws;
    std::istreambuf_iterator<char> it(std::cin);
    std::istreambuf_iterator<char> end;
    return std::string(it, end);
}