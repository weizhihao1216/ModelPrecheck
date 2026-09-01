# CompatibleWeaponC / CompatibleWeaponD
#
# 设计目标：多型号并行 + 单型号多线程都应 PASS。
# - 每线程 thread_local 状态（无全局单例踩踏）
# - C 与 D 无命名互斥量/共享内存，互不干扰
# - API 与 StandardWeapon 相同，可用默认 UserMain
#
# 构建产物（可直接当模型包目录）：
#   build/sample_models/CompatibleWeaponC/Release/
#   build/sample_models/CompatibleWeaponD/Release/
