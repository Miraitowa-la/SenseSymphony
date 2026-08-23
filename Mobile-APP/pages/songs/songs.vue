<template>
	<view class="page">
		<view class="section-head"><text>当前曲目 ♪</text><button class="refresh" :disabled="receiving" @click="refresh">刷新</button></view>
		<view class="current-card"><view class="cover hero-cover" :class="coverClass(currentSong)"><text>♪</text></view><view class="current-copy"><text class="current-name">{{ currentSongName }}</text><text class="song-meta">{{ currentSong ? songKind(currentSong.id) : '正在读取' }} · ID {{ currentSong ? currentSong.id : '—' }}</text><text class="active-badge">当前使用中</text></view></view>

		<view class="section-head list-head"><text>曲目列表 ♪</text><text v-if="!receiving" class="hint">{{ hint }}</text></view>
		<view v-if="songs.length" class="song-list"><view v-for="song in songs" :key="song.id" class="song-row" :class="{ active: song.id === selectedId }" @click="selectSong(song)"><view class="cover row-cover" :class="coverClass(song)"><text>♪</text></view><view class="song-copy"><text class="song-name">{{ song.name }}</text><text class="song-meta">{{ songKind(song.id) }} · ID {{ song.id }}</text></view><button v-if="song.id >= 16" class="delete" :disabled="deletingId !== null" @click.stop="deleteSong(song)">{{ deletingId === song.id ? '删除中' : '删除' }}</button><text v-else-if="song.id === selectedId" class="active-badge">当前使用中</text><button v-else class="switch" :disabled="selectingId !== null || deletingId !== null" @click.stop="selectSong(song)">{{ selectingId === song.id ? '切换中' : '切换' }}</button></view></view>
		<view v-else class="empty">{{ disconnected ? '蓝牙已断开' : receiving ? '正在读取内置曲目…' : '暂无曲目' }}</view>
		<button class="create" :disabled="receiving || deletingId !== null" @click="createSong">上传新曲目</button>
	</view>
</template>

<script>
import spp from '../../services/spp.js'
export default {
	data() { return { songs: [], selectedId: null, receiving: false, selectingId: null, deletingId: null, disconnected: false, hint: '读取开发板曲目', unsubscribe: null } },
	computed: {
		currentSong() { return this.songs.find((item) => item.id === this.selectedId) },
		currentSongName() { return this.currentSong ? this.currentSong.name : '正在读取…' }
	},
	onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent); this.refresh() },
	onHide() { if (this.unsubscribe) this.unsubscribe() },
	methods: {
		songKind(id) { return id < 16 ? '内置曲目' : '我的曲目' },
		coverClass(song) { return `cover-${((song && song.id) || 0) % 4}` },
		createSong() { uni.navigateTo({ url: '/pages/song-editor/song-editor' }) },
		refresh() { if (!spp.socket) { this.disconnected = true; this.hint = '请先连接 JDY-31-SPP'; return } this.disconnected = false; this.songs = []; this.receiving = true; spp.send('M2,SONGS\n') },
		onSppEvent(event) { if (event.type === 'status' && event.state !== '已连接') { this.disconnected = true; this.receiving = false; this.selectingId = null; this.deletingId = null; return } if (event.type !== 'frame') return; const f = event.frame; if (f.type === 'mode2_select_ok' && f.id === this.selectingId) { this.selectingId = null; this.selectedId = f.id; uni.showToast({ title: `已切换为 ${this.currentSongName}`, icon: 'success' }); return } if (f.type === 'mode2_song_delete_ok' && f.songId === this.deletingId) { this.deletingId = null; uni.showToast({ title: '已删除', icon: 'success' }); this.refresh(); return } if (f.type === 'mode2_error' && this.selectingId !== null) { this.selectingId = null; uni.showToast({ title: f.reason === 'BUSY' ? '当前挑战正在进行，请结束后再切换' : '切换失败', icon: 'none' }); return } if (f.type === 'mode2_error' && this.deletingId !== null) { this.deletingId = null; uni.showToast({ title: f.reason === 'READONLY' ? '内置曲目不可删除' : '删除失败', icon: 'none' }); return } if (!this.receiving) return; if (f.type === 'mode2_songs') this.selectedId = f.selectedId; if (f.type === 'mode2_song') this.songs.push(f); if (f.type === 'mode2_end') { this.receiving = false; this.hint = `共 ${this.songs.length} 首曲目` } },
		selectSong(song) { if (!song || song.id === this.selectedId || this.selectingId !== null || this.deletingId !== null) return; uni.showModal({ title: '切换曲目', content: `确认切换为 ${song.name}？`, success: (r) => { if (!r.confirm) return; this.selectingId = song.id; if (!spp.send(`M2,SELECT,${song.id}\n`)) { this.selectingId = null; uni.showToast({ title: '切换失败，请重新连接', icon: 'none' }) } } }) },
		deleteSong(song) { if (!song || song.id < 16 || this.deletingId !== null) return; uni.showModal({ title: '删除曲目', content: `确认删除 ${song.name}？`, success: (r) => { if (!r.confirm) return; this.deletingId = song.id; if (!spp.send(`M2,SONG,DELETE,${song.id}\n`)) { this.deletingId = null; uni.showToast({ title: '删除失败，请重新连接', icon: 'none' }) } } }) }
	}
}
</script>

