<template>
	<view class="page"><view class="top"><view><text class="title">第 {{ sequence }} 段演奏</text><text class="copy">槽位 {{ slot }} · {{ events.length }} / {{ expected }} 个事件</text></view><button class="play" @click="togglePlay">{{ playing ? '停止' : '播放' }}</button></view><view class="trace"><text class="trace-title">XY 轨迹</text><canvas canvas-id="trace" class="canvas" width="650" height="270"></canvas></view><view class="events"><text class="trace-title">事件列表</text><view v-for="(event, index) in events" :key="index" class="event"><text>{{ event.timeMs }} ms</text><text>{{ bands[event.band] }} · {{ notes[event.note] }}</text><text>({{ event.x }}, {{ event.y }})</text></view><view v-if="!events.length" class="waiting">{{ hint }}</view></view></view>
</template>

<script>
	import spp from '../../services/spp.js'
	import * as tone from '../../utils/tone.js'
	export default {
		data() { return { slot: 0, sequence: 0, expected: 0, events: [], hint: '正在读取详情…', playing: false, unsubscribe: null, bands: ['低音', '中音', '高音'], notes: ['Do', 'Re', 'Mi', 'Fa', 'Sol', 'La', 'Si'] } },
		onLoad(query) { this.slot = Number(query.slot); this.sequence = Number(query.sequence); this.expected = Number(query.eventCount) },
		onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent); this.load() },
		onHide() { this.stopPlayback() },
		onUnload() { this.stopPlayback(); if (this.unsubscribe) this.unsubscribe() },
		methods: {
			load() { if (!spp.socket) { this.hint = '连接已断开，请重新连接'; return } this.events = []; spp.send(`M1,GET,${this.slot}\n`) },
			onSppEvent(event) { if (event.type !== 'frame') return; const frame = event.frame; if (frame.type === 'record') { this.sequence = frame.sequence; this.expected = frame.eventCount } if (frame.type === 'event') this.events.push(frame); if (frame.type === 'end') { this.hint = this.events.length ? '' : '没有事件数据'; this.$nextTick(this.drawTrace) } },
			drawTrace() { const context = uni.createCanvasContext('trace', this); const width = 650; const height = 270; context.setFillStyle('#171426'); context.fillRect(0, 0, width, height); if (!this.events.length) { context.draw(); return } const xs = this.events.map((item) => item.x); const ys = this.events.map((item) => item.y); const minX = Math.min(...xs), maxX = Math.max(...xs), minY = Math.min(...ys), maxY = Math.max(...ys); const point = (event) => ({ x: 30 + (event.x - minX) * 590 / (maxX - minX || 1), y: 240 - (event.y - minY) * 210 / (maxY - minY || 1) }); context.setStrokeStyle('#bb91ff'); context.setLineWidth(4); this.events.forEach((event, index) => { const xy = point(event); index ? context.lineTo(xy.x, xy.y) : context.moveTo(xy.x, xy.y) }); context.stroke(); this.events.forEach((event) => { const xy = point(event); context.setFillStyle(['#76d7ff', '#b9f58b', '#ffbe75'][event.band] || '#fff'); context.beginPath(); context.arc(xy.x, xy.y, 7, 0, Math.PI * 2); context.fill() }); context.draw() },
			stopPlayback() { tone.stop(); this.playing = false },
			togglePlay() { if (this.playing) return this.stopPlayback(); if (!this.events.length) return; if (!tone.play(this.events, () => { this.playing = false })) { uni.showToast({ title: '当前环境不支持本地音色', icon: 'none' }); return } this.playing = true }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 36rpx; background: #161424; color: #f8f6ff; box-sizing: border-box; } .top { display: flex; align-items: center; justify-content: space-between; margin-bottom: 36rpx; } .title { display: block; font-size: 40rpx; font-weight: 700; } .copy { display: block; margin-top: 10rpx; color: #aaa5bd; font-size: 23rpx; } .play { margin: 0; color: #171426; background: #c4a2ff; border: 0; border-radius: 18rpx; font-size: 24rpx; } .play::after { border: 0; } .trace, .events { margin-bottom: 28rpx; padding: 26rpx; background: #211d35; border-radius: 26rpx; } .trace-title { display: block; margin-bottom: 20rpx; color: #c8b5ff; font-size: 25rpx; } .canvas { width: 650rpx; height: 270rpx; background: #171426; border-radius: 14rpx; } .event { display: flex; justify-content: space-between; padding: 20rpx 0; border-bottom: 1rpx solid #38324f; color: #ddd8eb; font-size: 21rpx; } .event:last-child { border: 0; } .waiting { color: #aaa5bd; text-align: center; padding: 30rpx 0; }
</style>
