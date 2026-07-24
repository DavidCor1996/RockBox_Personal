/*
 * Minimal Android 2.0 HOME activity for the Rockpod iPod 6G bring-up image.
 * It intentionally needs no touch screen, storage, network, or media service.
 */
package org.rockpod.eclair.launcher;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class RockpodLauncherActivity extends Activity {
    private static final String TAG = "RockpodLauncher";
    private TextView mStatus;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(12, 10, 12, 10);
        root.setBackgroundColor(Color.BLACK);

        TextView title = new TextView(this);
        title.setText("Rockpod Android 2.0");
        title.setTextColor(Color.WHITE);
        title.setTextSize(20);
        title.setGravity(Gravity.CENTER);
        root.addView(title, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.FILL_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        mStatus = new TextView(this);
        mStatus.setText("RAM-only emulator profile\nNo storage mounted");
        mStatus.setTextColor(Color.LTGRAY);
        mStatus.setTextSize(14);
        mStatus.setGravity(Gravity.CENTER);
        root.addView(mStatus, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.FILL_PARENT, 0, 1.0f));

        Button status = new Button(this);
        status.setText("Android status");
        status.setFocusable(true);
        status.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mStatus.setText("Android framework is running\nStorage remains disabled");
                Log.i(TAG, "IPOD6G_ECLAIR_LAUNCHER:STATUS_SELECTED");
            }
        });
        root.addView(status, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.FILL_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        Button about = new Button(this);
        about.setText("About this build");
        about.setFocusable(true);
        about.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mStatus.setText("Android 2.0 / Eclair\nRockpod iPod 6G research port");
                Log.i(TAG, "IPOD6G_ECLAIR_LAUNCHER:ABOUT_SELECTED");
            }
        });
        root.addView(about, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.FILL_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        setContentView(root);
        status.requestFocus();
        Log.i(TAG, "IPOD6G_ECLAIR_LAUNCHER:ON_CREATE");
    }
}
