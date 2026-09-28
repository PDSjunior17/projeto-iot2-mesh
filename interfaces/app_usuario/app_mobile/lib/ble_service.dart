import 'dart:async';
import 'dart:convert';

import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

class BleService {
  final FlutterReactiveBle ble = FlutterReactiveBle();

  static final Uuid serviceUuid = Uuid.parse(
    '7a1e0001-6b3c-4e91-a5c7-7d523d849001',
  );

  static final Uuid rxUuid = Uuid.parse(
    '7a1e0002-6b3c-4e91-a5c7-7d523d849001',
  );

  static final Uuid txUuid = Uuid.parse(
    '7a1e0003-6b3c-4e91-a5c7-7d523d849001',
  );

  StreamSubscription<DiscoveredDevice>? _scanSubscription;
  StreamSubscription<ConnectionStateUpdate>? _connectionSubscription;
  StreamSubscription<List<int>>? _notificationSubscription;

  QualifiedCharacteristic? _rxCharacteristic;
  QualifiedCharacteristic? _txCharacteristic;

  final StreamController<DiscoveredDevice> _devicesController =
      StreamController<DiscoveredDevice>.broadcast();

  final StreamController<DeviceConnectionState> _connectionController =
      StreamController<DeviceConnectionState>.broadcast();

  final StreamController<String> _messageController =
      StreamController<String>.broadcast();

  Stream<DiscoveredDevice> get devices => _devicesController.stream;

  Stream<DeviceConnectionState> get connectionState =>
      _connectionController.stream;

  Stream<String> get messages => _messageController.stream;

  void startScan() {
    stopScan();

    _scanSubscription = ble
        .scanForDevices(
          withServices: const [],
          scanMode: ScanMode.lowLatency,
        )
        .listen(
      (device) {
        if (device.name.toUpperCase().startsWith('LORA-NODE')) {
          _devicesController.add(device);
        }
      },
      onError: (Object error) {
        _messageController.add('Erro na busca BLE: $error');
      },
    );
  }

  Future<void> stopScan() async {
    await _scanSubscription?.cancel();
    _scanSubscription = null;
  }

  Future<void> connect(String deviceId) async {
    await stopScan();
    await disconnect();

    _connectionSubscription = ble
        .connectToDevice(
          id: deviceId,
          connectionTimeout: const Duration(seconds: 10),
        )
        .listen(
      (update) {
        _connectionController.add(update.connectionState);

        if (update.connectionState ==
            DeviceConnectionState.connected) {
          _configureCharacteristics(deviceId);
          _listenForNotifications();
          _messageController.add('ESP32 conectado.');
        }

        if (update.connectionState ==
            DeviceConnectionState.disconnected) {
          _messageController.add('ESP32 desconectado.');
        }
      },
      onError: (Object error) {
        _messageController.add('Erro na conexão: $error');
      },
    );
  }

  void _configureCharacteristics(String deviceId) {
    _rxCharacteristic = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: rxUuid,
      deviceId: deviceId,
    );

    _txCharacteristic = QualifiedCharacteristic(
      serviceId: serviceUuid,
      characteristicId: txUuid,
      deviceId: deviceId,
    );
  }

  void _listenForNotifications() {
    final tx = _txCharacteristic;

    if (tx == null) {
      return;
    }

    _notificationSubscription?.cancel();

    _notificationSubscription =
        ble.subscribeToCharacteristic(tx).listen(
      (data) {
        final text = utf8.decode(
          data,
          allowMalformed: true,
        );

        _messageController.add(text);
      },
      onError: (Object error) {
        _messageController.add(
          'Erro ao receber dados: $error',
        );
      },
    );
  }

  Future<void> sendMessage({
    required String destination,
    required String message,
  }) async {
    final rx = _rxCharacteristic;

    if (rx == null) {
      throw Exception('Nenhum ESP32 conectado.');
    }

    /*
     * Formato temporário do teste:
     *
     * MSG|DESTINO|TEXTO
     *
     * Exemplo:
     * MSG|0002|Ola, tudo bem?
     */
    final command = 'MSG|$destination|$message';
    final data = utf8.encode(command);

    await ble.writeCharacteristicWithResponse(
      rx,
      value: data,
    );

    _messageController.add(
      'Enviado para $destination: $message',
    );
  }

  Future<void> disconnect() async {
    await _notificationSubscription?.cancel();
    _notificationSubscription = null;

    await _connectionSubscription?.cancel();
    _connectionSubscription = null;

    _rxCharacteristic = null;
    _txCharacteristic = null;

    _connectionController.add(
      DeviceConnectionState.disconnected,
    );
  }

  Future<void> dispose() async {
    await stopScan();
    await disconnect();

    await _devicesController.close();
    await _connectionController.close();
    await _messageController.close();
  }
}
