package com.example.lsposedxjuse;

import android.util.Log;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public final class HookEntry implements IXposedHookLoadPackage {
    private static final String PREFIX = "[LSPosed XJ] ";

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam param) {
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
                        }
                    });

            System.loadLibrary("xjhook");
            XposedBridge.log(
                    "LSPosedXjUse: " + NativeBridge.installNativeHook()
                            + " in " + param.processName);
        } catch (Throwable error) {
            XposedBridge.log("LSPosedXjUse: initialization failed: " + error);
        }
    }
}
