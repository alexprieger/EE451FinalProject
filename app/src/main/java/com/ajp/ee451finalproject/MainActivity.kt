package com.ajp.ee451finalproject

import android.content.res.AssetManager.ACCESS_BUFFER
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.Color.rgb
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.ImageView
import com.ajp.ee451finalproject.databinding.ActivityMainBinding
import java.io.BufferedInputStream
import java.util.Vector

class MainActivity : AppCompatActivity() {

    class Pixel(val row: Int, val col: Int)

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)

        if (hasFocus) {
            val imageStream = BufferedInputStream(resources.assets.open("images/mesh.png", ACCESS_BUFFER))
            val imageBitmap = BitmapFactory.decodeStream(imageStream)
            val imageWidth = imageBitmap.width + 2 // + 2 to add black frame
            val imageHeight = imageBitmap.height + 2
            // Image array is in row major order, with 0 representing black and 1 representing white
            val imageArray = ByteArray(imageWidth * imageHeight) { i ->
                val row = i / imageWidth
                val col = i % imageWidth
                if (row == 0 || row == imageHeight - 1 || col == 0 || col == imageWidth - 1) {
                    // If on the frame, make black

                    0
                } else {
                    // Binarize by converting to black and white and thresholding at half (127)
                    val pixel = imageBitmap.getPixel(col - 1, row - 1)
                    if (Color.red(pixel) + Color.blue(pixel) + Color.green(pixel) / 3 < 127) {
                        // Transforming here between (x,y) coordinates and standard row-major matrix index ordering
                        // Also subtracting ones to deal with the frame
                        0
                    } else {
                        1
                    }
                }
            }
            val edgeList = suzukiEdgeFind(imageArray, imageWidth, imageHeight)
            val edgesImageArray = IntArray(imageBitmap.width * imageBitmap.height) {
                Color.BLACK
            }
            for (edge in edgeList) {
                for (pixel in edge) {
                    // - 1s to subtract off the frame
                    edgesImageArray[(pixel.row - 1) * imageBitmap.width + (pixel.col - 1)] = Color.WHITE
                }
            }
            val edgeImageBitmap = Bitmap.createBitmap(edgesImageArray, imageBitmap.width, imageBitmap.height, Bitmap.Config.ARGB_8888)
            val scaledEdgeImageBitmap = Bitmap.createScaledBitmap(edgeImageBitmap, binding.image.width, binding.image.height, false)
            binding.image.setImageBitmap(scaledEdgeImageBitmap)

            binding.edgeCount.text = "Edge count: ${edgeList.size}"
        }
    }

    external fun vulkanHello()

    external fun suzukiEdgeFind(image: ByteArray, width: Int, height: Int): Vector<Vector<Pixel>>

    companion object {
        // Used to load the 'ee451finalproject' library on application startup.
        init {
            System.loadLibrary("ee451finalproject")
        }
    }
}