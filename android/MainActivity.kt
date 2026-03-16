package com.pianotrainer.esp32

import android.net.wifi.WifiManager
import android.os.Bundle
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.*
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.MultipartBody
import okhttp3.RequestBody.Companion.asRequestBody
import org.json.JSONObject
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var tvStatus: TextView
    private lateinit var tvFiles: TextView
    private lateinit var btnSelect: Button
    private lateinit var btnUpload: Button
    private lateinit var btnPlay: Button
    private lateinit var btnStop: Button
    private lateinit var sliderSpeed: SeekBar
    private lateinit var tvSpeed: TextView
    private lateinit var spinnerFiles: Spinner

    private val client = OkHttpClient()
    private var esp32Ip = "192.168.4.1" // Default AP IP
    private var midiFiles = mutableListOf<String>()
    private var selectedFileIndex = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        initViews()
        setupListeners()
        checkWifiConnection()
        loadFiles()
    }

    private fun initViews() {
        tvStatus = findViewById(R.id.tvStatus)
        tvFiles = findViewById(R.id.tvFiles)
        btnSelect = findViewById(R.id.btnSelect)
        btnUpload = findViewById(R.id.btnUpload)
        btnPlay = findViewById(R.id.btnPlay)
        btnStop = findViewById(R.id.btnStop)
        sliderSpeed = findViewById(R.id.sliderSpeed)
        tvSpeed = findViewById(R.id.tvSpeed)
        spinnerFiles = findViewById(R.id.spinnerFiles)

        sliderSpeed.max = 200
        sliderSpeed.progress = 100
        tvSpeed.text = "Speed: 1.0x"
    }

    private fun setupListeners() {
        btnUpload.setOnClickListener {
            pickMidiFile()
        }

        btnPlay.setOnClickListener {
            playSelectedFile()
        }

        btnStop.setOnClickListener {
            stopPlayback()
        }

        btnSelect.setOnClickListener {
            showFileSelectionDialog()
        }

        sliderSpeed.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                val speed = progress / 100.0f
                tvSpeed.text = "Speed: ${speed}x"
                setSpeed(speed)
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        spinnerFiles.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(
                parent: AdapterView<*>?,
                view: View?,
                position: Int,
                id: Long
            ) {
                selectedFileIndex = position
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }

    private fun checkWifiConnection() {
        val wifiManager = applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
        val wifiInfo = wifiManager.connectionInfo
        val ipAddress = wifiInfo.ipAddress

        if (ipAddress == 0) {
            tvStatus.text = "Подключитесь к WiFi сети ESP32: PianoTrainer"
        } else {
            val ip = String.format(
                "%d.%d.%d.%d",
                ipAddress and 0xFF,
                (ipAddress shr 8) and 0xFF,
                (ipAddress shr 16) and 0xFF,
                (ipAddress shr 24) and 0xFF
            )
            esp32Ip = ip
            tvStatus.text = "Подключено к ESP32: $ip"
        }
    }

    private fun pickMidiFile() {
        val intent = android.content.Intent(android.content.Intent.ACTION_GET_CONTENT)
        intent.type = "*/*"
        intent.addCategory(android.content.Intent.CATEGORY_OPENABLE)
        startActivityForResult(
            android.content.Intent.createChooser(intent, "Выберите MIDI файл"),
            REQUEST_PICK_FILE
        )
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: android.content.Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (requestCode == REQUEST_PICK_FILE && resultCode == RESULT_OK) {
            data?.data?.let { uri ->
                uploadMidiFile(uri)
            }
        }
    }

    private fun uploadMidiFile(uri: android.net.Uri) {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val file = createTempFile(uri)
                val requestBody = file.asRequestBody("audio/midi".toMediaTypeOrNull())

                val multipartBody = MultipartBody.Builder()
                    .setType(MultipartBody.FORM)
                    .addFormDataPart("midi", file.name, requestBody)
                    .build()

                val request = Request.Builder()
                    .url("http://$esp32Ip/upload")
                    .post(multipartBody)
                    .build()

                val response = client.newCall(request).execute()

                withContext(Dispatchers.Main) {
                    if (response.isSuccessful) {
                        Toast.makeText(this@MainActivity, "Файл загружен!", Toast.LENGTH_SHORT).show()
                        loadFiles()
                    } else {
                        Toast.makeText(this@MainActivity, "Ошибка загрузки", Toast.LENGTH_SHORT).show()
                    }
                }

                file.delete()
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    Toast.makeText(this@MainActivity, "Ошибка: ${e.message}", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun createTempFile(uri: android.net.Uri): File {
        val inputStream = contentResolver.openInputStream(uri)
        val tempFile = File.createTempFile("midi_", ".mid", cacheDir)
        val outputStream = tempFile.outputStream()

        inputStream?.use { input ->
            outputStream.use { output ->
                input.copyTo(output)
            }
        }

        return tempFile
    }

    private fun loadFiles() {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val request = Request.Builder()
                    .url("http://$esp32Ip/files")
                    .build()

                val response = client.newCall(request).execute()
                val json = JSONObject(response.body?.string() ?: "{}")
                val filesArray = json.getJSONArray("files")

                midiFiles.clear()
                for (i in 0 until filesArray.length()) {
                    val fileObj = filesArray.getJSONObject(i)
                    midiFiles.add(fileObj.getString("name"))
                }

                withContext(Dispatchers.Main) {
                    val adapter = ArrayAdapter(
                        this@MainActivity,
                        android.R.layout.simple_spinner_item,
                        midiFiles
                    )
                    adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
                    spinnerFiles.adapter = adapter

                    tvFiles.text = "Файлов: ${midiFiles.size}"
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    tvFiles.text = "Ошибка загрузки списка"
                }
            }
        }
    }

    private fun playSelectedFile() {
        if (midiFiles.isEmpty()) {
            Toast.makeText(this, "Нет файлов для воспроизведения", Toast.LENGTH_SHORT).show()
            return
        }

        CoroutineScope(Dispatchers.IO).launch {
            try {
                val request = Request.Builder()
                    .url("http://$esp32Ip/play?index=$selectedFileIndex")
                    .build()

                client.newCall(request).execute()

                withContext(Dispatchers.Main) {
                    tvStatus.text = "Воспроизведение: ${midiFiles[selectedFileIndex]}"
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    tvStatus.text = "Ошибка воспроизведения"
                }
            }
        }
    }

    private fun stopPlayback() {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val request = Request.Builder()
                    .url("http://$esp32Ip/stop")
                    .post(RequestBody.create(null, ByteArray(0)))
                    .build()

                client.newCall(request).execute()

                withContext(Dispatchers.Main) {
                    tvStatus.text = "Остановлено"
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) {
                    tvStatus.text = "Ошибка остановки"
                }
            }
        }
    }

    private fun setSpeed(speed: Float) {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                val request = Request.Builder()
                    .url("http://$esp32Ip/speed?value=$speed")
                    .build()

                client.newCall(request).execute()
            } catch (e: Exception) {
                // Игнорируем ошибки скорости
            }
        }
    }

    private fun showFileSelectionDialog() {
        if (midiFiles.isEmpty()) {
            Toast.makeText(this, "Нет доступных файлов", Toast.LENGTH_SHORT).show()
            return
        }

        val builder = android.app.AlertDialog.Builder(this)
        builder.setTitle("Выберите файл")
        builder.setItems(midiFiles.toTypedArray()) { dialog, which ->
            selectedFileIndex = which
            Toast.makeText(this, "Выбран: ${midiFiles[which]}", Toast.LENGTH_SHORT).show()
        }
        builder.show()
    }

    companion object {
        private const val REQUEST_PICK_FILE = 1
    }
}
