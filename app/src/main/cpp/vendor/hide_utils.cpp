// solistClear —— 把指定 .so 从 linker 簿记里卸掉。
// 该实现从作者内部项目中脱敏移植：去掉了与本项目无关的内存字符串改写
// 辅助函数和其它特性入口，核心 soinfo 卸载流程保持原样（已在实机长期验证）。
// 依赖 vendor/ 下的 elf_util（源自 LSPosed/SandHook，GPL-3.0）、soinfo 布局
// 副本和 memory_utils。

#include <sys/mman.h>
#include <pthread.h>
#include <dlfcn.h>
#include "string"
#include "log.h"
#include "elf_util.h"
#include "list"
#include <unistd.h>
#include <vector>
#include "soinfo.h"
#include "memory_utils.h"
#include "hide_utils.h"

using namespace std;

namespace hide {
    namespace {
        // bionic ProtectedDataGuard —— g_soinfo_allocator / g_namespace_allocator
        // 等分配器底层是受保护页（默认 PROT_READ），构造时 mprotect 成 RW，
        // 析构时还原。我们要直接写 target->size / constructors_called，必须包它。
        class ProtectedDataGuard {
        public:
            ProtectedDataGuard() {
                if (ctor != nullptr) (this->*ctor)();
            }

            ~ProtectedDataGuard() {
                if (dtor != nullptr) (this->*dtor)();
            }

            static bool setup(const SandHook::ElfImg &linker) {
                ctor = MemFunc{.data = {.p = reinterpret_cast<void *>(linker.getSymbAddress(
                        "__dl__ZN18ProtectedDataGuardC2Ev")),
                        .adj = 0}}.f;
                dtor = MemFunc{.data = {.p = reinterpret_cast<void *>(linker.getSymbAddress(
                        "__dl__ZN18ProtectedDataGuardD2Ev")),
                        .adj = 0}}.f;
                return ctor != nullptr && dtor != nullptr;
            }

            ProtectedDataGuard(const ProtectedDataGuard &) = delete;

            void operator=(const ProtectedDataGuard &) = delete;

        private:
            using FuncType = void (ProtectedDataGuard::*)();

            static FuncType ctor;
            static FuncType dtor;

            union MemFunc {
                FuncType f;
                struct {
                    void *p;
                    std::ptrdiff_t adj;
                } data;
            };
        };

        ProtectedDataGuard::FuncType ProtectedDataGuard::ctor = nullptr;
        ProtectedDataGuard::FuncType ProtectedDataGuard::dtor = nullptr;

        // ─── linker 内部符号 ─────────────────────────────────────────────────
        // solist 链头 —— 用来按 name 查 target
        soinfo *solist = nullptr;

        // dlclose 主入口 —— do_dlclose 经 soinfo_from_handle 后直接调它。
        // file-scope static，符号 __dl__ZL13soinfo_unloadP6soinfo
        void (*soinfo_unload_fn)(soinfo *) = nullptr;

        // 模块装载/卸载计数器 —— dlpi_adds / dlpi_subs 的来源
        uint64_t *g_module_load_counter = nullptr;
        uint64_t *g_module_unload_counter = nullptr;

        // bionic 全局 dl 锁。do_dlclose 内部本来就持这把锁，
        // 我们绕过 do_dlclose 直接调 soinfo_unload，需要自己加。
        pthread_mutex_t *g_dl_mutex_ptr = nullptr;

        bool initialized = false;

        void setup() {
            SandHook::ElfImg linker("/linker64");

            ProtectedDataGuard::setup(linker);

            // solist 是 file-scope static 变量；getSymbAddress 返回变量地址，
            // 再 deref 一次拿到 head soinfo*
            auto **sl_addr = linker.getSymbAddress<soinfo **>("__dl__ZL6solist");
            solist = sl_addr ? *sl_addr : nullptr;
            if (solist == nullptr) {
                LOGE("solist not resolved");
            }

            soinfo_unload_fn = reinterpret_cast<decltype(soinfo_unload_fn)>(
                    linker.getSymbPrefixFirstAddress("__dl__ZL13soinfo_unloadP6soinfo"));
            if (soinfo_unload_fn == nullptr) {
                LOGE("soinfo_unload not resolved, hiding will be skipped");
            }

            g_module_load_counter = linker.getSymbPrefixFirstAddress<uint64_t *>(
                    "__dl__ZL21g_module_load_counter");
            g_module_unload_counter = linker.getSymbPrefixFirstAddress<uint64_t *>(
                    "__dl__ZL23g_module_unload_counter");

            // g_dl_mutex —— external 命名和 file-scope static 命名都试
            g_dl_mutex_ptr = linker.getSymbPrefixFirstAddress<pthread_mutex_t *>(
                    "__dl_g_dl_mutex");
            if (g_dl_mutex_ptr == nullptr) {
                g_dl_mutex_ptr = linker.getSymbPrefixFirstAddress<pthread_mutex_t *>(
                        "__dl__ZL10g_dl_mutex");
            }
            if (g_dl_mutex_ptr == nullptr) {
                LOGW("g_dl_mutex not resolved, will run without lock (race-prone)");
            }

            initialized = true;
        }

