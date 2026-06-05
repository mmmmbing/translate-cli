#pragma once

#include <string>

/**
 * @brief 文件与剪贴板工具类
 */
class FileUtils {
public:
    /**
     * @brief 从文件读取内容
     * @param filepath 文件路径
     * @return 文件内容字符串，失败返回空字符串
     */
    static std::string ReadFile(const std::string& filepath);

    /**
     * @brief 获取系统剪贴板内容
     * @return 剪贴板文本
     */
    static std::string GetClipboardContent();

    /**
     * @brief 去除字符串首尾空白字符
     * @param str 待处理字符串
     * @return 处理后的字符串
     */
    static std::string Trim(const std::string& str);

    /**
     * @brief 从标准输入 (stdin) 读取全部内容
     * @return 读到的字符串
     */
    static std::string ReadStdin();
};