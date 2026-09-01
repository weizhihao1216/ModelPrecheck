# ConflictWeaponA / ConflictWeaponB
#
# 两个“第三方”示例模型，API 与 StandardWeapon 相同（Model_Init/Step/Destroy），
# 但故意争用同一命名互斥量与共享内存，用于验证「多型号并行」能否检出冲突。
#
# 预期现象（两型号各 1 实例一起跑）：
#   - 双方 Init 时共享 activeCount>1，Model_Init 返回 -2，多型号并行应为 FAIL。
#   - 单独压测 / 多线程只跑其中一个型号应为 PASS（Init 等待约 0.4s 确认无同伴）。
#
# 说明：旧版互斥量方案在「一线程先完整跑完再跑另一线程」时会错开，导致误 PASS；
# v1.1 改为 Init 窗口内检测并存。
#
# 构建产物：
#   build/sample_models/ConflictWeaponA/Release/ConflictWeaponA.dll
#   build/sample_models/ConflictWeaponB/Release/ConflictWeaponB.dll
#
# 在预检工具中添加两个型号，包路径分别指向上述两个目录（或含 .dll/.lib/.h 的安装目录），
# UserMain 可用默认模板；编译后做「多型号并行」。