        void resetCounters(size_t load, size_t unload) {
            if (!initialized) {
                LOGE("not initialized");
                return;
            }
            if (g_module_load_counter == nullptr || g_module_unload_counter == nullptr) {
                LOGD("g_module counters not defined, skip reset");
                return;
            }
            auto loaded = *g_module_load_counter;
            auto unloaded = *g_module_unload_counter;
            if (loaded >= load) {
                *g_module_load_counter = loaded - load;
                LOGD("reset g_module_load_counter: %zx -> %zx",
                     (size_t) loaded, (size_t) *g_module_load_counter);
            }
            if (unloaded >= unload) {
                *g_module_unload_counter = unloaded - unload;
                LOGD("reset g_module_unload_counter: %zx -> %zx",
                     (size_t) unloaded, (size_t) *g_module_unload_counter);
            }
        }

        // 按地址定位目标 soinfo：传入本库任意函数地址（如 JNI_OnLoad），
        // 落在哪个 soinfo 的 [base, base+size) 区间内，哪个就是目标。
        // 不依赖 l_name —— APK 内嵌加载等场景下名字不可靠。
        soinfo *findTarget(uintptr_t addr) {
            for (soinfo *si = solist; si != nullptr; si = si->next) {
                if (si->base != 0 && si->size != 0 &&
                    addr >= si->base && addr < si->base + si->size) {
                    return si;
                }
            }
            return nullptr;
        }

        bool hideViaSoinfoUnload(uintptr_t anchor_addr) {
            if (!initialized) {
                LOGW("not initialized");
                return false;
            }
            if (soinfo_unload_fn == nullptr) {
                LOGE("soinfo_unload symbol missing, abort");
                return false;
            }

            soinfo *target = findTarget(anchor_addr);
            if (target == nullptr) {
                LOGW("no soinfo covers anchor 0x%lx", (unsigned long) anchor_addr);
                return false;
            }

            const char *target_name = target->link_map_head.l_name;
            LOGD("found target: soinfo=%p name=%s base=0x%lx size=0x%zx",
                 target, target_name ? target_name : "(null)",
                 (unsigned long) target->base, target->size);

            ProtectedDataGuard g;
            Memory::writeAddr(&target->size, (void *) 0);
            Memory::writeAddr(&target->constructors_called, (void *) 0);
            target->hide_set_gap_size(0);
            LOGD("triggering soinfo_unload(%p)", target);
            soinfo_unload_fn(target);
            LOGD("soinfo_unload returned");
            return true;
        }
    }  // namespace anon

    // RAII 锁守卫：包住整段操作。anon namespace 成员对所属 namespace 可见。
    class DlGuard {
    public:
        DlGuard() {
            if (g_dl_mutex_ptr != nullptr) {
                pthread_mutex_lock(g_dl_mutex_ptr);
                locked_ = true;
            }
        }

        ~DlGuard() {
            if (locked_) pthread_mutex_unlock(g_dl_mutex_ptr);
        }

        DlGuard(const DlGuard &) = delete;

        void operator=(const DlGuard &) = delete;

    private:
        bool locked_ = false;
    };

    void HideViaSoinfoUnload(uintptr_t anchor_addr) {
        hideViaSoinfoUnload(anchor_addr);
    }
}

void solistClear(uintptr_t anchor_addr) {
    hide::setup();

    // do_dlclose 内部本来就持 g_dl_mutex；我们绕过 do_dlclose 直接调 soinfo_unload，
    // 同样需要自己持锁，否则 dl_iterate_phdr / 另一个 dlopen 可能撞上
    // "size=0 / constructors_called=false" 之类的中间态。
    hide::DlGuard dl_guard;

    hide::HideViaSoinfoUnload(anchor_addr);

    // dlopen +1 load_counter；soinfo_unload +1 unload_counter。
    // dlpi_adds - dlpi_subs 看不出来（差值不变），但绝对值各 +1。
    // 都减回去，dlpi_adds / dlpi_subs 完全 = 注入前状态。
    hide::resetCounters(1, 1);
}