# Third-party notices

## Dobby

The following files are redistributed under Apache License 2.0:

- `app/src/main/cpp/third_party/dobby/include/dobby.h`
- `app/src/main/cpp/third_party/dobby/lib/arm64-v8a/libdobby.a`

The archive contains the Android arm64 DobbyXJ API used by this template.
Its license text is available at `THIRD_PARTY_LICENSES/Dobby.txt`.

## Xposed API 82

`app/libs/api-82.jar` is the official classic Xposed API 82 artifact published
by rovo89 under Apache License 2.0. It is connected through Gradle
`compileOnly` and is not packaged in the APK. At runtime these classes are
supplied by LSPosed.

SHA-256:

```text
f48c635f1c7469fdec0e00ad2ea0b7a6b2f5b55065784a35b7ca3a84615e8e25
```

The Apache License 2.0 text is available in the repository root `LICENSE`.
