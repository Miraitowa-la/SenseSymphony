<template>
	<view class="page">
		<text class="title">我的曲谱</text>
		<view class="name-row"><input v-model="name" :disabled="uploading" maxlength="20" placeholder="MY_SONG" /><text>›</text></view>
		<view class="control-row"><view class="beat"><text>节拍</text><input v-model.number="beatMs" :disabled="uploading" type="number" /><text>ms</text></view><button :disabled="uploading" @click="arrange">自动排列</button><button :disabled="uploading" @click="preview">{{ playing ? '停止' : '试听' }}</button></view>
		<view class="notes"><view v-for="(item,index) in notes" :key="index" class="note"><text class="index">{{ String(index + 1).padStart(2, '0') }}</text><text class="arrow">›</text><picker :range="noteNames" :value="item.note" :disabled="uploading" @change="changeNote(index,$event)"><view class="note-name" :class="'tone-' + item.note">{{ noteNames[item.note] }}</view></picker><text class="time-label">时间</text><input v-model.number="item.timeMs" :disabled="uploading" type="number" /><text class="ms">ms</text><button :disabled="uploading" @click="removeNote(index)">删除</button></view></view>
		<button class="add" :disabled="uploading || notes.length >= 42" @click="addNote">＋ 添加音符</button>
		<text class="summary">共 {{ notes.length }} 个音符</text>
		<button class="upload" :disabled="uploading" @click="upload">{{ uploading ? uploadLabel : '上传曲谱' }}</button>
		<button v-if="uploading" class="cancel" @click="cancelUpload">取消上传</button>
	</view>
</template>

<script>
import spp from '../../services/spp.js'
import * as tone from '../../utils/tone.js'
export default {
	data() { return { name: 'MY_SONG', beatMs: 600, notes: [{ timeMs: 0, note: 0 }], noteNames: ['Do', 'Re', 'Mi', 'Fa', 'Sol', 'La', 'Si'], playing: false, uploading: false, uploadState: 'idle', uploadIndex: 0, uploadNotes: [], unsubscribe: null } },
	computed: { uploadLabel() { return this.uploadState === 'begin' ? '正在开始上传…' : this.uploadState === 'commit' ? '正在提交…' : `正在上传 ${this.uploadIndex + 1}/${this.uploadNotes.length}` } },
	onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent) },
	onHide() { tone.stop(); this.playing = false; if (this.uploading) this.abort('上传已取消') },
	onUnload() { tone.stop(); if (this.unsubscribe) this.unsubscribe() },
	onBackPress() { if (this.uploading) { this.cancelUpload(); return true } },
	methods: {
		check() { const name = String(this.name || '').trim().toUpperCase(), beat = Number(this.beatMs); if (!/^[A-Z0-9_]{1,20}$/.test(name)) return '曲名仅限 A-Z、0-9、下划线'; if (!Number.isInteger(beat) || beat < 200 || beat > 2000) return '节拍范围为 200–2000 ms'; if (!this.notes.length || this.notes.length > 42) return '音符数量应为 1–42'; let last = -1; for (const n of this.notes) { if (!Number.isInteger(Number(n.timeMs)) || Number(n.timeMs) < 0 || Number(n.timeMs) <= last) return '时间必须严格递增'; if (!Number.isInteger(Number(n.note)) || n.note < 0 || n.note > 6) return '音符无效'; last = Number(n.timeMs) } return { name, beat, notes: this.notes.map((n) => ({ timeMs: Number(n.timeMs), note: Number(n.note) })) } },
		arrange() { const beat = Number(this.beatMs); if (!Number.isInteger(beat) || beat < 200 || beat > 2000) return uni.showToast({ title: '节拍范围为 200–2000 ms', icon: 'none' }); this.notes.forEach((n, i) => { n.timeMs = i * beat }) },
		addNote() { const beat = Number(this.beatMs); if (this.notes.length >= 42) return; if (!Number.isInteger(beat) || beat < 200 || beat > 2000) return uni.showToast({ title: '请先填写合法节拍', icon: 'none' }); const last = this.notes[this.notes.length - 1]; this.notes.push({ timeMs: Number(last.timeMs) + beat, note: 0 }) },
		removeNote(i) { if (this.notes.length > 1) this.notes.splice(i, 1); else uni.showToast({ title: '曲谱至少保留一个音符', icon: 'none' }) },
		changeNote(i, e) { this.notes[i].note = Number(e.detail.value) },
		preview() { if (this.playing) { tone.stop(); this.playing = false; return } const data = this.check(); if (typeof data === 'string') return uni.showToast({ title: data, icon: 'none' }); this.playing = tone.play(data.notes.map((n) => ({ ...n, band: 1 })), () => { this.playing = false }); if (!this.playing) uni.showToast({ title: '当前环境无法试听', icon: 'none' }) },
		upload() { const data = this.check(); if (typeof data === 'string') return uni.showToast({ title: data, icon: 'none' }); if (!spp.socket) return uni.showToast({ title: '请先连接设备', icon: 'none' }); uni.showModal({ title: '上传曲谱', content: `共 ${data.notes.length} 个音符，预计上传很快`, success: (r) => { if (!r.confirm) return; this.name = data.name; this.uploading = true; this.uploadState = 'begin'; this.uploadIndex = 0; this.uploadNotes = data.notes; if (!spp.send(`M2,UPLOAD,BEGIN,${data.name},${data.beat},${data.notes.length}\n`)) this.abort('上传失败，请重新连接') } }) },
		onSppEvent(e) { if (e.type === 'status' && e.state !== '已连接') { if (this.uploading) this.abort('连接已断开', false); return } if (e.type !== 'frame' || !this.uploading) return; const f = e.frame; if (f.type === 'mode2_error') return this.abort(`上传失败：${f.reason}`); if (this.uploadState === 'begin' && f.type === 'mode2_upload_begin_ok') { this.uploadState = 'note'; return this.sendNote() } if (this.uploadState === 'note' && f.type === 'mode2_upload_note_ok') { if (f.index !== this.uploadIndex + 1) return this.abort('音符确认序号异常'); this.uploadIndex++; if (this.uploadIndex < this.uploadNotes.length) return this.sendNote(); this.uploadState = 'commit'; if (!spp.send('M2,UPLOAD,COMMIT\n')) this.abort('提交失败'); return } if (this.uploadState === 'commit' && f.type === 'mode2_upload_ok') { this.uploading = false; this.uploadState = 'idle'; uni.showToast({ title: '上传成功', icon: 'success' }); setTimeout(() => uni.navigateBack(), 500) } },
		sendNote() { const n = this.uploadNotes[this.uploadIndex]; if (!spp.send(`M2,UPLOAD,NOTE,${n.timeMs},${n.note}\n`)) this.abort('上传失败，请重新连接') },
		abort(message, sendCancel = true) { const active = this.uploading; this.uploading = false; this.uploadState = 'idle'; if (active && sendCancel && spp.socket) spp.send('M2,UPLOAD,CANCEL\n'); uni.showToast({ title: message, icon: 'none' }) },
		cancelUpload() { this.abort('已取消上传') }
	}
}
</script>

