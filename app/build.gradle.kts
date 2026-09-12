plugins {
    id("com.android.application")
}

android {
    // namespace 避开 lsposed/xposed 关键词；applicationId 保持不变以便沿用已配置的作用域
    namespace = "com.example.xjuse"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        // appId 同样避开 lsposed/xposed 关键词；旧 appId 的模块需要先卸载
    applicationId = "com.example.xjuse"
        minSdk = 27
        targetSdk = 36
        versionCode = 1
        versionName = "1.0.0"

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_static"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    buildFeatures {
        buildConfig = false
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.6"
        }
    }
}

dependencies {
    compileOnly(files("libs/api-82.jar"))
}
