# lsposed-xj-use

> 禁止将本项目用于任何违法犯罪活动。

独立、最小化的 Android arm64 LSPosed 模块模板，包含：

- 使用经典 Xposed API Hook `android.util.Log.i(String, String)`。
- 使用预构建 DobbyXJ 静态库执行 copy-page Native Hook。
- `JNI_OnLoad` 完成后自动执行 `solistClear`，把本模块 .so 从 bionic
  linker 的 solist / dlpi 计数里抹掉（自卸载）。
- 不依赖任何 LSPosed、Dobby 或内核模块源码仓库。
- 自带 Gradle Wrapper 和仅编译期官方 Xposed API 82，可直接构建 APK。

本仓库不包含 LSPosed 安装包、内核模块或激活信息。

## 关键词约束（重要）

**项目内（Java 包名、类名、字符串字面量、applicationId、资源名等）不要使用
`lsposed` / `xposed` 及其大小写变体字符串**。配套运行框架会在目标进程内对包含
这些关键词的 DEX 字符串做等长随机替换，可能产生兼容性问题：类名被替换后静态
JNI 绑定解析失败（`UnsatisfiedLinkError: No implementation found`），字符串字面量
被替换后每个进程内容都不同。必须保留的只有协议本身：`xposed_init` 资产文件名、
manifest 的 `xposedmodule` / `xposedminversion` / `xposeddescription` /
`xposedscope` meta-data 键名，以及代码中对 `de.robv.android.xposed.*` API 的
引用（框架会做一致映射）。

因此本模板使用 `com.example.xjuse` 包名 / appId、`[XJ-USE] ` 日志前缀和
`module_scope` 作用域数组名（manifest 的 `xposedscope` 按资源 ID 引用，数组名
可自由命名），均不含关键词。

## 目录

```text
.
├── app/
│   ├── libs/api-82.jar
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── assets/xposed_init            ← 文件名是协议要求，不能改
│       ├── cpp/
│       │   ├── CMakeLists.txt
│       │   ├── native_hook.cpp           ← JNI_OnLoad：hook 安装 + 自卸载
│       │   ├── vendor/                   ← 脱敏移植的自卸载实现
│       │   │   ├── hide_utils.{h,cpp}    ← solistClear（按函数地址定位）
│       │   │   ├── elf_util.{h,cpp}      ← /linker64 符号解析
│       │   │   ├── soinfo.h              ← bionic soinfo 布局副本
│       │   │   ├── memory_utils.{h,cpp}
│       │   │   └── log.h                 ← 日志宏已置空，vendor 无输出
│       │   └── third_party/dobby/
│       │       ├── include/dobby.h
│       │       └── lib/arm64-v8a/libdobby.a
│       ├── java/com/example/xjuse/
│       │   ├── HookEntry.java
│       │   └── NativeBridge.java
│       └── res/values/arrays.xml         ← module_scope 建议作用域
├── build.gradle.kts
├── settings.gradle.kts
└── gradlew
```

官方 `api-82.jar` 只参与编译，使用 `compileOnly` 连接，不会打进最终 APK。设备运行时
使用 LSPosed 提供的真实 Xposed API。

## 使用条件

- Android arm64 设备。
- 已安装、激活并启用配套 LSPosed。
- 已安装并启用配套内核模块。
- 已在 LSPosed Manager 中完成当前启动周期的内核模块授权。
- JDK 17 或更高版本；推荐 JDK 21。
- Android SDK 36、Build Tools 36.0.0。
- Android NDK `29.0.14206865`。
- CMake `3.31.6`。

模板只附带 `arm64-v8a` 的 `libdobby.a`，Gradle 也只构建 arm64。

## 1. 获取工程

```bash
git clone https://github.com/yizhiyonggangdexiaojia/lsposed-xj-use.git
cd lsposed-xj-use
```

确认 Java：

```bash
java -version
```

如需显式选择 JDK 21：

```bash
export JAVA_HOME="/path/to/jdk-21"
```

