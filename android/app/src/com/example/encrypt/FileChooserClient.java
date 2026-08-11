package com.example.encrypt;

import android.app.Activity;
import android.net.Uri;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebView;

// 独立顶层类实现文件选择回调（避免内部类导致 d8 崩溃）
public class FileChooserClient extends WebChromeClient {

    // 静态回调：由 MainActivity 设置，接收文件选择结果
    public static ValueCallback<Uri[]> filePathCallback;

    @Override
    public boolean onShowFileChooser(WebView webView,
            ValueCallback<Uri[]> callback,
            FileChooserParams fileChooserParams) {
        filePathCallback = callback;
        try {
            Activity activity = MainActivity.instance;
            if (activity != null) {
                activity.startActivityForResult(fileChooserParams.createIntent(), MainActivity.REQ_FILE_CHOOSER);
                return true;
            }
        } catch (Exception e) {
            // ignore
        }
        filePathCallback = null;
        return false;
    }
}
