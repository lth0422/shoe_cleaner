package com.example.shoecleaner

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import java.util.*
import com.example.shoecleaner.ui.theme.ShoeCleanerTheme

class MainActivity : ComponentActivity() {
    private var bluetoothSocket: BluetoothSocket? = null
    private val BLUETOOTH_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            ShoeCleanerTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    ShoeCleanerControl(
                        onButtonClick = { command -> sendBluetoothCommand(command) }
                    )
                }
            }
        }
        
        // 블루투스 연결 설정
        setupBluetoothConnection()
    }

    private fun setupBluetoothConnection() {
        try {
            val bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
            val raspberryPi = bluetoothAdapter.bondedDevices.find { it.name == "RaspberryPi" }
            
            raspberryPi?.let {
                bluetoothSocket = it.createRfcommSocketToServiceRecord(BLUETOOTH_UUID)
                bluetoothSocket?.connect()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun sendBluetoothCommand(command: String) {
        try {
            bluetoothSocket?.outputStream?.write(command.toByteArray())
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}

@Composable
fun ShoeCleanerControl(onButtonClick: (String) -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceEvenly
    ) {
        Button(onClick = { onButtonClick("SWING_UP") }) {
            Text("스윙암 올리기")
        }
        Button(onClick = { onButtonClick("NORMAL_MODE") }) {
            Text("일반 모드")
        }
        Button(onClick = { onButtonClick("QUICK_MODE") }) {
            Text("쾌속 모드")
        }
        Button(onClick = { onButtonClick("SWING_DOWN") }) {
            Text("스윙암 내리기")
        }
        Button(onClick = { onButtonClick("POWER_OFF") }) {
            Text("전원 OFF")
        }
    }
}