Android SDK 通常通过 `ANDROID_HOME` 指定：

```bash
export ANDROID_HOME="$HOME/Android/Sdk"
```

也可以创建不提交到 Git 的 `local.properties`：

```properties
sdk.dir=/absolute/path/to/Android/sdk
```

## 2. 设置默认作用域

编辑：

```text
app/src/main/res/values/arrays.xml
```

把示例包名替换为自己的目标包名：

```xml
<string-array name="module_scope">
    <item>com.example.target</item>
</string-array>
```

该数组是模块建议作用域。安装后仍应在 LSPosed Manager 中检查并启用实际作用域。
`HookEntry` 只会进入 LSPosed 已为本模块选择的进程，因此不再维护第二份硬编码包名。

注意：刚重装过模块 APK 或改过作用域时，首次冷启动目标应用可能因框架的模块路径
缓存而加载不到新模块；重设一次作用域或再启动一次即可。

## 3. Java Hook

Java 示例位于：

```text
app/src/main/java/com/example/xjuse/HookEntry.java
```

它精确 Hook：

```java
android.util.Log.i(String tag, String message)
```

调用原方法前，示例把 message 改为：

```text
[XJ-USE] <原消息>
```

并带一个重入保护的命中计数：每个进程首次命中和每 50 次命中时，通过
`XposedBridge.log` 上报一条 `XjUse: Log.i hook hits=<n> tag=<tag>`。

核心代码：

```java
XposedHelpers.findAndHookMethod(
        Log.class,
        "i",
        String.class,
        String.class,
        new XC_MethodHook() {
            @Override
            protected void beforeHookedMethod(MethodHookParam param) {
                String message = (String) param.args[1];
                param.args[1] = PREFIX + String.valueOf(message);
            }
        });
```

替换为业务方法时，应明确写出目标类、方法名和参数类型，不要批量 Hook 全部方法。

## 4. DobbyXJ Native Hook 与 solistClear 自卸载

Native 示例位于：

```text
app/src/main/cpp/native_hook.cpp
```

`System.loadLibrary("xjhook")` 触发 `JNI_OnLoad`，它按顺序完成：

1. **DobbyXJ 安装**：`DobbyXjIsAvailable()` 检查 arm64 copy-page provider 后，
   `DobbySymbolResolver("libc.so", "open")` 解析目标并 `DobbyXjHook()` 安装
   replacement——每次 `open(pathname, ...)` 的路径打到模块日志，透传原函数。
2. **solistClear 自卸载**：以 `JNI_OnLoad` 自身函数地址为锚点定位本库 soinfo
   （落在 solist 某个 soinfo 的 `[base, base+size)` 区间内即目标，不依赖名字），
   然后：
   - `size = 0`：`soinfo_free` 的 munmap 分支被跳过，代码段保持映射，已安装的
     Hook 继续生效；
   - `constructors_called = 0`：`soinfo_unload` 不再执行 DT_FINI / JNI_OnUnload；
   - 在 `ProtectedDataGuard`（受保护页临时放开写权限）与自持的 `g_dl_mutex`
     （避免与并发 dlopen / dl_iterate_phdr 撞上中间态）保护下，直接调用 linker
     内部符号 `soinfo_unload(soinfo*)`；
   - 把本次 dlopen / 卸载造成的 `g_module_load_counter` /
     `g_module_unload_counter`（即 `dlpi_adds` / `dlpi_subs`）各回退 1，计数
     完全回到注入前。
3. 结果字符串写入 `NativeBridge.nativeReport` 静态字段，由 `HookEntry` 通过
   `XposedBridge.log` 输出。

