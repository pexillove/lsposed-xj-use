#pragma once

#include <cstdint>

// solistClear：在 JNI_OnLoad 完成后调用，传入本库任意函数地址
// （如 JNI_OnLoad 自身），按 [base, base+size) 区间定位 soinfo 并把
// 它从 bionic linker 的 solist / dlpi 计数里抹掉（不跑析构、不解除映射）。
void solistClear(uintptr_t anchor_addr);
