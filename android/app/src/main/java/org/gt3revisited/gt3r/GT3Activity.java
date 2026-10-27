package org.gt3revisited.gt3r;

import android.Manifest;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.RoundedCorner;
import android.view.View;
import android.view.WindowInsets;

import org.libsdl.app.SDLActivity;

import java.io.File;

/** Hosts the native engine. SDL is statically linked into libmain.so. */
public class GT3Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }

    // The disc image comes from the system file picker as a content:// URI.
    // Keep read access across app restarts so the remembered disc (and the
    // recent-discs list) still opens next launch.
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (resultCode == RESULT_OK && data != null && data.getData() != null
                && (data.getFlags() & Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION) != 0) {
            Uri uri = data.getData();
            try {
                getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (SecurityException e) {
                Log.w("GT3R", "Could not keep access to " + uri, e);
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    /**
     * Called from native code (src/platform/display_insets.cpp): the display
     * cutout per edge and the largest rounded-corner radius, in pixels, as
     * {left, top, right, bottom, radius}; null when not known yet. Unlike
     * SDL's safe area this leaves out the system-gesture zones.
     */
    public int[] displayInsets() {
        if (Build.VERSION.SDK_INT < 28) return null;
        View decor = getWindow() != null ? getWindow().getDecorView() : null;
        WindowInsets insets = decor != null ? decor.getRootWindowInsets() : null;
        if (insets == null) return null;
        int[] out = new int[5];
        DisplayCutout cut = insets.getDisplayCutout();
        if (cut != null) {
            out[0] = cut.getSafeInsetLeft();
            out[1] = cut.getSafeInsetTop();
            out[2] = cut.getSafeInsetRight();
            out[3] = cut.getSafeInsetBottom();
        }
        if (Build.VERSION.SDK_INT >= 31) {
            int[] positions = {RoundedCorner.POSITION_TOP_LEFT, RoundedCorner.POSITION_TOP_RIGHT,
                               RoundedCorner.POSITION_BOTTOM_LEFT, RoundedCorner.POSITION_BOTTOM_RIGHT};
            for (int p : positions) {
                RoundedCorner c = insets.getRoundedCorner(p);
                if (c != null) out[4] = Math.max(out[4], c.getRadius());
            }
        }
        return out;
    }

    /**
     * Called from native code (src/platform/shared_storage.cpp): the GT3Revisited
     * folder on the shared storage (what a file manager shows as the phone's
     * internal storage), for saves that outlive an uninstall; null when the
     * storage is not mounted.
     */
    public String sharedStorageDir() {
        if (!Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState())) return null;
        return new File(Environment.getExternalStorageDirectory(), "GT3Revisited").getAbsolutePath();
    }

    /** Whether files can be written there: "All files access" on Android 11+, the storage permission before. */
    public boolean hasSharedStorageAccess() {
        if (Build.VERSION.SDK_INT >= 30) return Environment.isExternalStorageManager();
        return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    /** Android 11+: opens the system's "All files access" switch for this app (native code polls the result). */
    public void requestSharedStorageAccess() {
        if (Build.VERSION.SDK_INT < 30) return; // native code asks for the storage permission
        runOnUiThread(() -> {
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                         Uri.parse("package:" + getPackageName())));
            } catch (ActivityNotFoundException e) {
                try {
                    startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                } catch (ActivityNotFoundException e2) {
                    Log.w("GT3R", "No screen for All files access", e2);
                }
            }
        });
    }
}
