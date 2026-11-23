# 本地整理与GitHub发布

本仓库为公开准备稿。默认不自动创建远程仓库、推送、打tag或发布Release。请先由维护者审核内容、署名和许可。

## 目录

```text
src/                    native桥与纯encoder
 tests/                  离线fake / fault injection测试
config/                 仅桥的通用INI
scripts/                编译器解析、可重复性检查、白名单打包
.github/workflows/      Windows x64构建/测试/上传artifact
 docs/                   中英文说明、架构、测试范围、发布步骤
README.md LICENSE NOTICE.md CHANGELOG.md CONTRIBUTING.md SECURITY.md
build.ps1 test.ps1
```

`build/` 和 `dist/` 为忽略的生成物。不会包含原版MU3Input、注入器、游戏/固件、现场日志或个人配置。测试中的fake原DLL只能用于离线测试，不可用作原IO分发。

## 本地验收

在Windows x64和MinGW-w64工具链下，从仓库目录运行：

```powershell
./scripts/check-reproducible.ps1 -Gcc 'D:/tools/mingw64/bin/gcc.exe'
./test.ps1
./scripts/package.ps1 -SkipBuild -Version v0.1.0
```

也可以直接 `./scripts/package.ps1 -Gcc <gcc.exe> -Version v0.1.0` 先构建、测试再打包。不要并行执行多个构建/打包脚本共享同一 `build/`。

可重复性是**同工具链、环境与源码**下重复编译PE文件的比较；不承诺不同GCC版本字节相同。生成ZIP带固定entry时间戳和外置SHA256；同样不承诺跨.NET/压缩库版本一致。输出未签名。

脚本白名单创建 `dist/MU3Input-LegacyBridge-v0.1.0-win64.zip` 和对应 `.sha256`，仅桥DLL、INI和公开文档/许可。另可执行 `./scripts/package-source.ps1 -Version v0.1.0` 生成纯源码ZIP，包含CI/源码/测试/文档而不包含任何DLL/EXE或`.git`历史。此二进制包不是完整源码仓库；本项目MIT不强制捆绑源码，但应在Release中链接相应commit/tag以便审查。

## 首次上传（维护者手动）

1. 只使用新整理的独立仓库/源码ZIP，**不要上传整个现场分析目录或旧验收ZIP**。
2. 核对MIT署名。当前为 `MU3Input LegacyBridge contributors`；若需更准确的著作权人名称请在发布前自行修改，勿为了公开填入不愿公布的实名。
3. 检查所有文件内容与Git暂存区，特别是`.gitignore`只防误加，不会清理已跟踪内容。
4. 在GitHub自行创建空仓库，建议名 `mu3input-legacy-bridge`，不要与其他工程历史合并。以下在新的独立目录执行，用户名/仓库地址由你替换：

   ```powershell
   git init
   git add .
   git status --short
   git diff --cached --check
   git diff --cached
   # 先检查本机 git user.name / user.email，必要时使用GitHub提供的noreply邮箱
   git commit -m "Initial public LegacyBridge source"
   git branch -M main
   git remote add origin https://github.com/YOUR-NAME/mu3input-legacy-bridge.git
   git push -u origin main
   ```

5. 首次push后检查Actions真实结果。当前workflow仅有 `contents: read` 并上传artifact，不自动创建Release。CI依赖外部actions/MSYS2及网络，它的绿色结果不代表硬件兼容。
6. 审核后再手动打tag、创建Release，附二进制ZIP和SHA256，标明单设备验证和未测范围。CI当前使用 `ci-<run_number>` 作artifact版本名，不会自动把tag变为发布版包名；正式Release可本地按上例构建对应tag。
7. 源码仓库保留源文件；二进制可放Release，不建议直接提交DLL/EXE。GitHub自动生成的源码archive会包含已跟踪文件，因此务必先检查暂存区。

## 每次发布检查

- [ ] build/test/同工具链重复PE构建通过，明确本地与远端CI的实际状态。
- [ ] 协议、ABI、原IO版本范围不扩大到未测设备。
- [ ] 包内无 `MU3Input.dll`（fake也不应出现）、固件、日志、截图、卡资料或个人路径。
- [ ] `README`、`NOTICE`、MIT、配置及文档完整，SHA256对应最终ZIP。
- [ ] 变更记录说明是否改变writer行为；若改变需重新做分阶段现场验证。
- [ ] 不宣称未做的完整游戏、长期、多设备或实际热插拔验证。
- [ ] 不运行上游二进制的来源保证推断，不承诺任何第三方许可。

上传的是源码与桥构建产物，不包含取得原IO或其他受限制软件的教程。
