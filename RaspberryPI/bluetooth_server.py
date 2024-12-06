import sys
import dbus, dbus.mainloop.glib
import serial
import threading
import time
from gi.repository import GLib
from example_advertisement import Advertisement, register_ad_cb, register_ad_error_cb
from example_gatt_server import Service, Characteristic, register_app_cb, register_app_error_cb

BLUEZ_SERVICE_NAME = 'org.bluez'
LE_ADVERTISING_MANAGER_IFACE = 'org.bluez.LEAdvertisingManager1'
GATT_MANAGER_IFACE = 'org.bluez.GattManager1'
UART_SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
CONTROL_CHARACTERISTIC_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'
COMPLETE_CHARACTERISTIC_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'
LOCAL_NAME = 'shoecleaner'
mainloop = None
GATT_CHRC_IFACE = 'org.bluez.GattCharacteristic1'

class MainControlCharacteristic(Characteristic):
    def __init__(self, bus, index, service, arduino):
        Characteristic.__init__(self, bus, index, CONTROL_CHARACTERISTIC_UUID, ['write'], service)
        self.arduino = arduino
        print("특성 초기화 완료")

    def WriteValue(self, value, options):
        print("\n=== 새로운 블루투스 명령 수신 ===")
        signals = [bool(int(x)) for x in value[:5]]
        print(f'수신된 제어 신호: {signals}')
        
        # 명령어 해석
        if signals[0]: print("명령: 전원 OFF")
        elif signals[1]: print("명령: 일반 모드")
        elif signals[2]: print("명령: 쾌속 모드")
        elif signals[3]: print("명령: 스윙암 올리기")
        elif signals[4]: print("명령: 스윙암 내리기")

        command = f"{int(signals[0])},{int(signals[1])},{int(signals[2])},{int(signals[3])},{int(signals[4])}\n"
        self.arduino.write(command.encode())
        print(f'아두이노로 전송: {command}')
        print("=====================================")

class CompleteCharacteristic(Characteristic):
    def __init__(self, bus, index, service):
        Characteristic.__init__(self, bus, index, COMPLETE_CHARACTERISTIC_UUID,
                              ['notify'], service)
        self.notifying = True
        print("Complete Characteristic 초기화 완료 (알림 활성화)")

    def StartNotify(self):
        if self.notifying:
            print("알림이 이미 활성화되어 있습니다")
            return
        self.notifying = True
        print("알림이 활성화되었습니다")

    def StopNotify(self):
        if not self.notifying:
            print("알림이 이미 비활성화되어 있습니다")
            return
        self.notifying = False
        print("알림이 비활성화되었습니다")

    def SendComplete(self, status):
        if not self.notifying:
            print("알림이 비활성화 상태입니다")
            return
        value = []
        if status == "SWING_UP_COMPLETE":
            value.append(dbus.Byte(1))
            print("안드로이드로 전송: SWING_UP_COMPLETE (1)")
        elif status == "SWING_DOWN_COMPLETE":
            value.append(dbus.Byte(2))
            print("안드로이드로 전송: SWING_DOWN_COMPLETE (2)")
        elif status == "CLEANING_END":
            value.append(dbus.Byte(3))
            print("안드로이드로 전송: CLEANING_END (3)")
        self.PropertiesChanged(GATT_CHRC_IFACE, {'Value': value}, [])

class UartService(Service):
    def __init__(self, bus, index, arduino):
        Service.__init__(self, bus, index, UART_SERVICE_UUID, True)
        self.control_characteristic = MainControlCharacteristic(bus, 0, self, arduino)
        self.complete_characteristic = CompleteCharacteristic(bus, 1, self)
        self.add_characteristic(self.control_characteristic)
        self.add_characteristic(self.complete_characteristic)
        
        # 아두이노로부터 상태 읽기 스레드 시작
        self.arduino = arduino
        threading.Thread(target=self.read_arduino_status, daemon=True).start()

    def read_arduino_status(self):
        while True:
            if self.arduino.in_waiting:
                status = self.arduino.readline().decode().strip()
                print(f"아두이노로부터 수신: {status}")
                if status in ["SWING_UP_COMPLETE", "SWING_DOWN_COMPLETE", "CLEANING_END"]:
                    print(f"상태 변경 감지: {status}")
                    self.complete_characteristic.SendComplete(status)
            time.sleep(0.1)

class UartApplication(dbus.service.Object):
    def __init__(self, bus, arduino):
        self.path = '/'
        self.services = []
        dbus.service.Object.__init__(self, bus, self.path)
        self.uart_service = UartService(bus, 0, arduino)
        self.add_service(self.uart_service)
    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_service(self, service):
        self.services.append(service)

    @dbus.service.method(dbus_interface='org.freedesktop.DBus.ObjectManager', out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        response = {}
        for service in self.services:
            response[service.get_path()] = service.get_properties()
            for chrc in service.get_characteristics():
                response[chrc.get_path()] = chrc.get_properties()
        return response

class UartAdvertisement(Advertisement):
    def __init__(self, bus, index):
        Advertisement.__init__(self, bus, index, 'peripheral')
        self.add_service_uuid(UART_SERVICE_UUID)
        self.add_local_name(LOCAL_NAME)
        self.include_tx_power = True

class BluetoothServer:
    def __init__(self):
        global mainloop
        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        bus = dbus.SystemBus()
        self.arduino = serial.Serial('/dev/ttyACM0', 9600)  # 아두이노 시리얼 포트 연결
        adapter = self.find_adapter(bus)
        
        if not adapter:
            print('BLE adapter not found')
            return

        ad_manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter), LE_ADVERTISING_MANAGER_IFACE)
        service_manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter), GATT_MANAGER_IFACE)

        # BLE 애플리케이션 및 광고 등록
        app = UartApplication(bus, self.arduino)
        adv = UartAdvertisement(bus, 0)

        mainloop = GLib.MainLoop()

        ad_manager.RegisterAdvertisement(adv.get_path(), {}, reply_handler=register_ad_cb, error_handler=register_ad_error_cb)
        service_manager.RegisterApplication(app.get_path(), {}, reply_handler=register_app_cb, error_handler=register_app_error_cb)

        mainloop_thread = threading.Thread(target=mainloop.run)
        mainloop_thread.start()

        print("블루투스 서버 시작됨. 기기 이름: 'shoecleaner'")

    def find_adapter(self, bus):
        remote_om = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, '/'), 'org.freedesktop.DBus.ObjectManager')
        objects = remote_om.GetManagedObjects()
        for o, props in objects.items():
            if LE_ADVERTISING_MANAGER_IFACE in props and GATT_MANAGER_IFACE in props:
                return o
        return None

if __name__ == '__main__':
    BluetoothServer()
