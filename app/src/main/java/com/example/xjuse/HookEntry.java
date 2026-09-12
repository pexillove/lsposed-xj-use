package com.example.xjuse;

import android.util.Log;

import java.util.concurrent.atomic.AtomicInteger;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public final class HookEntry implements IXposedHookLoadPackage {
    // 框架会在目标进程内对含 lsposed/xposed 关键词的 DEX 字符串做等长替换，
    // 前缀不能包含这些关键词，否则每个进程里前缀都会不同。
    private static final String PREFIX = "[XJ-USE] ";

    private static final AtomicInteger LOG_I_HITS = new AtomicInteger();
    private static volatile boolean reporting = false;
    private static String processName = "unknown";

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam param) {
        processName = param.processName;
        try {
            XposedHelpers.findAndHookMethod(
                    Log.class,
                    "i",
                    String.class,
                    String.class,
                    new XC_MethodHook() {
                        @Override
                        protected void beforeHookedMethod(MethodHookParam hookParam) {
                            String message = (String) hookParam.args[1];
                            hookParam.args[1] = PREFIX + String.valueOf(message);

                            int hits = LOG_I_HITS.incrementAndGet();
                            if ((hits == 1 || hits % 50 == 0) && !reporting) {
                                reporting = true;
                                try {
                                    XposedBridge.log(
                                            "XjUse: Log.i hook hits=" + hits
                                                    + " tag=" + hookParam.args[0]
                                                    + " in " + processName);
                                } catch (Throwable ignored) {
                                    // 上报失败不影响原日志调用
                                } finally {
                                    reporting = false;
                                }
                            }
                        }
                    });

            System.loadLibrary("xjhook");
            XposedBridge.log(
                    "XjUse: " + NativeBridge.nativeReport
                            + " in " + param.processName);
        } catch (Throwable error) {
            XposedBridge.log("XjUse: initialization failed: " + error);
        }
    }
}
