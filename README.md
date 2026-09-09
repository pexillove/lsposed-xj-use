# lsposed-xj-use

> 禁止将本项目用于任何违法犯罪活动。

独立、最小化的 Android arm64 LSPosed 模块模板，包含：

- 使用经典 Xposed API Hook `android.util.Log.i(String, String)`。
- 使用预构建 DobbyXJ 静态库执行 copy-page Native Hook。
- 不依赖任何 LSPosed、Dobby 或内核模块源码仓库。
- 自带 Gradle Wrapper 和仅编译期 Xposed API stub，可直接构建 APK。

本仓库不包含 LSPosed 安装包、内核模块或激活信息。

## 目录

```text
.
├── app/
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── assets/xposed_init
│       ├── cpp/
│       │   ├── CMakeLists.txt
│       │   ├── native_hook.cpp
│       │   └── third_party/dobby/
│       │       ├── include/dobby.h
│       │       └── lib/arm64-v8a/libdobby.a
│       ├── java/com/example/lsposedxjuse/
│       │   ├── HookEntry.java
│       │   └── NativeBridge.java
│       └── res/values/arrays.xml
├── xposed-api-stubs/
├── build.gradle.kts
├── settings.gradle.kts
└── gradlew
```

`xposed-api-stubs` 只参与编译，使用 `compileOnly` 连接，不会打进最终 APK。设备运行时
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
<string-array name="xposed_scope">
    <item>com.example.target</item>
</string-array>
```

该数组是模块建议作用域。安装后仍应在 LSPosed Manager 中检查并启用实际作用域。
`HookEntry` 只会进入 LSPosed 已为本模块选择的进程，因此不再维护第二份硬编码包名。

## 3. Java Hook

Java 示例位于：

```text
app/src/main/java/com/example/lsposedxjuse/HookEntry.java
```

它精确 Hook：

```java
android.util.Log.i(String tag, String message)
```

调用原方法前，示例把 message 改为：

```text
[LSPosed XJ] <原消息>
```

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
                param.args[1] = "[LSPosed XJ] " + String.valueOf(param.args[1]);
            }
        });
```

替换为业务方法时，应明确写出目标类、方法名和参数类型，不要批量 Hook 全部方法。

## 4. DobbyXJ Native Hook

Native 示例位于：

```text
app/src/main/cpp/native_hook.cpp
```

它执行以下流程：

1. 调用 `DobbyXjIsAvailable()` 检查 arm64 copy-page provider。
2. 用 `dlsym()` 获取当前进程的 `getpid` 地址。
3. 调用 `DobbyXjHook()` 安装 replacement。
4. replacement 调用原 trampoline，并保持返回值不变。
5. 主动调用一次目标函数，确认 replacement 已命中。

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
```

`DobbyXjHook()` 成功后，原函数必须通过返回的 trampoline 调用，不能在 replacement
中直接再次调用目标入口。replacement 的参数、返回值和 ABI 必须与目标函数一致。

模板用 `getpid()` 做无行为变化的安装自检。替换目标时，修改
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

构建系统把下面两个预构建文件直接作为依赖：

```text
app/src/main/cpp/third_party/dobby/include/dobby.h
app/src/main/cpp/third_party/dobby/lib/arm64-v8a/libdobby.a
```

不需要设置父仓库路径，也不需要下载 Dobby 源码。

当前静态库 SHA-256：

```text
50deea7a180e4de4036403e3945167476d04e3b9fa7a533b056dd2c933cf8c5d
```

发布或替换 `.a` 后，应同步更新这里的哈希并重新执行完整构建。

## 6. 安装和启用

安装 APK：

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

然后在 LSPosed Manager 中：

1. 启用 `LSPosed XJ Use`。
2. 选择目标应用作用域。
3. 确认内核模块已授权。
4. 强制停止并重新启动目标应用。

查看模块日志：

```bash
adb logcat | grep -F LSPosedXjUse
```

Native 安装成功时可看到类似：

```text
LSPosedXjUse: DobbyXjHook installed; Dobby=<VERSION>, pid=<PID>, hits=1
```

目标应用调用 `Log.i(String, String)` 时，消息前缀会变为
`[LSPosed XJ]`。

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
只热切换前台界面通常不足以重新加载模块。

### `UnsatisfiedLinkError`

确认目标进程为 arm64，并检查 APK：

```bash
unzip -l app/build/outputs/apk/debug/app-debug.apk | \
  grep 'lib/arm64-v8a/libxjhook.so'
```

### Java Hook 没有命中

模板只覆盖 `Log.i(String, String)`。`Log.i(String, String, Throwable)` 是另一个
overload；目标若调用 `Log.d`、`Log.w` 或直接调用 native logging，也不会命中本示例。

### Native Hook 安装失败

确认目标地址非空、函数 ABI 正确、目标 patch 范围没有与其他 Dobby Hook 重叠，并避免
在同一页面混用普通 Dobby 与 DobbyXJ patch。

## 9. 发布前检查

```bash
./gradlew --no-daemon clean :app:assembleDebug

unzip -l app/build/outputs/apk/debug/app-debug.apk | \
  grep 'lib/arm64-v8a/libxjhook.so'

unzip -l app/build/outputs/apk/debug/app-debug.apk | \
  grep 'de/robv/android/xposed' && exit 1 || true

shasum -a 256 \
  app/src/main/cpp/third_party/dobby/lib/arm64-v8a/libdobby.a
```

最后一项 APK 检查用于确认 compile-only API stub 没有被打包。

## License

本仓库模板使用 Apache License 2.0。预构建 DobbyXJ 静态库和 `dobby.h` 同样按照
Apache License 2.0 分发，许可证副本位于 `THIRD_PARTY_LICENSES/Dobby.txt`。
