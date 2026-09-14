# libdatachannel 源码说明

本目录 **完整提交到 Git 仓库**（包括 `deps/` 子模块），原因：

- **libdatachannel 的 submodules（`deps/`）下载极慢**  
  项目依赖 libjuice、libsrtp、usrsctp、plog 等子模块，国内网络环境克隆 GitHub submodules 经常超时/失败。

- **提交源码 ≠ 提交编译产物**  
  - 源码（`server/third_party/src/libdatachannel/`）：**提交**，便于团队直接构建。
  - 编译产物（`lib/`、`include/`、`build/`）：**不提交**（已在 `.gitignore` 排除），每个开发者本地编译。

---

## 使用方式

运行 `server/scripts/build-third-party.sh` 时：

- 如果 `deps/` 已存在 → 直接跳过 `git submodule` 步骤，立即编译
- 如果 `deps/` 缺失 → 自动执行 `git submodule update --init --recursive`（网络不佳时会很慢）

**推荐**：首次 clone 本仓库后，`deps/` 已随源码一起克隆完毕，无需额外操作。

---

## 版本管理

- 当前锁定版本：**v0.24.0**（见 `build-third-party.sh` 中的 `LDC_TAG`）
- 如需升级 libdatachannel：
  1. 修改 `build-third-party.sh` 中的 `LDC_TAG`
  2. 删除 `server/third_party/src/libdatachannel` 后重新运行脚本
  3. **提交新版本源码**到 Git

---

## 其他三方库为什么不提交源码？

- **OpenSSL / jsoncpp / cpp-httplib**：克隆速度快（Gitee 镜像），且体积相对小，无需提交源码。
- **libdatachannel**：submodules 复杂 + 网络不稳定，提交源码收益最大。
