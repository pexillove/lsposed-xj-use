package com.example.xjuse;

final class NativeBridge {
    private NativeBridge() {
    }

    // JNI_OnLoad 写入：DobbyXJ 安装结果 + solistClear 自卸载结果。
    // 库从 linker 簿记卸载后 Java 不能再调 native 方法，报告只走这个字段。
    static volatile String nativeReport = "native init did not run";
}
