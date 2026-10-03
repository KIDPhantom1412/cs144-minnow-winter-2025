---
name: read-pdf
description: 在 WSL 环境下读取 PDF 文件（提取文字或渲染页面为 PNG）。当需要阅读 PDF（如 handouts/checkN.pdf 实验文档）、提取 PDF 文本、或查看 PDF 中的图示（报文格式图、时序图等）时使用。
---

# read-pdf：在 WSL 中读取 PDF

AI agent 无法直接读取 PDF（二进制格式），统一使用本 skill 目录下的 `read-pdf.sh`。

## 实现方式

- 通过 **WSL 侧的 uv + PyMuPDF** 实现，全部在 WSL 内完成，不依赖 Windows 侧工具。
- Python 依赖由 uv 按 PEP 723 内联元数据（`scripts/read-pdf.py` 文件头）自动管理，无需全局安装任何 Python 包。
- `scripts/read-pdf.sh` 与 `scripts/read-pdf.py` 必须放在同一目录；脚本会自动定位同目录的 `read-pdf.py`，可在任意工作目录下调用。

## 用法

以下命令中的 `SKILL_DIR` 指本文件所在目录（如 `.agents/skills/read-pdf`）。

**读文字**（文本输出到 stdout 直接阅读；省略页码则输出全文）：

```bash
bash SKILL_DIR/scripts/read-pdf.sh text handouts/checkN.pdf [起始页] [结束页]
```

**看页面图示**（报文格式图、时序图等纯文本提取不到的内容）：

```bash
bash SKILL_DIR/scripts/read-pdf.sh render handouts/checkN.pdf 起始页 结束页 /tmp/<前缀> [dpi]
```

再用 ReadMediaFile 读取输出的 PNG（文件名为 `page-<页码>.png`）。dpi 默认 150。

## 注意事项

- 页码均为 1-based，end 为闭区间。
- 渲染产物一律放 `/tmp`，不进入项目目录。
- 首次运行时 uv 会自动解析并安装 PyMuPDF 到其缓存，属正常现象。