<style>
.page { min-height: 100vh; padding: 28rpx 28rpx 52rpx; color: #f8f6ff; background: radial-gradient(circle at 80% 0, #151343, transparent 38%), #070917; box-sizing: border-box; }
.section-head { display: flex; align-items: center; justify-content: space-between; margin: 6rpx 2rpx 16rpx; color: #f5f1ff; font-size: 26rpx; font-weight: 600; } .list-head { margin-top: 30rpx; } .hint { max-width: 330rpx; overflow: hidden; color: #9591ae; font-size: 19rpx; font-weight: 400; text-overflow: ellipsis; white-space: nowrap; }
.refresh { margin: 0; padding: 0 20rpx; border: 1rpx solid #48367b; border-radius: 14rpx; color: #d9ccff; font-size: 21rpx; line-height: 54rpx; background: #171530; } .refresh::after, .switch::after, .delete::after, .create::after { border: 0; }
.current-card { display: flex; align-items: center; min-height: 160rpx; padding: 18rpx; border: 1rpx solid #8846d2; border-radius: 18rpx; background: linear-gradient(120deg, #211443, #111a39); box-shadow: inset 0 0 28rpx #9159ed16; } .cover { display: flex; align-items: center; justify-content: center; overflow: hidden; border: 1rpx solid #a879ff; border-radius: 12rpx; color: #f9eeff; background-color: #30235e; box-shadow: inset 0 0 20rpx #9e73ff55; } .cover text { font-size: 46rpx; text-shadow: 0 0 16rpx #fff; } .hero-cover { width: 124rpx; height: 124rpx; margin-right: 20rpx; } .row-cover { width: 68rpx; height: 68rpx; margin-right: 16rpx; border-radius: 10rpx; } .row-cover text { font-size: 31rpx; }
.cover-0 { background-image: radial-gradient(circle at 50% 35%, #b9e9ff 0 2rpx, transparent 4rpx), radial-gradient(circle at 22% 24%, #fff 0 1rpx, transparent 3rpx), linear-gradient(145deg, #152b6d, #5a45c4 55%, #b37bff); } .cover-1 { background-image: radial-gradient(circle at 70% 26%, #ffe9a9 0 3rpx, transparent 5rpx), linear-gradient(145deg, #173e6c, #527ca0 55%, #98c7f5); } .cover-2 { background-image: radial-gradient(circle at 30% 26%, #ffe5b4 0 3rpx, transparent 5rpx), linear-gradient(145deg, #42263d, #966b49 55%, #e1bc73); } .cover-3 { background-image: radial-gradient(circle at 70% 25%, #f6b7d8 0 3rpx, transparent 5rpx), linear-gradient(145deg, #351d43, #70436c 55%, #d78aad); }
.current-copy, .song-copy { flex: 1; min-width: 0; } .current-name, .song-name, .song-meta { display: block; } .current-name { overflow: hidden; font-size: 30rpx; font-weight: 700; text-overflow: ellipsis; white-space: nowrap; } .song-name { overflow: hidden; color: #f3eeff; font-size: 25rpx; font-weight: 600; text-overflow: ellipsis; white-space: nowrap; } .song-meta { margin-top: 8rpx; color: #aaa4c5; font-size: 19rpx; } .active-badge { display: inline-block; margin-top: 12rpx; padding: 4rpx 11rpx; border-radius: 8rpx; color: #ddceff; font-size: 17rpx; background: #463091; }
.song-list { overflow: hidden; border: 1rpx solid #292949; border-radius: 16rpx; background: #11142a; } .song-row { display: flex; align-items: center; min-height: 94rpx; padding: 12rpx 16rpx; border-bottom: 1rpx solid #292945; } .song-row:last-child { border: 0; } .song-row.active { background: linear-gradient(90deg, #211445, #15183b); } .song-row .active-badge { margin: 0; white-space: nowrap; } .switch, .delete { min-width: 74rpx; margin: 0; padding: 0 13rpx; border: 0; border-radius: 10rpx; color: #fff; font-size: 20rpx; line-height: 48rpx; background: linear-gradient(135deg, #6e35bb, #7449c8); } .delete { color: #ffd1dc; background: #59263d; } .switch[disabled], .delete[disabled] { opacity: .48; }
.create { width: 100%; margin-top: 28rpx; border: 1rpx solid #c267f8; border-radius: 17rpx; color: #fff; font-size: 27rpx; line-height: 88rpx; background: linear-gradient(100deg, #a63bd8, #4652df); box-shadow: 0 0 20rpx #7d45d455; } .empty { margin: 100rpx 0; color: #aaa5bd; font-size: 25rpx; text-align: center; }
</style>