`xjhook` 摘除 soinfo 后仍保留映射，已安装 Hook 的 replacement 和 trampoline 会继续
执行。为避免 bionic 的 `[anon:atexit handlers]` 留下指向该未登记映射的回调，
`xjhook` 自身和预构建 `libdobby.a` 都以 `-fno-c++-static-destructors` 编译，最终
链接再用 `-nostartfiles` 删除 NDK CRT 自带的 `atexit` / `__cxa_finalize` 包装入口。
`.init_array` 仍正常执行，因此全局和函数局部静态对象照常初始化；变化仅是进程退出时
不再自动执行静态析构。普通自动对象仍在离开作用域时析构。当前产物经动态符号表、
重定位表和反汇编检查，不包含 `atexit`、`__cxa_atexit` 或 `__cxa_finalize`。

提醒：native hook 的安装和 solistClear 自卸载没有编译开关，`JNI_OnLoad` 里无条件
执行；如需临时禁用某项（例如定位加固应用崩溃），直接注释掉 `native_hook.cpp`
中 `JNI_OnLoad` 里的对应调用即可。

**自卸载之后 Java 不得再调用任何 native 方法**：soinfo 已归还分配器，静态 JNI
符号解析会失败。因此 `NativeBridge` 不含任何 native 方法，报告只经 Java 字段
传递。

`solistClear` 实现位于 `app/src/main/cpp/vendor/`，从作者内部项目脱敏移植（已实机
长期验证），linker 内部符号经 `elf_util`（源自 LSPosed / SandHook，GPL-3.0，
文件头保留原许可与署名）从 `/linker64` 解析；native hook 目标（libc 的 `open`）
经 `DobbySymbolResolver` 解析。整套实现是纯用户态 linker 数据结构操作，
**不与任何内核模块通讯**；vendor 的日志宏已置空，运行时无额外输出。

核心接口：

```cpp
bool DobbyXjIsAvailable();

int DobbyXjHook(
        void *address,
        dobby_dummy_func_t replacement,
        dobby_dummy_func_t *original);

int DobbyXjInstrument(
        void *address,
        dobby_instrument_callback_t callback);

int DobbyXjDestroy(void *address);

// vendor/hide_utils.h —— 按函数地址定位并卸载本库 soinfo
void solistClear(uintptr_t anchor_addr);
```

`DobbyXjHook()` 成功后，原函数必须通过返回的 trampoline 调用，不能在 replacement
中直接再次调用目标入口。replacement 的参数、返回值和 ABI 必须与目标函数一致。

模板用 `android_dlopen_ext`（等待 `loader.so`）和 `open` 做安装演示。替换目标时，修改
`native_hook.cpp` 中的目标解析、函数类型、replacement 和验证逻辑。

同一目标页不能混用普通 `DobbyHook()` 与 `DobbyXjHook()`。两个 patch 范围也不能重叠。
卸载 DobbyXJ Hook 时使用 `DobbyXjDestroy()`，不要混用 `DobbyDestroy()`。

## 5. 构建

macOS 或 Linux：

```bash
./gradlew --no-daemon :app:assembleDebug
```

Windows：

```powershell
.\gradlew.bat --no-daemon :app:assembleDebug
```

输出：

```text
app/build/outputs/apk/debug/app-debug.apk
```

构建系统把下面三个预构建文件直接作为依赖：

```text
app/libs/api-82.jar
app/src/main/cpp/third_party/dobby/include/dobby.h
app/src/main/cpp/third_party/dobby/lib/arm64-v8a/libdobby.a
```

不需要设置父仓库路径，也不需要下载 Dobby 源码。

当前静态库 SHA-256：

```text
507eba800895034841eea7137682ad51624fbfa63bf54fec1c905d101d324c1c
```

该库已启用 `Plugin.SymbolResolver`，可直接调用 `DobbySymbolResolver()`，原有
DobbyXJ 接口保持可用，并包含 `xjtmpmm` VMA naming：首次
`DobbyXjHook()` / `DobbyXjInstrument()` 通过 provider 校验后单向开启，对已有
Dobby arena 补标，并在后续 arena/candidate 创建时命名。符号解析不安装 Hook，也不
要求 KPM 授权；解析结果仍取决于目标库是否可见、文件是否可读及符号表是否保留。
Android arm64 上，新普通 arena 与 candidate 还会优先使用高地址 allocator；显式
near 分配仍保持距离约束并使用 `MAP_FIXED_NOREPLACE` 防止覆盖已有 VMA。

