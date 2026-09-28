import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';

import 'ble_service.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const LoraTestApp());
}

class LoraTestApp extends StatelessWidget {
  const LoraTestApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Teste LoRa BLE',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorSchemeSeed: Colors.blue,
        useMaterial3: true,
      ),
      home: const LoraTestScreen(),
    );
  }
}

class LoraTestScreen extends StatefulWidget {
  const LoraTestScreen({super.key});

  @override
  State<LoraTestScreen> createState() => _LoraTestScreenState();
}

class _LoraTestScreenState extends State<LoraTestScreen> {
  final BleService _bleService = BleService();

  final TextEditingController _destinationController =
      TextEditingController(text: '0002');

  final TextEditingController _messageController =
      TextEditingController();

  final List<DiscoveredDevice> _devices = [];
  final List<String> _events = [];

  StreamSubscription<DiscoveredDevice>? _deviceSubscription;
  StreamSubscription<DeviceConnectionState>?
      _connectionSubscription;
  StreamSubscription<String>? _messageSubscription;

  DeviceConnectionState _connectionState =
      DeviceConnectionState.disconnected;

  String? _connectedDeviceId;
  bool _isScanning = false;

  @override
  void initState() {
    super.initState();

    _deviceSubscription = _bleService.devices.listen(
      _addOrUpdateDevice,
    );

    _connectionSubscription =
        _bleService.connectionState.listen((state) {
      if (!mounted) {
        return;
      }

      setState(() {
        _connectionState = state;

        if (state == DeviceConnectionState.disconnected) {
          _connectedDeviceId = null;
        }
      });
    });

    _messageSubscription = _bleService.messages.listen(
      _addEvent,
    );
  }

  Future<void> _requestPermissions() async {
    await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();
  }

  Future<void> _startScan() async {
    await _requestPermissions();

    setState(() {
      _devices.clear();
      _isScanning = true;
    });

    _addEvent('Procurando nós LoRa...');
    _bleService.startScan();

    await Future<void>.delayed(
      const Duration(seconds: 8),
    );

    await _bleService.stopScan();

    if (!mounted) {
      return;
    }

    setState(() {
      _isScanning = false;
    });

    _addEvent('Busca finalizada.');
  }

  void _addOrUpdateDevice(DiscoveredDevice device) {
    if (!mounted) {
      return;
    }

    final index = _devices.indexWhere(
      (item) => item.id == device.id,
    );

    setState(() {
      if (index == -1) {
        _devices.add(device);
      } else {
        _devices[index] = device;
      }
    });
  }

  Future<void> _connect(DiscoveredDevice device) async {
    setState(() {
      _connectedDeviceId = device.id;
    });

    _addEvent('Conectando a ${device.name}...');
    await _bleService.connect(device.id);
  }

  Future<void> _disconnect() async {
    await _bleService.disconnect();
    _addEvent('Desconectado manualmente.');
  }

  Future<void> _sendMessage() async {
    final destination = _destinationController.text.trim();
    final message = _messageController.text.trim();

    if (_connectionState != DeviceConnectionState.connected) {
      _showMessage('Conecte um ESP32 primeiro.');
      return;
    }

    if (destination.isEmpty) {
      _showMessage('Informe o nó de destino.');
      return;
    }

    if (message.isEmpty) {
      _showMessage('Digite uma mensagem.');
      return;
    }

    try {
      await _bleService.sendMessage(
        destination: destination,
        message: message,
      );

      _messageController.clear();
    } catch (error) {
      _addEvent('Falha no envio: $error');
    }
  }

  void _addEvent(String event) {
    if (!mounted) {
      return;
    }

    final now = TimeOfDay.now();
    final hour = now.hour.toString().padLeft(2, '0');
    final minute = now.minute.toString().padLeft(2, '0');

    setState(() {
      _events.insert(0, '[$hour:$minute] $event');

      if (_events.length > 50) {
        _events.removeLast();
      }
    });
  }

