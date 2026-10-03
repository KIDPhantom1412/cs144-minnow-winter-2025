Stanford CS 144 Networking Lab
==============================

课程网站: https://cs144.stanford.edu

关于这个仓库
------------

这是个人学习 CS144 的仓库，采用 **AI 驱动的学习方式**：由 AI Agent
担任「指导老师」，负责讲解概念、解读骨架代码、分析报错和评审代码，
实验本身的代码由本人独立完成。具体约定写在 `AGENTS.md` 中。

相比官方仓库，本仓库额外添加了：

- `handouts/`：各 checkpoint 的实验讲义（`check0.pdf` ~ `check7.pdf`）
- `slides/`：课程幻灯片
- `AGENTS.md`：给 AI Agent 的角色定位与工作守则
- `.agents/skills/`：为 Agent 准备的技能（如 `read-pdf`，让 Agent
  能直接阅读上述 PDF 材料）

从零开始
--------

如果你 clone 了这个仓库，想从干净的起始状态自己动手做一遍：

```sh
git switch -c my-work check0-startercode
```

`check0-startercode` 分支指向「官方起始代码 + 上述课程材料」的状态，
尚未包含任何实验实现。从它建出自己的分支后，即可从 Checkpoint 0
开始（先读 `handouts/check0.pdf`）。

后续每个 checkpoint 的官方起始代码在 `check1-startercode` ~
`check7-startercode` 分支上；开始新 checkpoint 时，把对应分支合并
进你的工作分支即可：

```sh
git merge check1-startercode
```

构建与测试
----------

```sh
cmake -S . -B build                 # 配置构建系统
cmake --build build                 # 编译
cmake --build build --target test   # 运行测试
cmake --build build --target speed  # 运行性能基准
cmake --build build --target tidy   # 运行 clang-tidy 静态检查
cmake --build build --target format # 格式化代码
```
