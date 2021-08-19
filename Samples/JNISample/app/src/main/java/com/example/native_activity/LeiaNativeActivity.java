package com.example.native_activity;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;

import com.leia.android.lights.LeiaDisplayManager;
import com.leia.android.lights.LeiaSDK;
import com.leia.android.lights.SimpleDisplayQuery;

import static com.leia.android.lights.LeiaDisplayManager.BacklightMode.MODE_2D;
import static com.leia.android.lights.LeiaDisplayManager.BacklightMode.MODE_3D;

/**
 * Extend the NativeActivity
 *
 * The real point of this is to allow us to access the JNI_onLoad
 * When games are made in C, we need a way to call Enable3D and Disable3D in "random" places
 * The activity life cycle isn't really capable of managing it on its own
 * Getting access through JNI allows us to change it at our discretion
 */
public class LeiaNativeActivity extends NativeActivity {
    // Makes sure we call JNI_onLoad
    static {
        System.loadLibrary("native-activity");
    }

    // Store the activity so the static methods can access it.
    private static LeiaNativeActivity sActivity = null;
    private static String TAG = "LeiaNativeActivity";
    private static LeiaDisplayManager mDisplayManager;
    private static SimpleDisplayQuery mSimpleDisplayQuery;

    @Override
    protected void onCreate(Bundle savedStateInstance) {
        super.onCreate(savedStateInstance);
        // Here we store the activity for static use
        sActivity = this;
        mDisplayManager = LeiaSDK.getDisplayManager(this);
        mSimpleDisplayQuery = new SimpleDisplayQuery(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mDisplayManager = LeiaSDK.getDisplayManager(this);
        Log.e("test", "sharpening: " + mSimpleDisplayQuery.GetViewSharpening(false));
        Enable3D();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Disable3D();
    }

    static public void Enable3D() {
        if (mDisplayManager != null) {
            mDisplayManager.setBacklightMode(MODE_3D);
        }
    }

    static public void Disable3D() {
        if (mDisplayManager != null) {
            mDisplayManager.setBacklightMode(MODE_2D);
        }
    }
}
