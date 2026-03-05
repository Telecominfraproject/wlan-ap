# OpenSync Build Agent Skill

这个 Agent Skill 让 GitHub Copilot 能够理解和执行 OpenSync 固件构建任务。

## 已安装的文件

```
.github/skills/universal-build/
├── SKILL.md                    # Skill 定义和说明（Copilot 会读取这个）
├── universal_build_skill.sh    # 通用构建脚本
├── build_config.env            # 项目配置文件
└── README.md                   # 本文件
```

## 如何使用

一旦安装并启用，你可以在 GitHub Copilot Chat 中直接询问：

### 构建固件示例

```
"帮我构建 WF710G 固件，版本号是 12.2.6.25-WF710X-T2601203"
"Build WF710G firmware with version 12.2.6.25-WF710X-T2601203"
```

### 检查构建状态

```
"检查 WF710G 的构建状态"
"Is the WF710G build still running?"
```

### 清理构建

```
"清理 WF710G 的构建目录"
"Clean the build directory for WF710G"
```

### 查看配置

```
"显示当前的构建配置"
"Show the current build configuration"
```

## 自动激活

Copilot 会在以下情况自动加载这个 skill：

- 提到 "OpenSync"、"firmware" 或 "build"
- 询问关于 WF710G、WF810、WF710 的问题
- 需要版本管理或 Docker 容器操作
- 请求构建状态或清理操作

## 配置要求

### VS Code 设置

确保在 `.vscode/settings.json` 中启用了 Agent Skills：

```json
{
  "chat.useAgentSkills": true
}
```

### 系统要求

- Docker 已安装并运行
- 有权限访问 Docker socket
- wf710os:latest 镜像已构建或可访问

## 支持的项目

当前配置支持以下项目：

| 项目 | 模型名称 | Docker 镜像 |
|------|---------|-------------|
| WF710G | WF710G | wf710os:latest |
| WF810 | WF810 | wf710os:latest |
| WF710 | WF710 | wf710os:latest |

## 添加新项目

要添加新项目，编辑 `build_config.env`：

```bash
# 新项目配置
NEWPROJECT_MODEL_NAME="NewModel"
NEWPROJECT_CONTAINER_DIR="/src/build/path"
NEWPROJECT_MAKE_CMD="./build_command.sh"
NEWPROJECT_DOCKER_IMAGE="build-image:tag"
```

## 输出文件

构建过程会生成以下文件：

- `{MODEL}_{TIMESTAMP}.log` - 完整构建日志
- `{MODEL}_{TIMESTAMP}.errlog` - 错误日志

文件位于工作区根目录。

## 故障排除

### Skill 没有激活

1. 确认 `chat.useAgentSkills` 设置已启用
2. 重启 VS Code
3. 在 Copilot Chat 中明确提到构建或固件相关的关键词

### 构建失败

1. 检查 `.errlog` 文件
2. 验证 Docker 镜像存在：`docker images | grep wf710os`
3. 确认容器权限：`docker ps -a | grep docker_hughcheng`

### 找不到配置

1. 确认 `build_config.env` 文件存在
2. 检查项目名称是否在配置中定义
3. 验证环境变量格式：`{PROJECT}_MODEL_NAME`

## 更多信息

- [Agent Skills 官方文档](https://code.visualstudio.com/docs/copilot/customization/agent-skills)
- [agentskills.io 标准](https://agentskills.io/)
- [GitHub Copilot 文档](https://docs.github.com/copilot)
