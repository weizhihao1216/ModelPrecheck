# MemLeak_A

Intentional memory leak sample for「内存泄漏监测」.

- `Model_Init`: allocates **64KB**, never freed in `Model_Destroy`
- `Model_Step`: allocates **1KB** every step and drops the pointer

Default UserMain (~100 steps) ≈ **164KB** leak per call. At 10k stress runs that would be ~1.6GB; the profiler may early-abort when Working Set growth exceeds the UI **内存上限(MB)** (default 256; 0 = unlimited).

Use UserMain performance / memory growth monitoring — expect `memoryLeakRateMBPer10k` well above the 5 MB/10k warn threshold.
