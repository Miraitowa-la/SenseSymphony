<template>
	<view class="page">
		<view class="header"><view><text class="title">挑战成绩</text><text class="copy">{{ hint }}</text></view><button class="refresh" :disabled="songReceiving || receiving" @click="refresh">刷新</button></view>
		<view v-if="records.length" class="list"><view v-for="item in records" :key="item.slot" class="card"><view class="card-top"><view><text class="sequence">第 {{ item.sequence }} 次挑战</text><text class="song">{{ songName(item.songId) }}</text></view><button class="delete" :disabled="deletingSlot !== null" @click.stop="deleteRecord(item)">{{ deletingSlot === item.slot ? '删除中' : '删除' }}</button></view><view class="stats"><view><text class="number">{{ item.score }}</text><text>分数</text></view><view><text class="number">{{ item.accuracy }}%</text><text>准确率</text></view><view><text class="number">{{ item.maxCombo }}</text><text>最大连击</text></view></view><view class="judgements"><text>PERFECT {{ item.perfect }}</text><text>GREAT {{ item.great }}</text><text>GOOD {{ item.good }}</text><text>MISS {{ item.miss }}</text></view></view></view>
		<view v-else class="empty"><text>{{ emptyTitle }}</text><text>{{ emptyCopy }}</text></view>
	</view>
</template>

<script>
	import spp from '../../services/spp.js'
	export default {
		data() { return { records: [], songs: [], songReceiving: false, receiving: false, deletingSlot: null, disconnected: false, hint: '进入页面后自动获取', unsubscribe: null } },
		computed: {
			emptyTitle() { return this.disconnected ? '蓝牙已断开' : this.songReceiving || this.receiving ? '正在读取成绩…' : '暂无成绩记录' },
			emptyCopy() { return this.disconnected ? '请重新连接设备后刷新' : this.songReceiving || this.receiving ? '请稍候' : '完成挑战后会显示在这里' }
		},
		onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent); this.refresh() },
		onHide() { if (this.unsubscribe) this.unsubscribe() },
		methods: {
			refresh() { if (!spp.socket) { this.disconnected = true; this.hint = '请先连接 JDY-31-SPP'; return } this.disconnected = false; this.records = []; this.songs = []; this.receiving = false; this.songReceiving = true; this.hint = '正在读取曲目名称…'; spp.send('M2,SONGS\n') },
			loadScores() { if (!spp.socket) return; this.receiving = true; this.hint = '正在读取成绩历史…'; spp.send('M2,LIST\n') },
			onSppEvent(event) {
				if (event.type === 'status' && event.state !== '已连接') { this.disconnected = true; this.songReceiving = false; this.receiving = false; this.deletingSlot = null; this.hint = '连接已断开，请重新连接'; return }
				if (event.type !== 'frame') return
				const frame = event.frame
				if (frame.type === 'mode2_delete_ok' && frame.slot === this.deletingSlot) { this.deletingSlot = null; uni.showToast({ title: '已删除', icon: 'success' }); this.refresh(); return }
				if (frame.type === 'mode2_error' && this.deletingSlot !== null) { this.deletingSlot = null; uni.showToast({ title: '删除失败', icon: 'none' }); return }
				if (this.songReceiving) {
					if (frame.type === 'mode2_song') this.songs.push(frame)
					if (frame.type === 'mode2_end') { this.songReceiving = false; this.loadScores() }
					return
				}
				if (!this.receiving) return
				if (frame.type === 'mode2_item') this.records.push(frame)
				if (frame.type === 'mode2_end') { this.receiving = false; this.records.sort((a, b) => b.sequence - a.sequence); this.hint = this.records.length ? `共 ${this.records.length} 条成绩` : '暂无成绩记录' }
			},
			deleteRecord(item) {
				if (this.deletingSlot !== null) return
				uni.showModal({ title: '删除成绩', content: `确认删除第 ${item.sequence} 次挑战？`, success: (result) => { if (!result.confirm) return; this.deletingSlot = item.slot; if (!spp.send(`M2,DELETE,${item.slot}\n`)) { this.deletingSlot = null; uni.showToast({ title: '删除失败，请重新连接', icon: 'none' }) } } })
			},
			songName(songId) { const song = this.songs.find((item) => item.id === songId); return song ? song.name : `SONG ${songId}` }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 36rpx; background: #161424; color: #f8f6ff; box-sizing: border-box; } .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 40rpx; } .title { display: block; font-size: 46rpx; font-weight: 700; } .copy { display: block; margin-top: 10rpx; color: #aaa5bd; font-size: 24rpx; } .refresh { margin: 0; color: #d6c8ff; background: #2b2644; border: 0; border-radius: 18rpx; font-size: 24rpx; } .refresh::after, .delete::after { border: 0; } .card { margin-bottom: 20rpx; padding: 30rpx; background: #211d35; border-radius: 26rpx; } .card-top, .stats, .judgements { display: flex; justify-content: space-between; } .sequence { display: block; font-size: 30rpx; font-weight: 600; } .song { display: block; margin-top: 8rpx; color: #cbb7ff; font-size: 23rpx; letter-spacing: 2rpx; } .delete { margin: 0; padding: 0 18rpx; color: #ffb9c1; background: #41263b; border: 0; border-radius: 14rpx; font-size: 22rpx; line-height: 56rpx; } .delete[disabled] { opacity: .45; } .stats { margin: 30rpx 0 24rpx; } .stats view { display: flex; flex-direction: column; color: #aaa5bd; font-size: 21rpx; } .number { margin-bottom: 8rpx; color: #fff; font-size: 32rpx; font-weight: 600; } .judgements { padding-top: 22rpx; border-top: 1rpx solid #38324f; color: #bdb7cc; font-size: 19rpx; } .empty { margin-top: 180rpx; text-align: center; color: #aaa5bd; font-size: 27rpx; } .empty text { display: block; margin: 12rpx; }
</style>
