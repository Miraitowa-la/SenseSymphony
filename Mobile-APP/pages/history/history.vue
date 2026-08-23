<template>
	<view class="page">
		<view class="header"><view><text class="title">录制历史</text><text class="copy">{{ hint }}</text></view><button class="refresh" @click="refresh">刷新</button></view>
		<view v-if="records.length" class="list"><view v-for="item in records" :key="item.slot" class="item" @click="open(item)"><view class="music-icon">♫</view><view class="item-copy"><text class="sequence">第 {{ item.sequence }} 段演奏</text><text class="meta">录制槽位 {{ item.slot }}</text><text class="badge">音符数：{{ item.eventCount }}</text></view><view class="actions"><button class="delete" :disabled="deletingSlot !== null" @click.stop="deleteRecord(item)">{{ deletingSlot === item.slot ? '删除中' : '删除' }}</button></view></view></view>
		<view v-else class="empty"><text>暂无录制</text><text>连接设备后点击刷新获取记录</text></view>
	</view>
</template>

<script>
	import spp from '../../services/spp.js'
	export default {
		data() { return { records: [], hint: '进入页面后自动获取', receiving: false, deletingSlot: null, unsubscribe: null } },
		onShow() { this.unsubscribe = spp.subscribe(this.onSppEvent); this.refresh() },
		onHide() { if (this.unsubscribe) this.unsubscribe() },
		methods: {
			refresh() { if (!spp.socket) { this.hint = '请先连接 JDY-31-SPP'; return } this.records = []; this.receiving = true; this.hint = '正在读取录制列表…'; spp.send('M1,LIST\n') },
			onSppEvent(event) { if (event.type === 'status' && event.state !== '已连接') { this.hint = '连接已断开，请重新连接'; if (this.deletingSlot !== null) this.deletingSlot = null; return } if (event.type !== 'frame') return; const frame = event.frame; if (frame.type === 'delete_ok' && frame.slot === this.deletingSlot) { this.deletingSlot = null; uni.showToast({ title: '已删除', icon: 'success' }); this.refresh(); return } if (frame.type === 'error' && this.deletingSlot !== null) { this.deletingSlot = null; uni.showToast({ title: '删除失败', icon: 'none' }); return } if (!this.receiving) return; if (frame.type === 'item') this.records.push(frame); if (frame.type === 'end') { this.receiving = false; this.hint = this.records.length ? `共 ${this.records.length} 条录制` : '没有录制数据' } },
			deleteRecord(item) { if (this.deletingSlot !== null) return; uni.showModal({ title: '删除录制', content: `确认删除第 ${item.sequence} 段演奏？`, success: (result) => { if (!result.confirm) return; this.deletingSlot = item.slot; if (!spp.send(`M1,DELETE,${item.slot}\n`)) { this.deletingSlot = null; uni.showToast({ title: '删除失败，请重新连接', icon: 'none' }) } } }) },
			open(item) { uni.navigateTo({ url: `/pages/record/record?slot=${item.slot}&sequence=${item.sequence}&eventCount=${item.eventCount}` }) }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 30rpx 28rpx 52rpx; color: #f8f6ff; background: radial-gradient(circle at 80% 0, #171442, transparent 42%), #070917; box-sizing: border-box; } .header { display: flex; align-items: center; justify-content: space-between; margin: 4rpx 6rpx 28rpx; } .title { display: block; font-size: 42rpx; font-weight: 700; } .copy { display: block; max-width: 460rpx; margin-top: 8rpx; overflow: hidden; color: #9e9ab7; font-size: 21rpx; text-overflow: ellipsis; white-space: nowrap; } .refresh { margin: 0; padding: 0 20rpx; border: 1rpx solid #4a3a82; border-radius: 16rpx; color: #d9cbff; font-size: 22rpx; line-height: 56rpx; background: #181633; } .refresh::after, .delete::after { border: 0; }
	.list { display: flex; flex-direction: column; gap: 18rpx; } .item { display: flex; align-items: center; min-height: 128rpx; padding: 18rpx; border: 1rpx solid #7043ba; border-radius: 18rpx; background: linear-gradient(120deg, #201437, #111b3d); box-shadow: inset 0 0 30rpx #8358f011; } .music-icon { display: flex; align-items: center; justify-content: center; width: 78rpx; height: 78rpx; margin-right: 18rpx; border: 1rpx solid #a568ff; border-radius: 12rpx; color: #fff; font-size: 48rpx; background: linear-gradient(135deg, #6d3ccd, #a356e5); box-shadow: 0 0 20rpx #8d4dff55; } .item-copy { flex: 1; min-width: 0; } .sequence, .meta, .badge { display: block; } .sequence { font-size: 28rpx; font-weight: 700; } .meta { margin-top: 8rpx; color: #aaa6c1; font-size: 20rpx; } .badge { width: fit-content; margin-top: 9rpx; padding: 3rpx 10rpx; border-radius: 8rpx; color: #e4d4ff; font-size: 18rpx; background: #3a254f; } .actions { display: flex; align-items: center; } .delete { min-width: 96rpx; height: 62rpx; margin: 0; padding: 0 18rpx; border: 1rpx solid #634796; border-radius: 13rpx; color: #f0dfff; font-size: 24rpx; line-height: 62rpx; background: #2a2148; } .delete[disabled] { opacity: .45; } .empty { margin-top: 180rpx; color: #aaa5bd; font-size: 27rpx; text-align: center; } .empty text { display: block; margin: 12rpx; }
</style>
