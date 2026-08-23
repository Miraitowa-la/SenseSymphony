<template>
	<view class="page">
		<view class="header"><view><text class="title">挑战成绩</text><text class="copy">{{ hint }}</text></view><button class="refresh" :disabled="songReceiving || receiving" @click="refresh">刷新</button></view>
		<view v-if="records.length">
			<text class="section-title">我的最佳成绩</text>
			<view class="best"><view class="best-main"><text class="best-label">总分</text><text class="best-score">{{ bestRecord.score }}</text><text class="best-song">{{ songName(bestRecord.songId) }}</text></view><text class="grade" :class="'grade-' + grade(bestRecord)">{{ grade(bestRecord) }}</text><view class="best-stats"><view><text>准确率</text><strong>{{ bestRecord.accuracy }}%</strong></view><view><text>最大连击</text><strong>{{ bestRecord.maxCombo }}</strong></view><view><text>完美数</text><strong>{{ bestRecord.perfect }}</strong></view></view></view>
			<text class="section-title records-title">成绩记录 »</text>
			<view class="records"><view v-for="item in records" :key="item.slot" class="record"><view class="record-info"><text class="record-score">{{ item.score }}</text><text class="record-meta">{{ songName(item.songId) }} · 第 {{ item.sequence }} 次</text></view><text class="record-accuracy">{{ item.accuracy }}%</text><text class="record-combo">{{ item.maxCombo }}</text><text class="record-grade" :class="'grade-' + grade(item)">{{ grade(item) }}</text><button class="delete" :disabled="deletingSlot !== null" @click.stop="deleteRecord(item)">{{ deletingSlot === item.slot ? '删除中' : '删除' }}</button></view></view>
		</view>
		<view v-else class="empty"><text>{{ emptyTitle }}</text><text>{{ emptyCopy }}</text></view>
	</view>
</template>