维护者重建静态库时，在 LSPosed-Irena 源码仓库根目录执行以下命令；普通模块开发者
继续使用随仓库提供的 `.a`，无需额外源码依赖：

```bash
export ANDROID_NDK_HOME="$HOME/Library/Android/sdk/ndk/29.0.14206865"
CMAKE="$HOME/Library/Android/sdk/cmake/3.31.6/bin/cmake"
BUILD_DIR="$PWD/.dbg/dobby-symbol-resolver-build"

"$CMAKE" -S tests/native-hook-module/src/main/cpp -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$HOME/Library/Android/sdk/cmake/3.31.6/bin/ninja" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-27 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
  -DPlugin.SymbolResolver=ON \
  -DCMAKE_CXX_FLAGS=-fno-c++-static-destructors
"$CMAKE" --build "$BUILD_DIR" --target dobby --parallel 6
"$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" \
  --strip-debug "$BUILD_DIR/dobby/libdobby.a"
install -m 644 "$BUILD_DIR/dobby/libdobby.a" \
  tests/lsposed-xj-use/app/src/main/cpp/third_party/dobby/lib/arm64-v8a/libdobby.a
```

本次制品基于 Dobby 源码 `1873a3a7780aa9f25914e8335df3316a6951bcdd`，使用 NDK 29、
API 27、arm64 Release 构建并去除调试信息。Dobby 和最终 `xjhook` 都使用
`-fno-c++-static-destructors`；`xjhook`
另以 `-nostartfiles` 去掉未使用的 CRT atexit 包装器，最终 ELF 不导入
`__cxa_atexit`。2026-09-22 已完成 archive 和 APK 重建以及 ELF 检查；该新产物尚未
执行设备回归。此前 2026-09-14 在 Pixel 6 / Android 14 上验证相同 DobbyXJ
代码的 `DobbyXjHook(open)` 成功，candidate 与普通 executable arena 分别位于
`0x7b7d1fe000`、`0x7b7d1ff000`，均显示为 `[anon:xjtmpmm]`。

官方 Xposed API 82 JAR SHA-256：

```text
f48c635f1c7469fdec0e00ad2ea0b7a6b2f5b55065784a35b7ca3a84615e8e25
```

发布或替换任一预构建文件后，应同步更新对应哈希并重新执行完整构建。

## 6. 安装和启用

安装 APK：

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

然后在 LSPosed Manager 中：

1. 启用 `XJ Use`。
2. 选择目标应用作用域。
3. 确认内核模块已授权。
4. 强制停止并重新启动目标应用。

查看模块日志：

```bash
adb logcat | grep -F XjUse
```

模块加载并完成安装与自卸载时可看到类似：

```text
XjUse: DobbyXjHook(android_dlopen_ext) installed; waiting for loader.so; solistClear(&JNI_OnLoad) executed in <进程名>
XjUse: DobbyXjHook(open) success              ← loader.so 加载完成后
XjUse: open <路径>                             ← open hook 命中，每次调用一条
```

目标应用调用 `Log.i(String, String)` 时，消息前缀会变为
`[XJ-USE]`。

## 7. 替换为自己的 Native 目标

已知动态符号可通过 `dlsym()` 获取：

```cpp
void *target = dlsym(RTLD_DEFAULT, "target_symbol");
```

指定库尚未加载时，应等待对应 `.so` 加载后再解析目标。无导出符号的函数通常需要先
取得模块基址，再加上当前版本对应的 offset；offset 必须与目标 APK/so 版本绑定，不能
跨版本复用。

replacement 示例：

```cpp
using Target = int (*)(int);
Target original_target = nullptr;

int replacement_target(int value) {
    return original_target(value);
}

int result = DobbyXjHook(
        target,
        reinterpret_cast<dobby_dummy_func_t>(replacement_target),
        reinterpret_cast<dobby_dummy_func_t *>(&original_target));
```