<style>
.page { min-height: 100vh; padding: 30rpx 28rpx 52rpx; color: #f8f6ff; background: radial-gradient(circle at 80% 0, #151343, transparent 38%), #070917; box-sizing: border-box; } .title { display: block; margin: 4rpx 4rpx 24rpx; font-size: 42rpx; font-weight: 700; }
.name-row { display: flex; align-items: center; height: 66rpx; padding: 0 18rpx; border: 1rpx solid #303050; border-radius: 15rpx; background: #11142a; } .name-row input { flex: 1; color: #eee8ff; font-size: 24rpx; } .name-row text { color: #a8a1bf; font-size: 38rpx; }
.control-row { display: flex; gap: 12rpx; margin: 18rpx 0; } .beat { display: flex; align-items: center; flex: 1; padding: 0 14rpx; border-radius: 13rpx; background: #171a31; } .beat text { color: #a69fbd; font-size: 19rpx; } .beat input { width: 66rpx; margin: 0 10rpx; color: #fff; font-size: 24rpx; } .control-row button { margin: 0; padding: 0 16rpx; border: 0; border-radius: 13rpx; color: #d9ceff; font-size: 20rpx; line-height: 62rpx; background: #2d2750; } .control-row button::after, .note button::after, .add::after, .upload::after, .cancel::after { border: 0; }
.notes { display: flex; flex-direction: column; gap: 10rpx; } .note { display: flex; align-items: center; min-height: 72rpx; padding: 0 14rpx; border: 1rpx solid #282945; border-radius: 13rpx; background: linear-gradient(90deg, #12152c, #15172e); box-sizing: border-box; } .index { width: 44rpx; color: #91c4ff; font-size: 22rpx; } .arrow { margin-right: 12rpx; color: #77738e; font-size: 29rpx; } .note picker { width: 86rpx; } .note-name { padding: 7rpx 0; border-radius: 9rpx; color: #fff; font-size: 23rpx; text-align: center; } .tone-0 { background: #6846c9; } .tone-1 { background: #7560da; } .tone-2 { background: #c49343; } .tone-3 { background: #c85761; } .tone-4 { background: #54aa78; } .tone-5 { background: #4d82c7; } .tone-6 { background: #8248a9; } .time-label { margin-left: 16rpx; color: #9d98b2; font-size: 18rpx; } .note input { width: 78rpx; margin-left: auto; color: #fff; font-size: 22rpx; text-align: right; } .ms { margin-left: 7rpx; color: #aaa5bd; font-size: 18rpx; } .note button { margin: 0 0 0 13rpx; padding: 0; border: 0; color: #e1bee8; font-size: 18rpx; background: transparent; }
.add { width: 100%; margin-top: 24rpx; border: 1rpx solid #974ee2; border-radius: 15rpx; color: #eee4ff; font-size: 26rpx; line-height: 76rpx; background: linear-gradient(100deg, #40216d, #39258a); } .summary { display: block; margin: 22rpx 4rpx; color: #aaa5bd; font-size: 22rpx; } .upload { width: 100%; border: 1rpx solid #c46af7; border-radius: 16rpx; color: #fff; font-size: 29rpx; line-height: 88rpx; background: linear-gradient(100deg, #a43bda, #4453dc); box-shadow: 0 0 20rpx #7d45d455; } .cancel { width: 100%; margin-top: 14rpx; border: 1rpx solid #a64a64; border-radius: 15rpx; color: #ffb2c5; font-size: 23rpx; line-height: 70rpx; background: #3b1f30; }
</style>
