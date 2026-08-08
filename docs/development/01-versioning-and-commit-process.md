# 版本与提交规范

## 1. 版本规则

项目采用语义化版本 `vMAJOR.MINOR.PATCH`：

- `MAJOR`：架构或固件包格式不兼容变更；
- `MINOR`：完成一个可验证的新阶段或新增功能；
- `PATCH`：不改变既有接口的缺陷修复和小幅改进。

在 `v1.0.0` 前，阶段性交付通常增加 `MINOR`，阶段内部修复增加 `PATCH`。

## 2. 每次提交的强制内容

每次提交必须同时说明：

1. 日期；
2. 对应版本；
3. 实现的功能或修复的问题；
4. 主要改动文件；
5. 实际执行的验证；
6. 踩坑、根因和解决办法；没有踩坑时明确填写“无”。

每次提交同步更新：

- 根目录 `README.md` 的当前版本或版本记录；
- 根目录 `CHANGELOG.md` 的最新记录；
- 必要时更新架构、硬件、升级或调试文档。

## 3. 提交说明格式

提交标题使用：

```text
<type>: <简明说明>
```

建议类型：

- `feat`：新增功能；
- `fix`：修复缺陷；
- `refactor`：不改变行为的重构；
- `docs`：文档；
- `test`：测试；
- `build`：构建系统；
- `chore`：维护工作。

提交正文使用仓库根目录 `.gitmessage` 模板。

## 4. 提交前检查

```cmd
git status
git diff --stat
git diff --check
```

固件功能提交还必须执行：

```cmd
cd /d D:\ZzlWorkDir\STM32\STM32F407\Codex_STM32F407\Firmware\Bootloader
python script\build.py --clean
python script\flash.py
```

根据功能补充串口输出、GPIO寄存器、协议测试或硬件在环结果。只写“测试通过”是不够的，必须记录测试了什么。

## 5. 上传版本流程

1. 完成功能和验证；
2. 更新 README 和 CHANGELOG；
3. 提交代码；
4. 创建带说明的版本标签；
5. 推送分支和标签；
6. 核对 GitHub 远端提交哈希和标签。

示例：

```cmd
git tag -a v0.1.0 -m "STM32F407 Bootloader baseline"
git push origin main
git push origin v0.1.0
```

未经编译和目标板验证的提交不能创建发布标签。
