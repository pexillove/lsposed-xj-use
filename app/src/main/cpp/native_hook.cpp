#include <dlfcn.h>
#include <fcntl.h>
#include <jni.h>
#include <unistd.h>

#include <cstdio>
#include <mutex>
#include <string>

#include <android/log.h>

#include <dobby.h>

#include "vendor/hide_utils.h"

namespace {

// int open(const char *pathname, int flags, ...)
// 非 O_CREAT 调用不传第三参，x2 上的值内核会忽略，按三参直传即可。
using OpenFn = int (*)(const char *, int, mode_t);

std::mutex install_mutex;
OpenFn original_open = nullptr;

int replacement_open(const char *pathname, int flags, mode_t mode) {
    __android_log_print(ANDROID_LOG_INFO, "XjUse", "open %s", pathname);
    return original_open(pathname, flags, mode);
}

// DobbyXJ copy-page hook：解析 libc 的 open 安装 replacement。
std::string installOpenHookLocked() {
    std::lock_guard<std::mutex> lock(install_mutex);
    void *target = DobbySymbolResolver("libc.so", "open");
    if (target == nullptr) {
        return "open export was not found";
    }
    int result = DobbyXjHook(
            target,
            reinterpret_cast<dobby_dummy_func_t>(replacement_open),
            reinterpret_cast<dobby_dummy_func_t *>(&original_open));
    if (result != RS_SUCCESS || original_open == nullptr) {
        return "DobbyXjHook(open) failed";
    }
    return "DobbyXjHook(open) success";
}

void reportToJava(JNIEnv *env, const char *text) {
    if (env == nullptr) {
        return;
    }
    jclass bridge = env->FindClass("com/example/xjuse/NativeBridge");
    if (bridge == nullptr) {
        env->ExceptionClear();
        return;
    }
    jfieldID field = env->GetStaticFieldID(
            bridge, "nativeReport", "Ljava/lang/String;");
    if (field == nullptr) {
        env->ExceptionClear();
        env->DeleteLocalRef(bridge);
        return;
    }
    jstring value = env->NewStringUTF(text);
    env->SetStaticObjectField(bridge, field, value);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(bridge);
}

}  // namespace

// JNI_OnLoad 完成：安装 DobbyXJ open hook → solistClear 自卸载 → 结果写回
// NativeBridge.nativeReport。此后 Java 不再调用任何 native 方法
// （soinfo 已卸载，再解析符号会失败），报告只经 Java 字段传递。
// 如需临时禁用某项能力（例如定位加固应用崩溃），直接注释掉对应调用。
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    std::string report;
    if (!DobbyXjIsAvailable()) {
        report = "DobbyXJ provider unavailable; check arm64 and kernel module authorization";
    } else {
        report = installOpenHookLocked();
    }

    // 以 JNI_OnLoad 自身地址定位本库 soinfo，不依赖名字。
    solistClear(reinterpret_cast<uintptr_t>(&JNI_OnLoad));
    report += "; solistClear(&JNI_OnLoad) executed";

    reportToJava(env, report.c_str());
    __android_log_print(ANDROID_LOG_INFO, "XjUse", "%s", report.c_str());
    return JNI_VERSION_1_6;
}
