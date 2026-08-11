package com.example.encrypt;

import android.app.Activity;
import android.content.ContentValues;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.util.Base64;
import android.webkit.JavascriptInterface;

import java.io.OutputStream;

// JS <-> 原生桥：接收 HTML 传入的 base64 图片并保存到相册
// 独立顶层类（避免内部类触发 d8 兼容问题）
public class JsBridge {

    private final Activity activity;

    public JsBridge(Activity activity) {
        this.activity = activity;
    }

    // HTML 调用：window.AndroidBridge.saveImage(base64DataUrl, filename)
    @JavascriptInterface
    public boolean saveImage(String dataUrl, String filename) {
        try {
            // dataUrl 形如 data:image/png;base64,xxxx
            int comma = dataUrl.indexOf(',');
            if (comma < 0) return false;
            String b64 = dataUrl.substring(comma + 1);
            byte[] bytes = Base64.decode(b64, Base64.DEFAULT);
            Bitmap bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.length);
            if (bmp == null) return false;

            if (filename == null || filename.isEmpty()) filename = "image_" + System.currentTimeMillis() + ".png";

            ContentValues values = new ContentValues();
            values.put(MediaStore.Images.Media.DISPLAY_NAME, filename);
            values.put(MediaStore.Images.Media.MIME_TYPE, "image/png");
            if (Build.VERSION.SDK_INT >= 29) {
                values.put(MediaStore.Images.Media.RELATIVE_PATH, "Pictures/Encrypt");
                values.put(MediaStore.Images.Media.IS_PENDING, 1);
            }

            Uri uri = activity.getContentResolver()
                    .insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values);
            if (uri == null) return false;

            OutputStream os = activity.getContentResolver().openOutputStream(uri);
            if (os == null) return false;
            boolean ok = bmp.compress(Bitmap.CompressFormat.PNG, 100, os);
            os.flush();
            os.close();
            bmp.recycle();

            if (Build.VERSION.SDK_INT >= 29) {
                values.clear();
                values.put(MediaStore.Images.Media.IS_PENDING, 0);
                activity.getContentResolver().update(uri, values, null, null);
            }
            return ok;
        } catch (Exception e) {
            return false;
        }
    }
}
