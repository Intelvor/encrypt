package com.example.encrypt;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;
import android.webkit.WebSettings;
import android.webkit.WebView;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;

public class MainActivity extends Activity {

    public static final int REQ_FILE_CHOOSER = 1002;
    private static final int REQ_STORAGE = 1001;

    // 供 FileChooserClient 获取 Activity 启动文件选择器
    public static MainActivity instance;

    // 待注入页面的外部图片 data URL（页面加载完成后由 AppWebViewClient 消费）
    public static String pendingImageDataUrl = null;

    private WebView webView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        instance = this;

        webView = new WebView(this);
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        // 本地加密工具：禁止外部导航，只加载本地 assets
        // 页面加载完成后注入外部打开的图片
        webView.setWebViewClient(new AppWebViewClient());
        // 文件选择：由独立 FileChooserClient 处理 <input type="file">
        webView.setWebChromeClient(new FileChooserClient());
        // JS 桥：保存图片到相册等
        webView.addJavascriptInterface(new JsBridge(this), "AndroidBridge");
        webView.loadUrl("file:///android_asset/index.html");
        setContentView(webView);

        // 若从外部打开/分享图片，读取并暂存（页面就绪后注入）
        handleIncomingImage();

        // 请求存储权限（Android 6+ 动态权限）
        requestStoragePermission();
    }

    // 处理外部打开/分享的图片：读取内容转 base64 data URL，暂存待页面就绪后注入
    private void handleIncomingImage() {
        Intent intent = getIntent();
        if (intent == null) return;
        Uri uri = null;
        if (Intent.ACTION_VIEW.equals(intent.getAction())) {
            uri = intent.getData();
        } else if (Intent.ACTION_SEND.equals(intent.getAction())) {
            uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
        }
        if (uri == null) return;

        try {
            InputStream is = getContentResolver().openInputStream(uri);
            if (is == null) return;
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int n;
            while ((n = is.read(buf)) > 0) baos.write(buf, 0, n);
            is.close();
            byte[] bytes = baos.toByteArray();
            String b64 = Base64.encodeToString(bytes, Base64.NO_WRAP);
            pendingImageDataUrl = "data:image/png;base64," + b64;
        } catch (Exception e) {
            pendingImageDataUrl = null;
        }
    }

    private void requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= 23) {
            boolean read = checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED;
            boolean write = checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    == PackageManager.PERMISSION_GRANTED;
            if (!read || !write) {
                requestPermissions(
                    new String[]{
                        android.Manifest.permission.READ_EXTERNAL_STORAGE,
                        android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                    }, REQ_STORAGE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        // 权限结果无需额外处理
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_FILE_CHOOSER) {
            if (FileChooserClient.filePathCallback != null) {
                Uri[] results = null;
                if (resultCode == Activity.RESULT_OK && data != null) {
                    if (data.getData() != null) {
                        results = new Uri[]{ data.getData() };
                    } else if (data.getClipData() != null) {
                        int count = data.getClipData().getItemCount();
                        results = new Uri[count];
                        for (int i = 0; i < count; i++)
                            results[i] = data.getClipData().getItemAt(i).getUri();
                    }
                }
                FileChooserClient.filePathCallback.onReceiveValue(results);
                FileChooserClient.filePathCallback = null;
            }
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (instance == this) instance = null;
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }
}
