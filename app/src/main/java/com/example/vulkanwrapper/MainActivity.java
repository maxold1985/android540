package com.example.vulkanwrapper;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;

public class MainActivity extends Activity
{
	private static final int OPEN_DDS = 71;
	private TextView output;
	private ImageView preview;

	static
	{
		System.loadLibrary("vulkan_wrapper");
	}

	private static native String runVulkanTest();
	private static native String testBC7();
	private static native String testBC7GPU();
	private static native int[] decodeDdsBC7(byte[] file, int[] dimensions);

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		ScrollView scroll = new ScrollView(this);
		LinearLayout layout = new LinearLayout(this);
		layout.setOrientation(LinearLayout.VERTICAL);
		layout.setPadding(24, 24, 24, 24);
		output = new TextView(this);
		output.setText("Escolha um arquivo DDS BC7 para visualizar.");
		Button vulkanButton = new Button(this);
		vulkanButton.setText("Testar Vulkan");
		vulkanButton.setOnClickListener(v -> output.setText(runVulkanTest()));
		layout.addView(vulkanButton);
		Button cpuButton = new Button(this);
		cpuButton.setText("Testar BC7 CPU");
		cpuButton.setOnClickListener(v -> output.setText(testBC7()));
		layout.addView(cpuButton);
		Button gpuButton = new Button(this);
		gpuButton.setText("Testar BC7 GPU");
		gpuButton.setOnClickListener(v -> output.setText(testBC7GPU()));
		layout.addView(gpuButton);
		Button openButton = new Button(this);
		openButton.setText("Abrir textura BC7 (.dds)");
		openButton.setOnClickListener(v -> openDds());
		layout.addView(openButton);
		layout.addView(output);
		preview = new ImageView(this);
		preview.setAdjustViewBounds(true);
		preview.setScaleType(ImageView.ScaleType.FIT_CENTER);
		layout.addView(preview, new LinearLayout.LayoutParams(
			ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		scroll.addView(layout);
		setContentView(scroll);
	}

	private void openDds()
	{
		Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
		intent.setType("*/*");
		intent.addCategory(Intent.CATEGORY_OPENABLE);
		startActivityForResult(intent, OPEN_DDS);
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		super.onActivityResult(requestCode, resultCode, data);
		if (requestCode != OPEN_DDS || resultCode != RESULT_OK || data == null)
		{
			return;
		}
		Uri uri = data.getData();
		if (uri == null)
		{
			return;
		}
		try (InputStream in = getContentResolver().openInputStream(uri);
			ByteArrayOutputStream bytes = new ByteArrayOutputStream())
		{
			if (in == null)
			{
				throw new IllegalArgumentException("Arquivo indisponivel");
			}
			byte[] chunk = new byte[65536];
			int n;
			while ((n = in.read(chunk)) != -1)
			{
				if (bytes.size() + n > 64 * 1024 * 1024)
				{
					throw new IllegalArgumentException("DDS excede 64 MB");
				}
				bytes.write(chunk, 0, n);
			}
			int[] dims = new int[2];
			int[] pixels = decodeDdsBC7(bytes.toByteArray(), dims);
			if (pixels == null)
			{
				preview.setImageDrawable(null);
				output.setText("DDS nao suportado: requer DX10 BC7 2D, arraySize=1.");
				return;
			}
			Bitmap bitmap = Bitmap.createBitmap(pixels, dims[0], dims[1], Bitmap.Config.ARGB_8888);
			preview.setImageBitmap(bitmap);
			output.setText("BC7 convertido e exibido: " + dims[0] + " x " + dims[1] +
				"\\nVisualizacao via Android ImageView (nao Vulkan).");
		}
		catch (Exception e)
		{
			output.setText("Falha ao abrir DDS: " + e.getMessage());
		}
	}
}
