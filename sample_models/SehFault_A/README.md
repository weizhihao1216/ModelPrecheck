# SehFault_A / SehFault_B

Intentional SEH / hardware-exception samples for ModelPrecheck.

| Model | Fault site | Expected SEH |
|-------|------------|--------------|
| SehFault_A | `Model_Step` | ACCESS_VIOLATION (null deref) |
| SehFault_B | `Model_Init` | INT_DIVIDE_BY_ZERO |

API: `Model_Create / Init / Step / Destroy` + `WeaponObject` for Mo*.

Harness should report `exceptionOccurred=YES` when run via SafeCall wrappers.
