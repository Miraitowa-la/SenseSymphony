import { FrameBuffer } from '../utils/protocol.js'

const SPP_UUID = '00001101-0000-1000-8000-00805F9B34FB'
const TARGET_NAME = 'QianSai-MusicLab'

class SppService {
	constructor() {
		this.listeners = []
		this.status = { state: '未连接', message: '尚未连接设备', deviceName: '' }
		this.socket = null
		this.adapter = null
		this.lastDevice = null
		this.writeQueue = []
		this.writing = false
		this.readerTimer = null
	}

	subscribe(listener) {
		this.listeners.push(listener)
		listener({ type: 'status', ...this.status })
		return () => { this.listeners = this.listeners.filter((item) => item !== listener) }
	}

	emit(event) {
		this.listeners.slice().forEach((listener) => listener(event))
	}

	setStatus(state, message, deviceName = this.status.deviceName) {
		this.status = { state, message, deviceName }
		this.emit({ type: 'status', ...this.status })
	}

	async scan() {
		this.setStatus('扫描中', '正在查找已配对设备')
		try {
			await this.ensurePermissions()
			const adapter = this.getAdapter()
			if (!adapter.isEnabled()) throw new Error('请先打开手机蓝牙')
			adapter.cancelDiscovery()
			adapter.startDiscovery()
			const devices = this.bondedDevices(adapter)
			devices.sort((a, b) => Number(b.name === TARGET_NAME) - Number(a.name === TARGET_NAME))
			this.emit({ type: 'devices', devices })
			this.setStatus('未连接', devices.length ? '请选择已配对设备' : '未找到已配对设备')
			return devices
		} catch (error) {
			const message = ['请先打开手机蓝牙', '此手机不支持蓝牙', '蓝牙权限未授权', '仅支持 Android App'].includes(error.message)
				? error.message : '无法读取已配对设备，请检查蓝牙权限和系统配对状态'
			this.setStatus('未连接', message)
			throw new Error(message)
		}
	}

	connect(device) {
		if (!device || !device.address) return Promise.reject(new Error('设备信息无效'))
		this.lastDevice = device
		this.disconnect(false)
		this.setStatus('扫描中', '正在连接 ' + device.name, device.name)
		return new Promise((resolve, reject) => {
			try {
				const Thread = plus.android.importClass('java.lang.Thread')
				const runnable = plus.android.implements('java.lang.Runnable', { run: () => {
					try {
						const adapter = this.getAdapter()
						adapter.cancelDiscovery()
						const UUID = plus.android.importClass('java.util.UUID')
						const nativeDevice = adapter.getRemoteDevice(device.address)
						plus.android.importClass(nativeDevice)
						const uuid = UUID.fromString(SPP_UUID)
						let socket = nativeDevice.createRfcommSocketToServiceRecord(uuid)
						plus.android.importClass(socket)
						try {
							socket.connect()
						} catch (secureError) {
							try { socket.close() } catch (closeError) {}
							socket = nativeDevice.createInsecureRfcommSocketToServiceRecord(uuid)
							plus.android.importClass(socket)
							socket.connect()
						}
						this.socket = socket
						this.setStatus('已连接', '已连接 ' + device.name, device.name)
						this.startReader(socket)
						resolve()
					} catch (error) {
						const message = this.connectionError(error)
						this.setStatus('已断开', message, device.name)
						reject(new Error(message))
					}
				} })
				new Thread(runnable).start()
			} catch (error) { reject(error) }
		})
	}

	reconnect() {
		return this.lastDevice ? this.connect(this.lastDevice) : this.scan()
	}

	send(text) {
		if (!this.socket) return false
		this.writeQueue.push(String(text))
		if (!this.writing) this.flushWrites()
		return true
	}

	flushWrites() {
		if (!this.socket || !this.writeQueue.length) return
		this.writing = true
		const socket = this.socket
		const queue = this.writeQueue.splice(0)
		const Thread = plus.android.importClass('java.lang.Thread')
		const runnable = plus.android.implements('java.lang.Runnable', { run: () => {
			try {
				const output = socket.getOutputStream()
				plus.android.importClass(output)
				queue.forEach((text) => output.write(plus.android.invoke(text, 'getBytes', 'UTF-8')))
				output.flush()
			} catch (error) {
				this.handleDisconnect(socket, '发送失败，连接已断开')
			} finally {
				this.writing = false
				this.flushWrites()
			}
		} })
		new Thread(runnable).start()
	}

	disconnect(notify = true) {
		const socket = this.socket
		this.socket = null
		this.writeQueue = []
		this.stopReader()
		if (socket) {
			try { socket.close() } catch (error) {}
		}
		if (notify) this.setStatus('已断开', '连接已断开')
	}

	startReader(socket) {
		this.stopReader()
		const frames = new FrameBuffer()
		const input = socket.getInputStream()
		plus.android.importClass(input)
		this.readerTimer = setInterval(() => {
			if (this.socket !== socket) return this.stopReader()
			try {
				let chunk = ''
				// ponytail: nonblocking UI polling; use a native plugin reader for high-throughput traffic.
				for (let remaining = Math.min(input.available(), 512); remaining > 0; remaining--) chunk += String.fromCharCode(input.read())
				if (chunk) frames.push(chunk).forEach((frame) => this.emit({ type: 'frame', frame }))
			} catch (error) {
				this.handleDisconnect(socket, '连接已断开，请重新连接')
			}
		}, 30)
	}

	stopReader() {
		if (this.readerTimer) clearInterval(this.readerTimer)
		this.readerTimer = null
	}

	handleDisconnect(socket, message) {
		if (this.socket !== socket) return
		this.disconnect(false)
		this.setStatus('已断开', message)
	}

	connectionError(error) {
		const detail = String((error && (error.message || error)) || '').replace(/[\r\n]+/g, ' ').slice(0, 90)
		return detail ? `连接失败：${detail}` : '连接失败，请重试'
	}

	getAdapter() {
		if (this.adapter) return this.adapter
		if (typeof plus === 'undefined' || !plus.android) throw new Error('仅支持 Android App')
		const BluetoothAdapter = plus.android.importClass('android.bluetooth.BluetoothAdapter')
		this.adapter = BluetoothAdapter.getDefaultAdapter()
		if (!this.adapter) throw new Error('此手机不支持蓝牙')
		return this.adapter
	}

	bondedDevices(adapter) {
		const result = []
		const devices = adapter.getBondedDevices()
		plus.android.importClass(devices)
		const iterator = devices.iterator()
		plus.android.importClass(iterator)
		while (iterator.hasNext()) {
			const device = iterator.next()
			plus.android.importClass(device)
			result.push({ name: String(device.getName() || '未命名设备'), address: String(device.getAddress()) })
		}
		return result
	}

	ensurePermissions() {
		return new Promise((resolve, reject) => {
			try {
				const Build = plus.android.importClass('android.os.Build')
				const permissions = Build.VERSION.SDK_INT >= 31
					? ['android.permission.BLUETOOTH_SCAN', 'android.permission.BLUETOOTH_CONNECT']
					: ['android.permission.BLUETOOTH', 'android.permission.BLUETOOTH_ADMIN', 'android.permission.ACCESS_FINE_LOCATION']
				plus.android.requestPermissions(permissions, resolve, () => reject(new Error('蓝牙权限未授权')))
			} catch (error) { reject(error) }
		})
	}
}

export default new SppService()
