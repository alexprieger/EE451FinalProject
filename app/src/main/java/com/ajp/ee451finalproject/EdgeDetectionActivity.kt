package com.ajp.ee451finalproject

import android.content.res.AssetManager.ACCESS_BUFFER
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import com.ajp.ee451finalproject.databinding.ActivityEdgeDetectionBinding
import java.io.BufferedInputStream
import java.util.Vector
import kotlin.math.ceil
import kotlin.math.log2
import kotlin.math.pow

class EdgeDetectionActivity : AppCompatActivity() {

    class EdgeDetectionResult(val edges: Vector<Vector<Pixel>>, val timeMillis: Double)

    class Pixel(val row: Int, val col: Int)

    private lateinit var binding: ActivityEdgeDetectionBinding

    private var imageFilePath: String? = null

    private val useGarciaMolla = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        imageFilePath = intent.extras?.getString("image")

        binding = ActivityEdgeDetectionBinding.inflate(layoutInflater)
        setContentView(binding.root)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)

        if (hasFocus && imageFilePath != null) {
            val imageStream = BufferedInputStream(resources.assets.open("images/${imageFilePath}", ACCESS_BUFFER))
            val imageBitmap = BitmapFactory.decodeStream(imageStream)
            val imageWidth = imageBitmap.width + 2 // + 2 to add black frame
            val imageHeight = imageBitmap.height + 2

            val imageWidthPadded = 2.0.pow(ceil(log2(imageWidth.toDouble()))).toInt()
            val imageHeightPadded = 2.0.pow(ceil(log2(imageHeight.toDouble()))).toInt()

            // Image array is in row major order, with 0 representing black and 1 representing white
            val imageArray = ByteArray(imageWidthPadded * imageHeightPadded) { i ->
                val row = i / imageWidthPadded
                val col = i % imageWidthPadded
                if (row == 0 || row > imageBitmap.height || col == 0 || col > imageBitmap.width) {
                    // If on the frame or in the padding, make black
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
            val edgeResult = if (useGarciaMolla) {
                garciaMollaEdgeFindEfficient(imageArray, imageWidthPadded, imageHeightPadded)
            } else {
                suzukiEdgeFind(imageArray, imageWidthPadded, imageHeightPadded)
            }
            val edgeList = edgeResult.edges
            val edgeimageBitmap = imageBitmap.copy(Bitmap.Config.ARGB_8888, true)
            edgeimageBitmap.eraseColor(Color.BLACK)
            for (edge in edgeList) {
                for (pixel in edge) {
                    // - 1s to subtract off the frame
                    edgeimageBitmap.setPixel( pixel.col - 1,pixel.row - 1, Color.WHITE)
                }
            }
            val scaledEdgeImageBitmap = Bitmap.createScaledBitmap(edgeimageBitmap, binding.image.width, binding.image.height, false)
            binding.image.setImageBitmap(scaledEdgeImageBitmap)

            binding.edgeCount.text = "Edge count: ${edgeList.size}"
            binding.time.text = "Time: ${edgeResult.timeMillis}ms"
        }
    }

    external fun vulkanHello()

    external fun suzukiEdgeFind(image: ByteArray, width: Int, height: Int): EdgeDetectionResult

    external fun garciaMollaEdgeFind(image: ByteArray, width: Int, height: Int): EdgeDetectionResult

    external fun garciaMollaEdgeFindParallel(image: ByteArray, width: Int, height: Int, nThreads: Int): EdgeDetectionResult

    external fun garciaMollaEdgeFindEfficient(image: ByteArray, width: Int, height: Int): EdgeDetectionResult

    companion object {
        // Used to load the 'ee451finalproject' library on application startup.
        init {
            System.loadLibrary("ee451finalproject")
        }
    }
}