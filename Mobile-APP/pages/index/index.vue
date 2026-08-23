<template>
	<view class="page">
		<view class="hero">
			<view class="stars"></view><text class="note left">♪</text><text class="note right">♫</text>
			<text class="brand">芯露谷老农民</text><text class="eyebrow">QIANSAI MUSIC LAB</text><text class="tagline">无接触式音乐创作系统</text>
			<view class="wave"><view class="wave-line"></view><view class="wave-core"></view></view>
		</view>

		<view class="status">
			<view class="bluetooth">ᛒ</view><view class="status-text"><text class="device-name">{{ status.deviceName || 'JDY-31-SPP 设备' }}</text><text class="status-message" :class="status.state">{{ status.message }}</text></view>
			<button class="link" @click="connectionAction">{{ status.state === '已断开' ? '重连' : '连接设备' }}</button>
		</view>

		<view v-if="showDevices" class="devices"><text class="devices-title">已配对设备</text><text v-if="!devices.length" class="empty">未找到设备，请先在系统蓝牙中配对 JDY-31-SPP。</text><view v-for="device in devices" :key="device.address" class="device" @click="connect(device)"><view><text>{{ device.name }}</text><text class="address">{{ device.address }}</text></view><text v-if="device.name === target" class="tag">推荐</text></view></view>

		<view class="modes"><view class="mode history" @click="openHistory"><text class="mode-title">录制历史</text><text class="mode-copy">查看全部演奏记录</text><text class="mode-art">♫</text></view><view class="mode scores" @click="openScores"><text class="mode-title">歌曲与成绩</text><text class="mode-copy">挑战记录与排行榜</text><text class="mode-art">♜</text></view></view>
	</view>
</template>

