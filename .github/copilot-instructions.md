# GitHub Copilot Instructions

## 语言规则 / Language Rules

- **回答语言**：所有回答、推理过程、分析说明均使用**中文**。
- **代码与配置**：代码、命令、文件名、配置项、注释（代码内）等保持**英文**，不做翻译。
- **文档**：项目文档（README、spec、AGENTS.md 等）保持**英文**，除非用户明确要求中文。

## Git 提交规则 / Git Commit Rules

每个**阶段性修改完成后**自动执行 `git add` + `git commit`，要求：

- **必须填写详细 message**，格式：
  ```
  <type>(<scope>): <short summary>

  <body — 说明做了什么、为什么这样做>

  <footer — 关联 issue / ticket（如有）>
  ```
- **type** 取值：`feat` / `fix` / `docs` / `refactor` / `chore` / `test` / `ci`
- **scope** 为受影响的模块或目录（如 `feeds/hostapd`、`profiles`、`patches`）
- **body** 用英文写，简洁描述变更内容和动机
- 不得使用 `git commit --amend` 或 `git push --force` 等破坏性操作，除非用户明确确认

### 阶段定义 / Phase Definition

| 阶段 | 触发条件 |
|------|---------|
| spec / plan | 规格或计划文件写完后 |
| implement | 每个功能模块代码完成后 |
| fix | 每次 bug fix 完成后 |
| docs | 文档更新完成后 |
| refactor | 重构完成后 |
