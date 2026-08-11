package com.example.encrypt;

import android.webkit.WebView;
import android.webkit.WebViewClient;

// 独立顶层类：页面加载完成后注入外部图片（避免匿名类触发 d8 崩溃）
public class AppWebViewClient extends WebViewClient {

    @Override
    public void onPageFinished(WebView view, String url) {
        super.onPageFinished(view, url);
        // 页面就绪后，把待处理的图片 data URL 交给 JS
        String pending = MainActivity.pendingImageDataUrl;
        if (pending != null && pending.length() > 0) {
            MainActivity.pendingImageDataUrl = null;
            // 转义：data URL 里没有单引号（base64 只含 +/ 和字母数字），安全
            String js = "localStorage.setItem('pendingImage', '" + pending + "');" +
                        "if (window.loadExternalImage) window.loadExternalImage(localStorage.getItem('pendingImage'));";
            view.evaluateJavascript(js, null);
        }
    }
}
