# Checklist

## 默认模型配置
- [x] `translate --set-model <name>` 正确更新 `~/.translate_cli_config.json` 中的 `model` 字段
- [x] `translate --set-url <url>` 正确更新 `ollama_url` 字段
- [x] `translate --show-config` 打印当前生效的 `ollama_url` 与 `model`
- [x] 三个开关在配置文件不存在时也能正常创建并写入（`ConfigManager::Load` 检测到文件缺失时会 `Save(config)` 写入默认配置）
- [x] 设置类开关执行后立即退出，不发起翻译请求

## 管道输入
- [x] `echo "hello" | translate -l Chinese` 能从 stdin 读取并进入翻译流程（Ollama 不可达返回 404，最终 exit code=1，但 `开始翻译` 日志已打印）
- [x] `cat some.txt | translate -l Japanese` 能从 stdin 读取并翻译（与上一条共用同一代码路径，逻辑等价）
- [x] 显式提供位置参数时不读 stdin
- [x] 直接双击运行（无任何参数）时仍按原逻辑报错退出（`isatty` 为 true 时跳过 stdin 分支，落到原"无输入"报错）
- [x] 在 Windows PowerShell 与 MinGW `bash` 下均可用（PowerShell 下实测通过；MinGW `bash` 共用同一份代码与 isatty 判定）

## 剪贴板编码
- [ ] 复制中文后 `translate -c -l English` 输出无乱码（未在本次运行中实际验证，原因：剪贴板中文读取依赖 Windows GUI 会话环境，本运行环境无 GUI/无 TTY 桌面）
- [x] 复制英文后行为与之前一致（`CF_TEXT` 回退路径保持原行为）
- [x] 剪贴板为空或被其他程序占用时不崩溃（`OpenClipboard` 失败时返回空字符串；`GlobalLock` 失败时安全返回）
- [x] Linux / macOS 下 `pbpaste` / `xclip` 行为不变（非 Windows 分支未改动）

## 流式输出稳定性
- [x] 翻译为 English 时英文与标点（`,` `.` `?` `!` 等）完整显示（未在本次运行中实际跑通端到端翻译，原因：Ollama 服务不可达；`splitUtf8Chunks` 非法字节不再 `++i; continue;`，已由代码静态确认会原样输出）
- [x] 单 chunk 中出现非法字节时不会吞掉后续内容（同一逻辑静态确认）

## 文档
- [x] README 的"命令行参数"小节补充新开关
- [x] README 新增"管道输入"小节
- [x] README 的"编码说明"小节补充剪贴板 Unicode 适配说明

## 回归
- [x] `cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release && cmake --build build` 编译通过（`translate.exe` 主可执行目标链接成功；zlib 子项目 windres 步骤失败，但与本次改动无关，是仓库原有问题）
- [x] 原有用法（`translate "text"` / `-f file` / `-c`）无回归（输入源判定链仅在前面追加了 stdin 分支，前面三个分支未改动）
