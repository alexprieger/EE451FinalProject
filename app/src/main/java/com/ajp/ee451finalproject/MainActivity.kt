package com.ajp.ee451finalproject

import android.content.Context
import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import android.view.View.OnClickListener
import android.widget.Button
import com.ajp.ee451finalproject.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity(), OnClickListener {

    private lateinit var binding: ActivityMainBinding

    companion object {
        private val sizeArray = intArrayOf(4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096)
        private val numThreadsArray = intArrayOf(0, 1, 2)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        binding.sizeButton.text = sizeArray[0].toString()
        binding.sizeButton.setOnClickListener { v ->
            val button = v as Button
            val currentSize = button.text.toString().toInt()
            val indexOfCurrentSize = sizeArray.indexOf(currentSize)
            val newSize = sizeArray[(indexOfCurrentSize + 1) % sizeArray.size]
            button.text = newSize.toString()
        }
        binding.numThreadsButton.text = numThreadsArray[0].toString()
        binding.numThreadsButton.setOnClickListener { v ->
            val button = v as Button
            val currentSize = button.text.toString().toInt()
            val indexOfCurrentSize = numThreadsArray.indexOf(currentSize)
            val newSize = numThreadsArray[(indexOfCurrentSize + 1) % numThreadsArray.size]
            button.text = newSize.toString()
        }
        binding.useGarciaMollaButton.text = false.toString()
        binding.useGarciaMollaButton.setOnClickListener { v ->
            val button = v as Button
            button.text = (!button.text.toString().toBoolean()).toString()
        }
    }

    override fun onCreateView(name: String, context: Context, attrs: AttributeSet): View? {
        return super.onCreateView(name, context, attrs)
    }

    override fun onClick(v: View?) {
        val button = v as Button
        val edgeIntent = Intent(this, EdgeDetectionActivity::class.java).apply {
            putExtra("image", button.text)
            putExtra("size", binding.sizeButton.text.toString().toInt())
            putExtra("useGarciaMolla", binding.useGarciaMollaButton.text.toString().toBoolean())
            putExtra("numThreads", binding.numThreadsButton.text.toString().toInt())
        }
        startActivity(edgeIntent)
    }

}