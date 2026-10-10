package com.example.vulkanwrapper;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity extends Activity {
    static { System.loadLibrary("vulkan_wrapper"); }
    private static native String runVulkanTest();
    private static native String testBC7();
    private static native String testBC7GPU();

    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(24, 24, 24, 24);
        TextView output = new TextView(this);
        output.setText("Pressione para testar Vulkan via wrapper.");
        Button button = new Button(this);
        button.setText("Testar Vulkan");
        button.setOnClickListener(v -> output.setText(runVulkanTest()));
        layout.addView(button);
        Button bc7Button = new Button(this);
        bc7Button.setText("Testar BC7 CPU");
        bc7Button.setOnClickListener(v -> output.setText(testBC7()));
        layout.addView(bc7Button);
        Button gpuButton = new Button(this);
        gpuButton.setText("Testar BC7 GPU");
        gpuButton.setOnClickListener(v -> output.setText(testBC7GPU()));
        layout.addView(gpuButton);
        layout.addView(output);
        setContentView(layout);
    }
}
