package com.ajp.ee451finalproject

import android.content.res.AssetManager.ACCESS_BUFFER
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import com.ajp.ee451finalproject.databinding.ActivityEdgeDetectionBinding
import java.io.BufferedInputStream
import java.util.Vector
import kotlin.math.ceil
import kotlin.math.log2
import kotlin.math.pow

class EdgeDetectionActivity : AppCompatActivity() {

    class EdgeDetectionResult(val edges: Vector<Vector<Pixel>>,
                              val timeSetupMillis: Double,
                              val timeSuzukiMillis: Double,
                              val timeJoinMillis: Double,
                              val timeMillis: Double)

    class Pixel(val row: Int, val col: Int)

    private lateinit var binding: ActivityEdgeDetectionBinding

    private var imageFilePath: String? = null
    private var imageSize: Int? = null

    private var useGarciaMolla = false
    private var numThreads: Int? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        imageFilePath = intent.extras?.getString("image")
        imageSize = intent.extras?.getInt("size")
        useGarciaMolla = intent.extras?.getBoolean("useGarciaMolla") ?: false
        numThreads = intent.extras?.getInt("numThreads")

        binding = ActivityEdgeDetectionBinding.inflate(layoutInflater)
        setContentView(binding.root)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)

        if (hasFocus && imageFilePath != null) {
            val imageStream = BufferedInputStream(resources.assets.open("images/${imageFilePath}/${imageSize}.png", ACCESS_BUFFER))
            val imageBitmap = BitmapFactory.decodeStream(imageStream)

            // Image array is in row major order, with 0 representing black and 1 representing white
            val imageArray = ByteArray(imageBitmap.width * imageBitmap.height) { i ->
                val row = i / imageBitmap.width
                val col = i % imageBitmap.width
                if (row == 0 || row == imageBitmap.height - 1 || col == 0 || col == imageBitmap.width - 1) {
                    // If on the frame or in the padding, make black
                    0
                } else {
                    // Binarize by converting to black and white and thresholding at half (127)
                    val pixel = imageBitmap.getPixel(col - 1, row - 1)
                    if (Color.red(pixel) + Color.blue(pixel) + Color.green(pixel) / 3 < 128) {
                        // Transforming here between (x,y) coordinates and standard row-major matrix index ordering
                        // Also subtracting ones to deal with the frame
                        0
                    } else {
                        1
                    }
                }
            }
            val edgeResult = if (useGarciaMolla) {
                garciaMollaEdgeFindParallel(imageArray, imageBitmap.width, imageBitmap.height, numThreads ?: 0)
            } else {
                suzukiEdgeFind(imageArray, imageBitmap.width, imageBitmap.height)
            }
            val edgeList = edgeResult.edges
            val edgeimageBitmap = imageBitmap.copy(Bitmap.Config.ARGB_8888, true)
            edgeimageBitmap.eraseColor(Color.BLACK)
            for (edge in edgeList) {
                for (pixel in edge) {
                    // - 1s to subtract off the frame
                    edgeimageBitmap.setPixel( pixel.col,pixel.row, Color.WHITE)
                }
            }
            val scaledEdgeImageBitmap = Bitmap.createScaledBitmap(edgeimageBitmap, binding.image.width, binding.image.height, false)
            binding.image.setImageBitmap(scaledEdgeImageBitmap)

            binding.edgeCount.text = "Edge count: ${edgeList.size}"
            binding.time.text = "Total time: ${edgeResult.timeMillis}ms\n" +
                    "Setup time: ${edgeResult.timeSetupMillis}ms\n" +
                    "Suzuki time: ${edgeResult.timeSuzukiMillis}ms\n" +
                    "Join time: ${edgeResult.timeJoinMillis}ms"
        }
    }

    external fun vulkanHello()

    external fun suzukiEdgeFind(image: ByteArray, width: Int, height: Int): EdgeDetectionResult

    external fun garciaMollaEdgeFind(image: ByteArray, width: Int, height: Int): EdgeDetectionResult

    external fun garciaMollaEdgeFindParallel(image: ByteArray, width: Int, height: Int, lgSqrtNumThreads: Int): EdgeDetectionResult

    companion object {
        // Used to load the 'ee451finalproject' library on application startup.
        init {
            System.loadLibrary("ee451finalproject")
        }
    }
}