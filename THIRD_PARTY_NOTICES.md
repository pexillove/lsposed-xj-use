# Third-party notices

## Dobby

The following files are redistributed under Apache License 2.0:

- `app/src/main/cpp/third_party/dobby/include/dobby.h`
- `app/src/main/cpp/third_party/dobby/lib/arm64-v8a/libdobby.a`

The archive contains the Android arm64 DobbyXJ API used by this template.
Its license text is available at `THIRD_PARTY_LICENSES/Dobby.txt`.

## Xposed API declarations

`xposed-api-stubs` contains compile-time-only declarations for the small subset
of the classic Xposed API used by this example. The module is connected through
Gradle `compileOnly` and is not packaged in the APK. At runtime these classes
are supplied by LSPosed.