安装前检查 `target`，安装后检查 `result == RS_SUCCESS` 和
`original_target != nullptr`。

## 8. 常见问题

### `DobbyXJ provider unavailable`

逐项检查：

- 设备 ABI 为 arm64。
- 配套内核模块已安装并启用。
- 当前设备启动周期已经从 LSPosed Manager 执行“内核模块授权”。
- 模块作用域包含目标应用。

### 模块已安装但没有日志

检查 LSPosed Manager 中模块开关和作用域，强制停止目标应用后重新启动。修改作用域后
只热切换前台界面通常不足以重新加载模块；刚重装过模块 APK 时首次冷启动可能因框架
路径缓存而加载不到，重设一次作用域或再启动一次即可。

### `UnsatisfiedLinkError`

两种可能。先确认目标进程为 arm64，并检查 APK：

```bash
unzip -l app/build/outputs/apk/debug/app-debug.apk | \
  grep 'lib/arm64-v8a/libxjhook.so'
```

若库存在仍报 `No implementation found`，多半是模块 Java 包名 / 类名包含
`lsposed` / `xposed` 关键词，被框架在目标进程内等长改名导致静态 JNI 绑定失效——
参见“关键词约束”一节，把包名和类名改成不含关键词的形式。

### Java Hook 没有命中

模板只覆盖 `Log.i(String, String)`。`Log.i(String, String, Throwable)` 是另一个
overload；目标若调用 `Log.d`、`Log.w` 或直接调用 native logging，也不会命中本示例。
部分加固应用会压制自身 logcat 输出，此时 Java Hook 证据以模块日志里的
`XjUse: Log.i hook hits=...` 遥测为准。

### Native Hook 安装失败

确认目标地址非空、函数 ABI 正确、目标 patch 范围没有与其他 Dobby Hook 重叠，并避免
在同一页面混用普通 Dobby 与 DobbyXJ patch。

### 加固应用加载即崩溃

`solistClear` 只清理 linker 簿记（solist / dl_iterate_phdr / dlpi 计数），不改变
`/proc/self/maps` 中的文件映射行。对启动期扫描自身 maps 并校验未知库映射的加固应用
（如带完整性自检的金融类 App），加载任何额外 native 库都可能触发其自我保护，与
hook 内容无关。可临时注释掉 `JNI_OnLoad` 中的 hook 安装 / solistClear 调用甚至
`System.loadLibrary` 来定位；本模板不提供也不建议在此引入内核通讯或 maps 视图处理。

## 9. 发布前检查

```bash
./gradlew --no-daemon clean :app:assembleDebug

unzip -l app/build/outputs/apk/debug/app-debug.apk | \
  grep 'lib/arm64-v8a/libxjhook.so'

unzip -l app/build/outputs/apk/debug/app-debug.apk | \
  grep 'de/robv/android/xposed' && exit 1 || true

# 关键词残留检查：除 de/robv/android/xposed API 引用外不应命中
unzip -p app/build/outputs/apk/debug/app-debug.apk classes.dex | \
  strings -a | grep -iE 'lsposed|xposed' | sort -u

shasum -a 256 \
  app/src/main/cpp/third_party/dobby/lib/arm64-v8a/libdobby.a
```

第二项 APK 检查用于确认 compile-only Xposed API 没有被打包；第三项用于确认
“关键词约束”没有引入新的 `lsposed` / `xposed` 字符串（manifest 的协议键名除外）。

## License

本仓库模板使用 Apache License 2.0。官方 Xposed API 82、预构建 DobbyXJ 静态库和
`dobby.h` 同样按照 Apache License 2.0 分发，详见 `THIRD_PARTY_NOTICES.md`。

`vendor/elf_util.{h,cpp}` 源自 LSPosed / SandHook（GPL-3.0），文件头保留原许可
与署名；将其编入 APK 分发时该部分遵循 GPL-3.0 条款。`vendor/` 其余文件为作者
内部实现的脱敏移植版本。
