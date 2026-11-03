# 凭据加密与迁移（P2-3）

实施日期：2026-10-26。`Oran7CredentialStore` 仅编译进 Oran7Core；Windows 当前用户范围的 CryptProtectData/CryptUnprotectData，带 CRYPTPROTECT_UI_FORBIDDEN，不使用 LOCAL_MACHINE，不弹出系统解密 UI，不引入自定义密码算法。

## 保护范围

| 分组 | 加密字段 | 仍保留为普通配置 |
|---|---|---|
| netease | 整个 netease_auth（含 MUSIC_U、__csrf、nickname 与未知扩展） | — |
| bilibili | 整个 bilibili_auth（含 sessdata、bili_jct、用户 ID/名称与未知扩展） | — |
| recording | whip_url、bearer_token、stun_url、turn_url、turn_user、turn_password、signaling_url、signaling_user、signaling_password | 模式、尺寸、帧率、码率、音频开关、输出目录/前缀、room_id/publisher_id 等非认证参数 |
| legacy | user.password、非占位值的 user.username（若旧配置存在） | auto_login、remember_password 与默认占位用户名 default；旧 TCP 模块继续停用 |

连接 URL 可能将认证信息放在 userinfo、路径或查询里，所以完整保护，而非只移除已知查询参数。不扫描或修改用户 SQLite 数据，不激活旧服务端，不改 WHIP/浏览器登录协议。

## 文件与接口

普通配置路径和 schemaVersion=2 保持不变。业务 snapshot/getValueQVariant 继续得到内存中的完整值，updateConfig/setValueQVariant 仍走现有校验；只有配置管理器负责持久化，禁止另行将 snapshot().toJson() 直接写盘。

普通 config.json 中的非空凭据迁出后，由 credentialStorage.version=1、credentialStorage.groups 的分组 UUID 引用代替。空的默认凭据不必创建密文文件。

密文目录为 `<配置文件完整路径>.credentials/`，生产默认是 `<exe>/../Config/config.json.credentials/`，文件为 `<UUID>.dpapi`。文件头 ORAN7-DPAPI-1，后面只有 DPAPI 密文；加密内容包含独立版本、分组标识和数据，分组还通过 DPAPI entropy 绑定，交换两组引用不能解密。UUID 严格校验，配置引用不能构造目录跳转。

`Oran7DpapiCookiePersistence` 取代旧明文 AppJsonCookiePersistence；默认接全局配置管理器，也支持注入独立 Manager 供检查使用。CookieStore 的 load/save 会报告存储错误；B 站 saveCookies 现在立即提交加密保存。录制设置继续沿用已有聚合提交及 ok/msg 错误回滚。

## 迁移与原子性

下次启动程序时，配置首次加载会识别旧明文，并按下列顺序迁移；本次开发验证不读取或迁移用户真实凭据。

1. 将变化的凭据组写入新的、不可变的密文代际，QSaveFile 原子提交，关闭句柄后实际解密回读并校验。
2. 按 P1-5 约定，首次覆盖已有 config.json 前保留原字节备份；备份失败则不替换普通配置。
3. 移除普通配置中的凭据，连同新引用通过 QSaveFile 原子提交；成功后才更新内存中的已提交引用与基线。

普通配置是唯一提交点。加密文件写好但步骤 2/3 失败或进程中断，旧配置仍引用旧密文，原登录态可恢复；未引用的新文件只是孤立密文，重启不会自动采用。保存失败保留内存修改，可重试。没有在两个文件之间依赖覆盖顺序的半更新状态。

配置保存使用非阻塞 QLockFile；另一个实例已改变凭据引用时，旧实例保存会报错并要求重启，避免普通设置保存复活旧登录态。业务快照仍不得跨异步任务缓存后整体写回。

## 失败、重新登录与退出

- 密文缺失、损坏、用途不匹配或不属于当前 Windows 用户：该组不恢复，credentialError(group) 可查询；其他组和普通参数仍可用。存在密文引用时绝不回退旧明文。普通配置保存不会覆盖不可解密的密文或清空其引用。
- 用户重新登录/显式修改该组参数：创建新密文并切换对应组引用，其他组保持原引用。旧文件不原地覆盖。
- 退出登录：清空运行期登录态并提交空密文代际，重启后保持退出状态；失败会报告存储错误，可以重试。resetToDefaults 也会清空当前各组凭据。
- 凭据引用格式或普通配置中的存储版本未知：关闭凭据恢复并禁止覆盖原配置，保留原件供新版或人工恢复。
- 更换 Windows 账户或迁移到另一台机器不应依赖复制密文恢复登录；在目标环境重新登录/设置参数。真实跨账户切换没有在本次检查中执行，检查用不同 entropy 的实际 DPAPI blob 覆盖同一解密失败分支。

退出登录清除的是当前有效凭据，不等于擦除历史副本。已有明文备份、首次迁移的原件备份、旧密文和孤立密文均保留，不自动销毁用户文件。需要彻底清理历史副本时另行明确范围。

## 历史副本与打包

需要人工评估的范围是 `build/Config/config.json.bak-*`、旧 build/Config 下的配置备份、用户手动备份及历史重构材料中的配置副本。仅对当前 build/Config 的文件名元数据生成了 `build/refactor-work/P2-3/historical-backup-inventory.json`；没有读取这些文件的内容，不能据此判定所有历史副本都已加密。

当前 ReleasePackage 脚本递归复制时排除 Config、config.json.credentials、Logs、refactor-work、tests，以及 config.json、config.json.bak-*、写锁、*.dpapi 和 *.dmp。保留原脚本编码；未执行会清空输出目录的打包流程。其他手动复制或旧打包脚本不在本次验证范围内。

## 验证

新增 `ctest --preset checks -R credentials`：真实 DPAPI 往返/完整性/分组绑定、旧格式启动迁移、内存快照、未知键保留、无明文落盘、重复迁移、实际 CookiePersistence、退出登录、密文损坏、无明文回退、按组重新登录、不可写目录、Windows 句柄阻止最终提交、提交失败重启恢复、旧实例覆盖保护与未知版本保护。

26/26 项正式离线 CTest 通过（70.18 秒）；credentials 含 101 项断言。Release 构建通过，编译数据库确认存储仅由 Oran7Core 持有一份，系统导入包含 CRYPT32。实际登录、重启恢复和用户设置由用户启动主程序验收。