<script>
	import spp from '../../services/spp.js'
	export default {
		data() { return { status: spp.status, devices: [], showDevices: false, target: 'QianSai-MusicLab', unsubscribe: null } },
		onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent) },
		onHide() { if (this.unsubscribe) this.unsubscribe() },
		methods: {
			onSppEvent(event) { if (event.type === 'status') this.status = event; if (event.type === 'devices') { this.devices = event.devices; this.showDevices = true } },
			async connectionAction() { if (this.status.state === '已断开' && spp.lastDevice) { try { await spp.reconnect() } catch (error) { uni.showToast({ title: '重连失败，请重试', icon: 'none' }) } return } this.scan() },
			async scan() { this.showDevices = true; try { await spp.scan() } catch (error) { uni.showToast({ title: error.message || '无法扫描设备', icon: 'none' }) } },
			async connect(device) { try { await spp.connect(device); this.showDevices = false } catch (error) { uni.showToast({ title: '连接失败，请重试', icon: 'none' }) } },
			openHistory() { uni.switchTab({ url: '/pages/history/history' }) },
			openScores() { uni.switchTab({ url: '/pages/scores/scores' }) }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 26rpx 34rpx 44rpx; overflow: hidden; color: #f8f6ff; background: #070917; box-sizing: border-box; }
	.hero { position: relative; height: 370rpx; margin: 6rpx -34rpx 28rpx; overflow: hidden; text-align: center; background: radial-gradient(ellipse at 50% 85%, #22205a 0%, #0b0b24 45%, #070917 78%); }
	.stars, .stars::after { position: absolute; inset: 0; content: ''; background-image: radial-gradient(#936cff 1rpx, transparent 2rpx), radial-gradient(#376bff 1rpx, transparent 2rpx); background-position: 10rpx 22rpx, 80rpx 68rpx; background-size: 86rpx 92rpx, 120rpx 108rpx; opacity: .5; }
	.brand, .eyebrow, .tagline, .note { position: relative; z-index: 1; display: block; } .brand { padding-top: 54rpx; font-size: 47rpx; font-weight: 700; letter-spacing: 2rpx; text-shadow: 0 0 16rpx #b962ff; } .eyebrow { margin-top: 10rpx; color: #b7a7ff; font-size: 24rpx; letter-spacing: 7rpx; } .tagline { margin-top: 15rpx; color: #e6dcff; font-size: 21rpx; }
	.note { position: absolute; z-index: 2; color: #c857ff; font-size: 42rpx; text-shadow: 0 0 14rpx #c857ff; } .note.left { left: 98rpx; top: 205rpx; transform: rotate(-20deg); } .note.right { right: 92rpx; top: 205rpx; transform: rotate(16deg); }
	.wave { position: absolute; right: -80rpx; bottom: 2rpx; left: -80rpx; height: 132rpx; border-radius: 50% 50% 0 0; background: repeating-linear-gradient(168deg, transparent 0 13rpx, #7235e066 14rpx 16rpx, transparent 17rpx 27rpx); transform: perspective(200rpx) rotateX(50deg); } .wave-line { position: absolute; top: 52rpx; right: 0; left: 0; height: 3rpx; background: linear-gradient(90deg, transparent, #fc58ed, #66a9ff, #fc58ed, transparent); box-shadow: 0 0 16rpx #8b5cff; } .wave-core { position: absolute; top: 30rpx; left: 50%; width: 16rpx; height: 48rpx; border-radius: 50%; background: #ddc6ff; box-shadow: 0 0 30rpx 12rpx #9456ff; }
	.status { position: relative; display: flex; align-items: center; min-height: 112rpx; padding: 22rpx 20rpx; border: 1rpx solid #23284e; border-radius: 16rpx; background: linear-gradient(135deg, #10162f, #11152b); box-sizing: border-box; box-shadow: inset 0 0 30rpx #5d45c411; }
	.bluetooth { display: flex; align-items: center; justify-content: center; width: 70rpx; height: 70rpx; margin-right: 18rpx; border: 2rpx solid #6889ff; border-radius: 12rpx; color: #71a5ff; font-size: 46rpx; } .status-text { min-width: 0; } .device-name, .status-message { display: block; } .device-name { max-width: 300rpx; overflow: hidden; font-size: 26rpx; font-weight: 700; text-overflow: ellipsis; white-space: nowrap; } .status-message { margin-top: 7rpx; color: #58d388; font-size: 22rpx; }
	.link { margin: 0 0 0 auto; padding: 0 28rpx; border: 0; border-radius: 14rpx; color: #fff; font-size: 22rpx; line-height: 58rpx; background: linear-gradient(135deg, #343961, #262943); } .link::after { border: 0; }
	.devices { margin-top: 14rpx; padding: 20rpx; border: 1rpx solid #2d2850; border-radius: 16rpx; background: #101227; } .devices-title { color: #c7b4fb; font-size: 22rpx; } .empty, .address { display: block; margin-top: 8rpx; color: #aaa5bd; font-size: 20rpx; } .device { display: flex; align-items: center; justify-content: space-between; padding-top: 18rpx; } .tag { padding: 5rpx 12rpx; border-radius: 20rpx; color: #171426; font-size: 18rpx; background: #bfa7ff; }
	.modes { display: flex; gap: 18rpx; margin-top: 26rpx; } .mode { position: relative; flex: 1; height: 186rpx; overflow: hidden; padding: 26rpx 22rpx; border: 1rpx solid #7842c2; border-radius: 17rpx; box-sizing: border-box; } .history { background: radial-gradient(circle at 75% 95%, #ba58de55, transparent 43%), linear-gradient(135deg, #3c114e, #291642); } .scores { border-color: #4e58d5; background: radial-gradient(circle at 74% 95%, #506aff55, transparent 43%), linear-gradient(135deg, #141642, #1e2070); } .mode-title, .mode-copy { position: relative; z-index: 1; display: block; } .mode-title { font-size: 29rpx; font-weight: 700; } .mode-copy { margin-top: 12rpx; color: #d0c6eb; font-size: 19rpx; } .mode-art { position: absolute; right: 18rpx; bottom: -14rpx; color: #dc96ff; font-size: 100rpx; opacity: .72; transform: rotate(-17deg); } .scores .mode-art { color: #a5b4ff; }
</style>