<script>
	import spp from '../../services/spp.js'
	export default {
		data() { return { records: [], songs: [], songReceiving: false, receiving: false, deletingSlot: null, disconnected: false, hint: '进入页面后自动获取', unsubscribe: null } },
		computed: {
			emptyTitle() { return this.disconnected ? '蓝牙已断开' : this.songReceiving || this.receiving ? '正在读取成绩…' : '暂无成绩记录' },
			emptyCopy() { return this.disconnected ? '请重新连接设备后刷新' : this.songReceiving || this.receiving ? '请稍候' : '完成挑战后会显示在这里' },
			bestRecord() { return this.records.reduce((best, item) => item.score > best.score ? item : best, this.records[0]) }
		},
		onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent); this.refresh() },
		onHide() { if (this.unsubscribe) this.unsubscribe() },
		methods: {
			refresh() { if (!spp.socket) { this.disconnected = true; this.hint = '请先连接 JDY-31-SPP'; return } this.disconnected = false; this.records = []; this.songs = []; this.receiving = false; this.songReceiving = true; this.hint = '正在读取曲目名称…'; spp.send('M2,SONGS\n') },
			loadScores() { if (!spp.socket) return; this.receiving = true; this.hint = '正在读取成绩历史…'; spp.send('M2,LIST\n') },
			onSppEvent(event) { if (event.type === 'status' && event.state !== '已连接') { this.disconnected = true; this.songReceiving = false; this.receiving = false; this.deletingSlot = null; this.hint = '连接已断开，请重新连接'; return } if (event.type !== 'frame') return; const frame = event.frame; if (frame.type === 'mode2_delete_ok' && frame.slot === this.deletingSlot) { this.deletingSlot = null; uni.showToast({ title: '已删除', icon: 'success' }); this.refresh(); return } if (frame.type === 'mode2_error' && this.deletingSlot !== null) { this.deletingSlot = null; uni.showToast({ title: '删除失败', icon: 'none' }); return } if (this.songReceiving) { if (frame.type === 'mode2_song') this.songs.push(frame); if (frame.type === 'mode2_end') { this.songReceiving = false; this.loadScores() } return } if (!this.receiving) return; if (frame.type === 'mode2_item') this.records.push(frame); if (frame.type === 'mode2_end') { this.receiving = false; this.records.sort((a, b) => b.sequence - a.sequence); this.hint = this.records.length ? `共 ${this.records.length} 条成绩` : '暂无成绩记录' } },
			deleteRecord(item) { if (this.deletingSlot !== null) return; uni.showModal({ title: '删除成绩', content: `确认删除第 ${item.sequence} 次挑战？`, success: (result) => { if (!result.confirm) return; this.deletingSlot = item.slot; if (!spp.send(`M2,DELETE,${item.slot}\n`)) { this.deletingSlot = null; uni.showToast({ title: '删除失败，请重新连接', icon: 'none' }) } } }) },
			songName(songId) { const song = this.songs.find((item) => item.id === songId); return song ? song.name : `SONG ${songId}` },
			grade(item) { const accuracy = Number(item.accuracy) || 0; return accuracy >= 95 ? 'S' : accuracy >= 85 ? 'A' : accuracy >= 70 ? 'B' : 'C' }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 30rpx 28rpx 54rpx; color: #f8f6ff; background: radial-gradient(circle at 82% 2%, #171540, transparent 42%), #070917; box-sizing: border-box; } .header { display: flex; align-items: center; justify-content: space-between; margin: 4rpx 6rpx 28rpx; } .title { display: block; font-size: 42rpx; font-weight: 700; } .copy { display: block; max-width: 460rpx; margin-top: 8rpx; overflow: hidden; color: #9e9ab7; font-size: 21rpx; text-overflow: ellipsis; white-space: nowrap; } .refresh { margin: 0; padding: 0 20rpx; border: 1rpx solid #4a3a82; border-radius: 16rpx; color: #d9cbff; font-size: 22rpx; line-height: 56rpx; background: #181633; } .refresh::after, .delete::after { border: 0; }
	.section-title { display: block; margin: 12rpx 4rpx 16rpx; color: #f4efff; font-size: 25rpx; font-weight: 600; } .best { position: relative; overflow: hidden; padding: 24rpx 26rpx 0; border: 1rpx solid #5c39a9; border-radius: 18rpx; background: radial-gradient(circle at 72% 15%, #9258dd66, transparent 32%), linear-gradient(125deg, #241144, #1a1738); } .best::after { position: absolute; right: -20rpx; bottom: 66rpx; width: 210rpx; height: 54rpx; border-radius: 50%; content: ''; background: #9e5dff33; filter: blur(18rpx); transform: rotate(-8deg); } .best-main { position: relative; z-index: 1; } .best-label, .best-score, .best-song { display: block; } .best-label { color: #c5b9e6; font-size: 20rpx; } .best-score { margin-top: 4rpx; font-size: 46rpx; font-weight: 700; letter-spacing: 1rpx; } .best-song { margin-top: 4rpx; max-width: 400rpx; overflow: hidden; color: #bda5ff; font-size: 19rpx; text-overflow: ellipsis; white-space: nowrap; } .grade { position: absolute; z-index: 1; top: 18rpx; right: 32rpx; font-size: 88rpx; font-style: italic; font-weight: 800; text-shadow: 0 0 22rpx currentColor; } .grade-S { color: #ffdd77; } .grade-A { color: #9db7ff; } .grade-B { color: #a5dcff; } .grade-C { color: #d69eff; } .best-stats { position: relative; z-index: 1; display: flex; justify-content: space-between; margin: 18rpx -26rpx 0; padding: 18rpx 26rpx; border-top: 1rpx solid #49366f; } .best-stats view { display: flex; flex-direction: column; color: #b7b1c8; font-size: 18rpx; } .best-stats strong { margin-top: 7rpx; color: #fff; font-size: 27rpx; } .records-title { margin-top: 28rpx; }
	.records { display: flex; flex-direction: column; gap: 13rpx; } .record { display: flex; align-items: center; min-height: 82rpx; padding: 14rpx 18rpx; border: 1rpx solid #242343; border-radius: 14rpx; background: #13162c; box-sizing: border-box; } .record-info { flex: 1; min-width: 0; } .record-score, .record-meta { display: block; } .record-score { font-size: 26rpx; font-weight: 700; } .record-meta { max-width: 210rpx; margin-top: 5rpx; overflow: hidden; color: #8f8aa8; font-size: 18rpx; text-overflow: ellipsis; white-space: nowrap; } .record-accuracy, .record-combo { width: 92rpx; color: #e5e1ed; font-size: 22rpx; font-weight: 600; text-align: center; } .record-grade { width: 40rpx; font-size: 35rpx; font-style: italic; font-weight: 800; text-align: center; } .delete { margin: 0 0 0 10rpx; padding: 0 9rpx; border: 0; color: #ffb7c9; font-size: 18rpx; line-height: 44rpx; background: transparent; } .delete[disabled] { opacity: .45; } .empty { margin-top: 180rpx; color: #aaa5bd; font-size: 27rpx; text-align: center; } .empty text { display: block; margin: 12rpx; }
</style>
