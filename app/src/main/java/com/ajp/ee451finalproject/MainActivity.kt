package com.ajp.ee451finalproject

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.View
import android.view.View.OnClickListener
import android.widget.Button

class MainActivity : AppCompatActivity(), OnClickListener {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
    }

    override fun onClick(v: View?) {
        val button = v as Button
        val edgeIntent = Intent(this, EdgeDetectionActivity::class.java).apply {
            putExtra("image", button.text)
        }
        startActivity(edgeIntent)
    }

}