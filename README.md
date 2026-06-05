# TranslateCLI

基于 Ollama 的命令行翻译工具，使用 C++17、CMake、cpr、nlohmann/json 和 CLI11 实现。

## 功能特性

- 支持直接传入文本翻译
- 支持从文件读取内容翻译
- 支持从剪贴板读取内容翻译
- 支持流式输出翻译结果
- 支持通过配置文件保存 Ollama 地址和默认模型
- Windows 控制台已适配 UTF-8 输出，减少中文乱码问题
- 文件读取支持 UTF-8 BOM、UTF-16 LE、UTF-16 BE 自动识别

## 目录结构

```text
.
├─ include/        头文件
├─ src/            源码
├─ scripts/        构建 / 打包脚本
├─ build/          构建输出目录
├─ dist/           自包含发布目录 (运行 scripts/release.ps1 后生成)
└─ CMakeLists.txt  CMake 构建配置
```

## 环境要求

- C++17 编译器
- CMake 3.14 及以上
- 可访问网络以便 CMake 首次拉取依赖
- 本地已安装并运行 Ollama

本项目当前通过 `FetchContent` 自动拉取以下依赖：

- nlohmann/json 3.11.3
- cpr 1.10.5
- CLI11 2.4.1

## 构建

### Windows + MinGW Makefiles

```powershell
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

如果你使用的是 MSYS2 / UCRT64，也可以直接在 `build` 目录下执行：

```powershell
mingw32-make
```

### Visual Studio / MSVC

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 运行

生成的可执行文件名为：

```text
build/translate.exe
```

常见用法如下。

### 直接翻译文本

```powershell
.\build\translate.exe "你好，世界"
```

### 指定目标语言

```powershell
.\build\translate.exe -l English "今天天气很好"
```

### 从文件读取

```powershell
.\build\translate.exe -f .\input.txt -l Japanese
```

### 从剪贴板读取

```powershell
.\build\translate.exe -c -l English
```

### 管道输入

当且仅当没有提供位置参数、`-f` 或 `-c` 时，程序会自动检测 stdin；
若 stdin 不是交互终端（即被管道 / 重定向），会从中读取全部内容作为待翻译文本。

```powershell
echo "你好" | .\build\translate.exe -l English
Get-Content .\input.txt | .\build\translate.exe -l English
```

### 临时覆盖模型

```powershell
.\build\translate.exe -m qwen2.5:14b "请把这句话翻译成英文"
```

### 查看帮助

```powershell
.\build\translate.exe --help
```

### 设置 / 查看默认配置

```powershell
# 把默认模型写入配置文件（执行后立即退出）
.\build\translate.exe --set-model qwen2.5:14b

# 把默认 Ollama URL 写入配置文件（执行后立即退出）
.\build\translate.exe --set-url http://192.168.1.10:11434

# 打印当前生效的配置
.\build\translate.exe --show-config
```

> 配置文件不存在时，上述 `--set-*` 命令会自动创建 `~/.translate_cli_config.json`。

## 命令行参数

```text
translate.exe [OPTIONS] [text]

Positionals:
  text                     要翻译的文本

Options:
  -f, --file              从文件读取内容进行翻译
  -l, --lang              目标语言，默认 Chinese
  -m, --model             指定使用的模型，覆盖配置文件
  -c, --clipboard         从剪贴板读取内容进行翻译
  --set-model <name>      将默认模型写入配置文件后退出
  --set-url <url>         将默认 Ollama URL 写入配置文件后退出
  --show-config           打印当前生效的 ollama_url / model 后退出
  -h, --help              显示帮助
```

输入源优先级：位置参数 > `-f` 文件 > `-c` 剪贴板 > stdin（仅当 stdin 不是 TTY 时）。

## 配置文件

程序会在用户目录下读取或创建配置文件：

```text
~/.translate_cli_config.json
```

Windows 下通常对应：

```text
C:/Users/你的用户名/.translate_cli_config.json
```

示例内容：

```json
{
    "ollama_url": "http://localhost:11434",
    "model": "qwen2.5:7b"
}
```

## 输出说明

- 程序会先输出日志
- 翻译正文采用流式输出
- 翻译前后会打印分隔线 `---`

## 编码说明

为减少中文乱码，项目已做以下处理：

- Windows 控制台启动时切换到 UTF-8 代码页
- 源码文件使用 UTF-8 编码
- 文件输入支持 UTF-8 BOM、UTF-16 LE、UTF-16 BE 自动转换
- Windows 剪贴板读取已支持中文等多字节字符（`CF_UNICODETEXT` → `WideCharToMultiByte(CP_UTF8, ...)`），如 `CF_UNICODETEXT` 不可用会自动回退到 `CF_TEXT`

如果你在 PowerShell 中仍看到乱码，可先执行：

```powershell
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
```

## 注意事项

- 运行前请确保 Ollama 服务可访问
- 首次构建会下载第三方依赖，时间可能较长
- 在 Windows + MinGW 环境下，`CMakeLists.txt` 已包含对 curl/zlib 资源编译和编码兼容的修复

## 发布与打包

> `translate.exe` 不是单文件可执行程序，运行时需要 3 个动态库：
> `libcpr.dll` / `libcurl.dll` / `libzlib.dll`。
> 把 `translate.exe` 单独拷到其他目录会因找不到 dll 而启动失败。

请使用项目自带的发布脚本，它会把 `translate.exe` 与 3 个 dll 一起打包成自包含目录：

```powershell
# 在项目根目录 (PowerShell)
.\scripts\release.ps1
```

脚本会：

1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`（首次会拉 cpr/curl/zlib/json/cli11）
2. `cmake --build build -j`
3. 把 `build/translate.exe` 和 3 个 dll 复制到 `dist/bin/`
4. 用 `Compress-Archive` 打成 `translate-cli-windows-x64.zip`

完成后 `dist/bin/` 目录结构如下，整个目录拷贝到任意位置都能直接运行：

```text
dist/bin/
├─ translate.exe
├─ libcpr.dll
├─ libcurl.dll
└─ libzlib.dll
```

可选参数：

```powershell
# 指定生成器
.\scripts\release.ps1 -Generator "Visual Studio 17 2022"

# 跳过构建, 复用已有 build/
.\scripts\release.ps1 -SkipBuild

# 自定义输出 zip 路径
.\scripts\release.ps1 -Output D:\releases\translate-cli.zip
```

> 为什么脚本不直接用 `cmake --install`？因为 `FetchContent` 拉下来的
> zlib 子项目把 install 路径硬编码为 `C:/Program Files (x86)/TranslateCLI`，
> 在 `cmake --install` 时会触发权限错误。脚本改成直接 `Copy-Item` 4 个文件，
> 简单稳定，不依赖子项目的 install 行为。

## 后续可扩展方向

- 增加 OpenAI 兼容接口切换
- 增加源语言自动检测
- 增加批量文件翻译
- 增加结果写回文件或复制回剪贴板
