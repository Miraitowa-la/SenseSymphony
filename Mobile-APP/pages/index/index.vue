<template>
	<view class="page">
		<view class="hero"><text class="eyebrow">QIANSAI · MUSIC LAB</text><text class="title">把每一次演奏，留成旋律。</text></view>
		<view class="status"><view class="dot" :class="status.state"></view><view><text class="status-name">{{ status.state }}</text><text class="status-message">{{ status.message }}</text></view><button class="link" @click="connectionAction">{{ status.state === '已断开' ? '重新连接' : '连接设备' }}</button></view>
		<view v-if="showDevices" class="devices"><text class="section-title">已配对设备</text><text v-if="!devices.length" class="empty">未找到设备。请先在系统蓝牙中配对 JDY-31-SPP。</text><view v-for="device in devices" :key="device.address" class="device" @click="connect(device)"><view><text>{{ device.name }}</text><text class="address">{{ device.address }}</text></view><text v-if="device.name === target" class="tag">推荐</text></view></view>
		<view class="modes"><view class="mode primary" @click="openHistory"><text class="mode-index">01</text><text class="mode-title">录制历史</text><text class="mode-copy">查看、回放你的演奏轨迹</text></view><view class="mode" @click="openScores"><text class="mode-index">02</text><text class="mode-title">歌曲与成绩</text><text class="mode-copy">查看挑战成绩历史</text></view></view>
	</view>
</template>

<script>
	import spp from '../../services/spp.js'
	export default {
		data() { return { status: spp.status, devices: [], showDevices: false, target: 'JDY-31-SPP', unsubscribe: null } },
		onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent) },
		onHide() { if (this.unsubscribe) this.unsubscribe() },
		methods: {
			onSppEvent(event) { if (event.type === 'status') this.status = event; if (event.type === 'devices') { this.devices = event.devices; this.showDevices = true } },
			async connectionAction() { if (this.status.state === '已断开' && spp.lastDevice) { try { await spp.reconnect() } catch (error) { uni.showToast({ title: '重连失败，请重试', icon: 'none' }) } return } this.scan() },
			async scan() { this.showDevices = true; try { await spp.scan() } catch (error) { uni.showToast({ title: error.message || '无法扫描设备', icon: 'none' }) } },
			async connect(device) { try { await spp.connect(device); this.showDevices = false } catch (error) { uni.showToast({ title: '连接失败，请重试', icon: 'none' }) } },
			openHistory() { uni.navigateTo({ url: '/pages/history/history' }) },
			openScores() { uni.navigateTo({ url: '/pages/mode2/index' }) }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 72rpx 40rpx; background: #161424; color: #f8f6ff; box-sizing: border-box; } .hero { margin: 36rpx 0 70rpx; } .eyebrow { color: #bfa7ff; font-size: 20rpx; letter-spacing: 4rpx; } .title { display: block; margin-top: 20rpx; font-size: 52rpx; font-weight: 700; line-height: 1.3; } .status { display: flex; align-items: center; gap: 18rpx; padding: 26rpx; background: #24213a; border-radius: 24rpx; } .dot { width: 16rpx; height: 16rpx; border-radius: 50%; background: #8f8a9e; } .dot.已连接 { background: #71e3a3; box-shadow: 0 0 14rpx #71e3a3; } .dot.扫描中 { background: #ffd36e; } .status-name { display: block; font-size: 28rpx; font-weight: 600; } .status-message, .address, .mode-copy, .empty { display: block; margin-top: 6rpx; color: #aaa5bd; font-size: 22rpx; } .link { margin-left: auto; padding: 0; color: #c8b5ff; font-size: 24rpx; background: transparent; border: 0; } .link::after { border: 0; } .devices { margin-top: 22rpx; padding: 24rpx; background: #201d34; border-radius: 22rpx; } .section-title { font-size: 24rpx; color: #bfa7ff; } .device { display: flex; justify-content: space-between; align-items: center; padding: 24rpx 0 4rpx; } .tag { padding: 5rpx 12rpx; border-radius: 20rpx; color: #161424; font-size: 20rpx; background: #bfa7ff; } .modes { margin-top: 62rpx; } .mode { margin-bottom: 22rpx; padding: 34rpx; border: 2rpx solid #35304d; border-radius: 28rpx; } .mode.primary { background: linear-gradient(135deg, #7654d7, #b36bef); border: 0; } .mode-index { color: #c7b8ea; font-size: 22rpx; } .mode-title { display: block; margin: 18rpx 0 8rpx; font-size: 36rpx; font-weight: 700; } .primary .mode-copy, .primary .mode-index { color: #f0eaff; }
</style>
