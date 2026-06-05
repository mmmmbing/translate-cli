# Tasks

- [x] Task 1: 在 CLI 中暴露"设置默认模型 / URL / 查看配置"三个开关
  - [x] SubTask 1.1: 在 `ConfigManager` 中增加 `SetModel` / `SetUrl` / `Get` 便捷函数（落到 `data` 字典或直接修改 `AppConfig` 后写回）
  - [x] SubTask 1.2: 在 `src/main.cpp` 的 CLI11 配置中新增三个 flag：`--set-model` / `--set-url` / `--show-config`
  - [x] SubTask 1.3: 在 CLI11_PARSE 之后、最先判断这三个开关：命中则修改并 `Save` 配置（缺失文件时自动创建），打印结果后 `return 0`
  - [x] SubTask 1.4: 验证：`translate --set-model qwen2.5:14b` 后再次 `translate --show-config` 能看到新模型

- [x] Task 2: 支持从 stdin 自动读取待翻译文本
  - [x] SubTask 2.1: 在 `FileUtils` 中新增 `ReadStdin()` 静态方法，使用 `std::istreambuf_iterator<char>` 读取 `std::cin` 全量内容并返回 `std::string`
  - [x] SubTask 2.2: 在 `main.cpp` 输入源优先级链中追加：位置参数 / `-f` / `-c` 都为空 且 `!isatty(fileno(stdin))` 时，调用 `FileUtils::ReadStdin()` 并打印一条 INFO 日志
  - [x] SubTask 2.3: 验证：`echo "hello" | build\translate.exe -l Chinese` 进入了翻译流程并打印 `开始翻译` 日志（Ollama 不可达时返回非 0，符合 spec 允许）

- [x] Task 3: 修复 Windows 剪贴板中文编码
  - [x] SubTask 3.1: 在 `FileUtils::GetClipboardContent` 中把 `CF_TEXT` 替换为优先尝试 `CF_UNICODETEXT`（用 `GlobalLock` 拿到 `wchar_t*`），再通过 `WideCharToMultiByte(CP_UTF8, ...)` 转成 UTF-8 `std::string`
  - [x] SubTask 3.2: 当 `CF_UNICODETEXT` 不可用时回退到原 `CF_TEXT` 行为，避免剪贴板内容异常时直接失败
  - [x] SubTask 3.3: 验证：复制中文 → `translate -c -l English`，输出无乱码（未在本次运行中实际验证，原因：剪贴板中文读取依赖 Windows GUI 会话环境，本命令行为无 TTY/无 GUI，但代码路径已由静态确认 + Unicode 转码逻辑实现）

- [x] Task 4: 改善 `splitUtf8Chunks` 对非法字节的兜底
  - [x] SubTask 4.1: 把原来"非法起始字节直接 `++i` 跳过"改为：把该单字节原样追加到 `out_complete`（保留用户可见字符），再 `++i` 继续
  - [x] SubTask 4.2: 验证：翻译为 English 时英文 / 标点不丢失（未在本次运行中实际跑通端到端翻译，原因：Ollama 服务不可达；逻辑已由代码静态确认）

- [x] Task 5: 更新 README
  - [x] SubTask 5.1: 在"命令行参数"小节追加 `--set-model` / `--set-url` / `--show-config` 的说明与示例
  - [x] SubTask 5.2: 新增小节"管道输入"，演示 `echo "..." | translate` 用法
  - [x] SubTask 5.3: 在"编码说明"中追加一句：剪贴板现已支持中文（`CF_UNICODETEXT` → UTF-8）

# Task Dependencies
- Task 3 与 Task 4 互相独立，可并行
- Task 2 依赖 Task 1 已实现的"配置加载完成后才进入输入判断"流程（但实际上 Task 2 不依赖 Task 1 的实现细节，代码层面可独立编写）
- Task 5 依赖 Task 1 / 2 / 3 / 4 全部完成
