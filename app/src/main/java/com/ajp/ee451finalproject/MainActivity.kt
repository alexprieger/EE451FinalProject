package com.ajp.ee451finalproject

import android.content.res.AssetManager.ACCESS_BUFFER
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.Color.rgb
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import com.ajp.ee451finalproject.databinding.ActivityMainBinding
import java.io.BufferedInputStream
import java.io.IOException

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Example of a call to a native method
        binding.sampleText.text = stringFromJNI()

        val imageStream = BufferedInputStream(resources.assets.open("images/waffle.png", ACCESS_BUFFER))
        val imageBitmap = BitmapFactory.decodeStream(imageStream)
        // Image array is in row major order, with 0 representing black and 1 representing white
        val imageArray = ByteArray(imageBitmap.width * imageBitmap.height) { i ->
            val row = i / imageBitmap.width
            val col = i % imageBitmap.width
            // Doing a bit of a transformation here between (x,y) coordinates and standard row-major matrix index ordering
            if (imageBitmap.getPixel(col, row) == rgb(0, 0, 0)) 0 else 1
        }
        processImageBitmap(imageArray, imageBitmap.width, imageBitmap.height)
    }

    /**
     * A native method that is implemented by the 'ee451finalproject' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    external fun vulkanHello()

    external fun processImageBitmap(image: ByteArray, width: Int, height: Int)

    companion object {
        // Used to load the 'ee451finalproject' library on application startup.
        init {
            System.loadLibrary("ee451finalproject")
        }
    }
}