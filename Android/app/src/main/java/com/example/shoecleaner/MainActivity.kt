package com.example.shoecleaner

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.widget.Toast
import java.util.*
import com.example.shoecleaner.ui.theme.ShoeCleanerTheme
import android.content.pm.PackageManager
import androidx.activity.result.contract.ActivityResultContracts

class MainActivity : ComponentActivity() {
    private var bluetoothGatt: BluetoothGatt? = null
    private val UART_SERVICE_UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    private val CONTROL_CHARACTERISTIC_UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")

    private val BLUETOOTH_PERMISSIONS = arrayOf(
        android.Manifest.permission.BLUETOOTH_SCAN,
        android.Manifest.permission.BLUETOOTH_CONNECT
    )

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions.all { it.value }) {
            setupBluetoothConnection()
        } else {
            Toast.makeText(this, "블루투스 권한이 필요합니다", Toast.LENGTH_SHORT).show()
        }
    }

    private var isConnected by mutableStateOf(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            ShoeCleanerTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    ShoeCleanerControl(
                        isConnected = isConnected,
                        onButtonClick = { command -> sendBluetoothCommand(command) }
                    )
                }
            }
        }

        checkAndRequestBluetoothPermissions()
    }

    private fun checkAndRequestBluetoothPermissions() {
        if (BLUETOOTH_PERMISSIONS.all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }) {
            setupBluetoothConnection()
        } else {
            requestPermissionLauncher.launch(BLUETOOTH_PERMISSIONS)
        }
    }

    private fun setupBluetoothConnection() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val bluetoothAdapter = bluetoothManager.adapter

        // Bluetooth가 활성화되어 있는지 확인
        if (!bluetoothAdapter.isEnabled) {
            Toast.makeText(this, "블루투스가 비활성화되어 있습니다", Toast.LENGTH_SHORT).show()
            return
        }

        // 권한 확인 후 BLE 스캔 시작
        if (checkSelfPermission(android.Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
            startBluetoothScan(bluetoothAdapter)
        } else {
            Toast.makeText(this, "블루투스 권한이 필요합니다", Toast.LENGTH_SHORT).show()
        }
    }

    private fun startBluetoothScan(bluetoothAdapter: BluetoothAdapter) {
        try {
            if (checkSelfPermission(android.Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "블루투스 스캔 권한이 없습니다", Toast.LENGTH_SHORT).show()
                return
            }

            val bluetoothLeScanner = bluetoothAdapter.bluetoothLeScanner
            val scanFilter = ScanFilter.Builder()
                .setDeviceName("shoecleaner")
                .build()
            val scanSettings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build()

            val scanCallback = object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) {
                    if (checkSelfPermission(android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                        Toast.makeText(this@MainActivity, "블루투스 연결 권한이 없습니다", Toast.LENGTH_SHORT).show()
                        return
                    }
                    
                    val device = result.device
                    if (device.name == "shoecleaner") {
                        bluetoothLeScanner.stopScan(this)
                        connectToDevice(device)
                    }
                }
                override fun onScanFailed(errorCode: Int) {
                    Toast.makeText(this@MainActivity, "스캔 실패: $errorCode", Toast.LENGTH_SHORT).show()
                }
            }

            bluetoothLeScanner.startScan(listOf(scanFilter), scanSettings, scanCallback)
        } catch (e: SecurityException) {
            Toast.makeText(this, "블루투스 권한이 거부되었습니다", Toast.LENGTH_SHORT).show()
        }
    }

    private fun connectToDevice(device: BluetoothDevice) {
        try {
            if (checkSelfPermission(android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "블루투스 연결 권한이 없습니다", Toast.LENGTH_SHORT).show()
                return
            }

            bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {
                override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                    when (newState) {
                        BluetoothProfile.STATE_CONNECTED -> {
                            isConnected = true
                            gatt.discoverServices()
                        }
                        BluetoothProfile.STATE_DISCONNECTED -> {
                            isConnected = false
                        }
                    }
                }
                override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
                    if (status == BluetoothGatt.GATT_SUCCESS) {
                        val service = gatt.getService(UART_SERVICE_UUID)
                        val characteristic = service?.getCharacteristic(CONTROL_CHARACTERISTIC_UUID)
                        characteristic?.let { gatt.readCharacteristic(it) }
                    }
                }
            })
        } catch (e: SecurityException) {
            Toast.makeText(this, "블루투스 권한이 거부되었습니다", Toast.LENGTH_SHORT).show()
        }
    }

    private fun sendBluetoothCommand(command: String) {
        if (!isConnected) {
            Toast.makeText(this, "기기가 연결되어 있지 않습니다", Toast.LENGTH_SHORT).show()
            return
        }

        val service = bluetoothGatt?.getService(UART_SERVICE_UUID)
        val characteristic = service?.getCharacteristic(CONTROL_CHARACTERISTIC_UUID)

        val commandBytes = when (command) {
            "POWER_OFF" -> byteArrayOf(1, 0, 0, 0, 0)
            "NORMAL_MODE" -> byteArrayOf(0, 1, 0, 0, 0)
            "QUICK_MODE" -> byteArrayOf(0, 0, 1, 0, 0)
            "SWING_UP" -> byteArrayOf(0, 0, 0, 1, 0)
            "SWING_DOWN" -> byteArrayOf(0, 0, 0, 0, 1)
            else -> byteArrayOf(0, 0, 0, 0, 0)
        }

        characteristic?.value = commandBytes
        bluetoothGatt?.writeCharacteristic(characteristic)
    }
}

@Composable
fun ShoeCleanerControl(
    isConnected: Boolean,
    onButtonClick: (String) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceEvenly
    ) {
        Text(text = if (isConnected) "연결됨" else "연결 안됨", color = if (isConnected) Color.Green else Color.Red)
        Button(onClick = { onButtonClick("SWING_UP") }) { Text("스윙암 올리기") }
        Button(onClick = { onButtonClick("NORMAL_MODE") }) { Text("일반 모드") }
        Button(onClick = { onButtonClick("QUICK_MODE") }) { Text("쾌속 모드") }
        Button(onClick = { onButtonClick("SWING_DOWN") }) { Text("스윙암 내리기") }
        Button(onClick = { onButtonClick("POWER_OFF") }) { Text("전원 OFF") }
    }
}
