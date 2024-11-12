import bluetooth
import serial
import time

# 아두이노와 시리얼 통신 설정
arduino = serial.Serial('/dev/ttyUSB0', 9600)

# 블루투스 서버 설정
server_sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
port = 1
server_sock.bind(("", port))
server_sock.listen(1)

print("블루투스 연결 대기 중...")
client_sock, address = server_sock.accept()
print(f"연결됨: {address}")

while True:
    try:
        data = client_sock.recv(1024)
        if not data:
            break
        
        # 받은 명령을 아두이노로 전달
        arduino.write(data)
        time.sleep(0.1)
        
    except Exception as e:
        print(f"에러 발생: {e}")
        break

client_sock.close()
server_sock.close()