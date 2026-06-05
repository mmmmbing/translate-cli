# 默认模型配置 / 管道输入 / 中文编码适配 Spec

## Why

当前 CLI 工具存在三个明显的可用性问题：

1. **每次都要指定大模型**：用户必须显式传入 `-m <model>` 才能切换模型。虽然配置文件中已经能保存 `model` 字段，但缺乏一种"在终端里一条命令修改并持久化默认模型"的体验；同时新用户首次运行就会因为配置文件不存在而需要手动编辑 JSON。
2. **管道符（stdin）输入未适配**：在 PowerShell / Bash 中常见的 `echo "xxx" | translate` 或 `cat a.txt | translate` 场景下，程序没有任何输入，提示"没有检测到有效的待翻译内容"。需要支持从 stdin 自动读取。
3. **中文到其他目标语言的编码未适配**：Windows 剪贴板读取使用的是 `CF_TEXT`（ANSI 代码页），当用户复制中文内容再调用 `-c` 时，剪贴板内容已经是乱码；同样，输入包含中文字符的文本经过 Ollama 流式返回英文 / 日文等非中文时，由于 `std::cout` 在 Windows 下默认不是 UTF-8 之外的"安全"通道，少数 token 中夹带的非法 UTF-8 序列也可能打印成 `?` 或乱码。

本次改动以最小改动为目标，使工具对中文用户更顺手。

## What Changes

- 新增 CLI 子命令 `--set-model <name>` 与 `--set-url <url>`，执行后立刻把默认值写入 `~/.translate_cli_config.json`。
- 新增 CLI 选项 `--show-config`，在终端直接打印当前生效的 `ollama_url` 和 `model`。
- 在 `main.cpp` 中加入 **stdin 自动检测**：当且仅当没有提供位置参数、文件、剪贴板输入、且 stdin 不是交互终端（`isatty` 判定为 false）时，从 stdin 读取所有内容作为待翻译文本。
- 修复 `FileUtils::GetClipboardContent` 的编码问题：Windows 下优先使用 `CF_UNICODETEXT`，通过 `WideCharToMultiByte(CP_UTF8, ...)` 转成 UTF-8；非 Windows 平台行为保持不变。
- 修复 `splitUtf8Chunks` 在非中文目标语言下把"非法起始字节"简单 `++i` 跳过导致的字符丢失问题：当连续出现非法字节时，整段原始字节原样下推到 `on_stream`（避免吞掉用户实际想看到的 `?`、符号等 ASCII 字节）。
- 更新 `README.md` 中的"配置文件"与"命令行参数"两个小节，补充新增选项的说明与示例。

无破坏性变更（**BREAKING**）：现有命令行参数、配置文件字段、构建方式保持不变。

## Impact

- Affected specs: 无（首个 spec）
- Affected code:
  - `src/main.cpp`（命令行解析 + 输入源优先级 + stdin 读取）
  - `src/file_utils.cpp`（剪贴板编码 + 非法 UTF-8 兜底）
  - `src/ollama_client.cpp`（`splitUtf8Chunks` 兜底策略微调）
  - `include/file_utils.hpp`（视需要暴露 stdin 读取工具函数）
  - `README.md`（文档同步）

## ADDED Requirements

### Requirement: 可在 CLI 中配置默认大模型

CLI 必须在不打开配置文件编辑器的前提下，允许用户设置并持久化默认 Ollama URL 与默认模型名。

#### Scenario: 通过 --set-model 修改默认模型
- **WHEN** 用户执行 `translate.exe --set-model qwen2.5:14b`
- **THEN** 程序把 `~/.translate_cli_config.json` 中的 `model` 字段更新为 `qwen2.5:14b`，打印类似 `[INFO] 已将默认模型设置为: qwen2.5:14b` 的提示，并正常退出（不发起翻译请求）。

#### Scenario: 通过 --set-url 修改默认 URL
- **WHEN** 用户执行 `translate.exe --set-url http://192.168.1.10:11434`
- **THEN** 程序把 `ollama_url` 字段写入配置文件并退出。

#### Scenario: 查看当前配置
- **WHEN** 用户执行 `translate.exe --show-config`
- **THEN** 程序以人类可读格式打印 `ollama_url` 和 `model`，不发起翻译请求。

#### Scenario: 配置文件不存在时
- **WHEN** 用户首次执行 `translate.exe --set-model xxx` 而 `~/.translate_cli_config.json` 还不存在
- **THEN** 程序创建该文件，写入当前 URL（默认 `http://localhost:11434`）与新模型名。

### Requirement: 支持从 stdin 管道输入

当且仅当用户没有提供位置参数、`-f`、`-c` 中的任何一种输入源，且 stdin 被检测为非交互终端时，程序应自动从 stdin 读取所有内容作为待翻译文本。

#### Scenario: 通过 echo 管道输入
- **WHEN** 用户在 PowerShell 中执行 `echo "你好" | translate.exe -l English`
- **THEN** 程序从 stdin 读取 `你好`，调用 Ollama 进行翻译并流式输出。

#### Scenario: 通过 cat 管道输入
- **WHEN** 用户执行 `cat README.md | translate.exe -l English`
- **THEN** 程序读取整个文件内容并翻译。

#### Scenario: 显式无输入源且无 stdin
- **WHEN** 用户直接双击运行 `translate.exe` 而不提供任何参数
- **THEN** 保持原有行为：打印错误 "没有检测到有效的待翻译内容" 并以非 0 退出码返回。

#### Scenario: 显式提供位置参数时不读 stdin
- **WHEN** 用户执行 `translate.exe "hello"`
- **THEN** 程序使用位置参数 `hello` 作为输入，不尝试从 stdin 读取。

### Requirement: 剪贴板中文读取不再乱码

在 Windows 平台下，剪贴板读取必须支持中文等多字节字符，不能因 ANSI 代码页截断造成乱码。

#### Scenario: 复制中文后通过 -c 翻译
- **WHEN** 用户在编辑器中复制了一段中文，然后执行 `translate.exe -c -l English`
- **THEN** 剪贴板内容被正确读取（UTF-8），翻译结果中不出现 `???`、乱码或字符丢失。

#### Scenario: 剪贴板为空或读取失败
- **WHEN** 剪贴板中无可用文本或剪贴板被其他程序占用
- **THEN** 程序打印警告并按现有"无输入"流程报错退出。

#### Scenario: 非 Windows 平台剪贴板
- **WHEN** 用户在 Linux / macOS 上使用 `-c`
- **THEN** 沿用原有 `pbpaste || xclip` 行为，不引入回归。

### Requirement: 流式输出对非中文目标语言也保持编码稳定

`OllamaClient::Translate` 在流式回调里通过 `splitUtf8Chunks` 处理 UTF-8 切分；当遇到"非法起始字节"时，必须避免因为单纯 `++i` 跳过导致整段输出被吞掉。

#### Scenario: 包含 ASCII 标点 `?` `,` `.` 的英文翻译
- **WHEN** 翻译目标为 English
- **THEN** 用户能在终端看到完整英文译文，标点不丢失。

#### Scenario: 出现极少量被服务端切碎的非法字节
- **WHEN** 单个 chunk 中包含 1-2 个不能组成合法 UTF-8 序列的字节
- **THEN** 这些字节以原样回退到输出流（不会出现明显的大段空缺），并继续处理后续字节。

## MODIFIED Requirements

无（这是首个 spec，不修改已有需求）。

## REMOVED Requirements

无。
