/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.sample.moreteapots;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.NativeActivity;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.WindowManager.LayoutParams;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.TextView;

import com.leia.android.lights.LeiaDisplayManager;
import com.leia.android.lights.LeiaDisplayManager.BacklightMode;
import com.leia.android.lights.LeiaSDK;
import com.leia.android.lights.SimpleDisplayQuery;
import com.leia.android.lights.BacklightModeListener;

import static com.leia.android.lights.LeiaDisplayManager.BacklightMode.MODE_2D;
import static com.leia.android.lights.LeiaDisplayManager.BacklightMode.MODE_3D;

public class MoreTeapotsNativeActivity extends NativeActivity
                                       implements BacklightModeListener {
    static {
        System.loadLibrary("MoreTeapotsNativeActivity");
    }

    private BacklightMode mExpectedBacklightMode;
    private boolean mBacklightHasShutDown;
    private boolean mIsDeviceCurrentlyInPortraitMode;
    private LeiaDisplayManager mDisplayManager;
    private MoreTeapotsNativeActivity _activity;
    private SimpleDisplayQuery mLeiaQuery;

    @Override
    public void onBacklightModeChanged(BacklightMode backlightMode)
    {
        Log.e("MoreTeapotsActivity", "onBacklightModeChanged: callback received");
        // Do something to remember the backlight is no longer on
        // Later, we have to let the native side know this occurred.
        if (mExpectedBacklightMode == MODE_3D &&
            mExpectedBacklightMode != backlightMode) {
            Log.e("MoreTeapotsActivity", "onBacklightModeChanged: mBacklightHasShutDown = true;");
            mBacklightHasShutDown = true;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //Hide toolbar
        int SDK_INT = android.os.Build.VERSION.SDK_INT;
        Log.i("OnCreate", "OnCreate!!!");
        if (SDK_INT >= 19) {
            setImmersiveSticky();
            View decorView = getWindow().getDecorView();
            decorView.setOnSystemUiVisibilityChangeListener
                    (new View.OnSystemUiVisibilityChangeListener() {
                        @Override
                        public void onSystemUiVisibilityChange(int visibility) {
                            setImmersiveSticky();
                        }
                    });
        }
        _activity = this;
        mIsDeviceCurrentlyInPortraitMode = IsPortraitCurrentOrientation();
        mLeiaQuery = new SimpleDisplayQuery(this);
        mDisplayManager = LeiaSDK.getDisplayManager(this);
        if (mDisplayManager != null) {
            Log.e("MoreTeapotsActivity", "onCreate: registered = true;");
            mDisplayManager.registerBacklightModeListener(this);
        }
    }

    @TargetApi(19)
    protected void onResume() {
        super.onResume();
        _activity = this;
        //Hide toolbar
        int SDK_INT = android.os.Build.VERSION.SDK_INT;
        if (SDK_INT >= 11 && SDK_INT < 14) {
            getWindow().getDecorView().setSystemUiVisibility(View.STATUS_BAR_HIDDEN);
        } else if (SDK_INT >= 14 && SDK_INT < 19) {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LOW_PROFILE);
        } else if (SDK_INT >= 19) {
            setImmersiveSticky();
        }

        mIsDeviceCurrentlyInPortraitMode = IsPortraitCurrentOrientation();
        Enable3D();
    }

    protected void onPause() {
        super.onPause();

        Disable3D();
    }
    // Our popup window, you will call it from your C/C++ code later

    @TargetApi(19)
    void setImmersiveSticky() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    PopupWindow _popupWindow;
    TextView _label;

    @SuppressLint("InflateParams")
    public void showUI() {
        if (_popupWindow != null)
            return;

        _activity = this;

        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                LayoutInflater layoutInflater
                        = (LayoutInflater) getBaseContext()
                        .getSystemService(LAYOUT_INFLATER_SERVICE);
                View popupView = layoutInflater.inflate(R.layout.widgets, null);
                _popupWindow = new PopupWindow(
                        popupView,
                        LayoutParams.WRAP_CONTENT,
                        LayoutParams.WRAP_CONTENT);

                LinearLayout mainLayout = new LinearLayout(_activity);
                MarginLayoutParams params = new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                params.setMargins(0, 0, 0, 0);
                _activity.setContentView(mainLayout, params);

                // Show our UI over NativeActivity window
                _popupWindow.showAtLocation(mainLayout, Gravity.TOP | Gravity.START, 10, 10);
                _popupWindow.update();

                _label = (TextView) popupView.findViewById(R.id.textViewFPS);

            }
        });
    }

    public void updateFPS(final String str, final float fFPS) {
        if (_label == null)
            return;

        _activity = this;
        this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                _label.setText(String.format("%s: %2.2f FPS", str, fFPS));
            }
        });
    }

    public boolean IsPortraitCurrentOrientation() {
        boolean is_portrait_current_orientation = false;

        DisplayMetrics dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(dm);
        int width = dm.widthPixels;
        int height = dm.heightPixels;

        if (height > width) {
            is_portrait_current_orientation = true;
        }
        else {
            is_portrait_current_orientation = false;
        }
        return is_portrait_current_orientation;
    }

    public float[] GetViewSharpening() {
        return mLeiaQuery.GetViewSharpening(mIsDeviceCurrentlyInPortraitMode);
    }

    public float GetAlignmentOffset() {
        return mLeiaQuery.GetAlignmentOffset(mIsDeviceCurrentlyInPortraitMode);
    }

    public int GetSystemDisparity() {
        return mLeiaQuery.GetSystemDisparity();
    }

    public int[] GetScreenResolution() {
        return mLeiaQuery.GetScreenResolution(mIsDeviceCurrentlyInPortraitMode);
    }

    public int[] GetNumAvailableViews() {
        return mLeiaQuery.GetNumAvailableViews(mIsDeviceCurrentlyInPortraitMode);
    }

    public int[] GetViewResolution() {
        return mLeiaQuery.GetViewResolution(mIsDeviceCurrentlyInPortraitMode);
    }

    public void Enable3D() {
        mExpectedBacklightMode = MODE_3D;
        SetBacklightMode(mExpectedBacklightMode);
    }

    public void Disable3D() {
        mExpectedBacklightMode = MODE_2D;
        SetBacklightMode(mExpectedBacklightMode);
    }

    private void SetBacklightMode(BacklightMode mode) {
        if (mDisplayManager != null) {
            mDisplayManager.setBacklightMode(mode);
        }
    }

    public int HasBacklightShutdown() {
        Log.e("MoreTeapotsActivity", "HasBacklightShutdown: mBacklightHasShutDown = " + mBacklightHasShutDown);
        return mBacklightHasShutDown ? 1 : 0;
    }
}