  void _showMessage(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(message),
      ),
    );
  }

  String get _connectionText {
    switch (_connectionState) {
      case DeviceConnectionState.connecting:
        return 'Conectando';

      case DeviceConnectionState.connected:
        return 'Conectado';

      case DeviceConnectionState.disconnecting:
        return 'Desconectando';

      case DeviceConnectionState.disconnected:
        return 'Desconectado';
    }
  }

  Color _connectionColor(BuildContext context) {
    if (_connectionState == DeviceConnectionState.connected) {
      return Colors.green;
    }

    if (_connectionState == DeviceConnectionState.connecting) {
      return Colors.orange;
    }

    return Theme.of(context).colorScheme.error;
  }

  @override
  void dispose() {
    _deviceSubscription?.cancel();
    _connectionSubscription?.cancel();
    _messageSubscription?.cancel();

    _destinationController.dispose();
    _messageController.dispose();

    _bleService.dispose();

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final connected =
        _connectionState == DeviceConnectionState.connected;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Teste LoRa BLE'),
        actions: [
          if (connected)
            IconButton(
              onPressed: _disconnect,
              tooltip: 'Desconectar',
              icon: const Icon(Icons.bluetooth_disabled),
            ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: ListTile(
              leading: Icon(
                connected
                    ? Icons.bluetooth_connected
                    : Icons.bluetooth_disabled,
                color: _connectionColor(context),
              ),
              title: Text(_connectionText),
              subtitle: Text(
                _connectedDeviceId ?? 'Nenhum ESP32 selecionado',
              ),
            ),
          ),
          const SizedBox(height: 12),
          FilledButton.icon(
            onPressed: _isScanning ? null : _startScan,
            icon: _isScanning
                ? const SizedBox(
                    width: 18,
                    height: 18,
                    child: CircularProgressIndicator(
                      strokeWidth: 2,
                    ),
                  )
                : const Icon(Icons.bluetooth_searching),
            label: Text(
              _isScanning
                  ? 'Procurando...'
                  : 'Procurar ESP32',
            ),
          ),
          const SizedBox(height: 16),
          Text(
            'Dispositivos encontrados',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 8),
          if (_devices.isEmpty)
            const Text(
              'Nenhum dispositivo encontrado.',
            ),
          for (final device in _devices)
            Card(
              child: ListTile(
                leading: const Icon(Icons.developer_board),
                title: Text(
                  device.name.isEmpty
                      ? 'ESP32 sem nome'
                      : device.name,
                ),
                subtitle: Text(
                  'RSSI: ${device.rssi} dBm\n${device.id}',
                ),
                isThreeLine: true,
                trailing: FilledButton(
                  onPressed: () => _connect(device),
                  child: const Text('Conectar'),
                ),
              ),
            ),
          const Divider(height: 32),
          Text(
            'Enviar mensagem',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _destinationController,
            decoration: const InputDecoration(
              labelText: 'Nó de destino',
              hintText: 'Exemplo: 0002',
              border: OutlineInputBorder(),
            ),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _messageController,
            minLines: 2,
            maxLines: 4,
            maxLength: 120,
            decoration: const InputDecoration(
              labelText: 'Mensagem',
              hintText: 'Digite uma mensagem curta',
              border: OutlineInputBorder(),
            ),
          ),
          FilledButton.icon(
            onPressed: connected ? _sendMessage : null,
            icon: const Icon(Icons.send),
            label: const Text('Enviar ao ESP32'),
          ),
          const Divider(height: 32),
          Row(
            children: [
              Expanded(
                child: Text(
                  'Eventos recebidos',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ),
              TextButton(
                onPressed: () {
                  setState(() {
                    _events.clear();
                  });
                },
                child: const Text('Limpar'),
              ),
            ],
          ),
          if (_events.isEmpty)
            const Text('Nenhum evento recebido.'),
          for (final event in _events)
            Padding(
              padding: const EdgeInsets.only(bottom: 6),
              child: SelectableText(event),
            ),
        ],
      ),
    );
  }
}
