#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <mutex>

#include <dobby.h>

namespace {

using GetPid = pid_t (*)();

std::mutex install_mutex;
std::atomic<int> hook_hits{0};
GetPid original_getpid = nullptr;
bool install_attempted = false;
bool install_succeeded = false;

pid_t replacement_getpid() {
    hook_hits.fetch_add(1, std::memory_order_relaxed);
    return original_getpid != nullptr ? original_getpid() : -1;
}

jstring make_result(JNIEnv *env, const char *text) {
    return env->NewStringUTF(text);
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_lsposedxjuse_NativeBridge_installNativeHook(
        JNIEnv *env, jclass) {
    std::lock_guard<std::mutex> lock(install_mutex);

    if (install_attempted) {
        return make_result(
                env,
                install_succeeded
                        ? "DobbyXjHook is already installed"
                        : "DobbyXjHook installation already failed");
    }
    install_attempted = true;

    if (!DobbyXjIsAvailable()) {
        return make_result(
                env,
                "DobbyXJ provider unavailable; check arm64 and kernel module authorization");
    }

    void *target = dlsym(RTLD_DEFAULT, "getpid");
    if (target == nullptr) {
        return make_result(env, "getpid export was not found");
    }

    int result = DobbyXjHook(
            target,
            reinterpret_cast<dobby_dummy_func_t>(replacement_getpid),
            reinterpret_cast<dobby_dummy_func_t *>(&original_getpid));
    if (result != RS_SUCCESS || original_getpid == nullptr) {
        return make_result(env, "DobbyXjHook(getpid) failed");
    }

    auto hooked_getpid = reinterpret_cast<GetPid>(target);
    pid_t observed_pid = hooked_getpid();
    int observed_hits = hook_hits.load(std::memory_order_relaxed);
    if (observed_pid <= 0 || observed_hits < 1) {
        DobbyXjDestroy(target);
        original_getpid = nullptr;
        return make_result(env, "DobbyXjHook self-check failed");
    }

    install_succeeded = true;
    char status[160];
    std::snprintf(
            status,
            sizeof(status),
            "DobbyXjHook installed; Dobby=%s, pid=%d, hits=%d",
            DobbyGetVersion(),
            observed_pid,
            observed_hits);
    return make_result(env, status);
}
