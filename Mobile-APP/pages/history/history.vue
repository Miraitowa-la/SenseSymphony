<template>
	<view class="page">
		<view class="header"><view><text class="title">录制历史</text><text class="copy">{{ hint }}</text></view><button class="refresh" @click="refresh">刷新</button></view>
		<view v-if="records.length" class="list"><view v-for="item in records" :key="item.slot" class="item" @click="open(item)"><view><text class="sequence">第 {{ item.sequence }} 段演奏</text><text class="meta">槽位 {{ item.slot }} · {{ item.eventCount }} 个事件</text></view><view class="actions"><button class="delete" :disabled="deletingSlot !== null" @click.stop="deleteRecord(item)">{{ deletingSlot === item.slot ? '删除中' : '删除' }}</button><text class="arrow">›</text></view></view></view>
		<view v-else class="empty"><text>暂时没有录制</text><text>连接设备后点击刷新获取记录</text></view>
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
			onSppEvent(event) {
				if (event.type === 'status' && event.state !== '已连接') { this.hint = '连接已断开，请重新连接'; if (this.deletingSlot !== null) this.deletingSlot = null; return }
				if (event.type !== 'frame') return
				const frame = event.frame
				if (frame.type === 'delete_ok' && frame.slot === this.deletingSlot) { this.deletingSlot = null; uni.showToast({ title: '已删除', icon: 'success' }); this.refresh(); return }
				if (frame.type === 'error' && this.deletingSlot !== null) { this.deletingSlot = null; uni.showToast({ title: '删除失败', icon: 'none' }); return }
				if (!this.receiving) return
				if (frame.type === 'item') this.records.push(frame)
				if (frame.type === 'end') { this.receiving = false; this.hint = this.records.length ? `共 ${this.records.length} 条录制` : '没有录制数据' }
			},
			deleteRecord(item) {
				if (this.deletingSlot !== null) return
				uni.showModal({ title: '删除录制', content: `确认删除第 ${item.sequence} 段演奏？`, success: (result) => {
					if (!result.confirm) return
					this.deletingSlot = item.slot
					if (!spp.send(`M1,DELETE,${item.slot}\n`)) { this.deletingSlot = null; uni.showToast({ title: '删除失败，请重新连接', icon: 'none' }) }
				} })
			},
			open(item) { uni.navigateTo({ url: `/pages/record/record?slot=${item.slot}&sequence=${item.sequence}&eventCount=${item.eventCount}` }) }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 36rpx; background: #161424; color: #f8f6ff; box-sizing: border-box; } .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 40rpx; } .title { display: block; font-size: 46rpx; font-weight: 700; } .copy, .meta { display: block; margin-top: 10rpx; color: #aaa5bd; font-size: 24rpx; } .refresh { margin: 0; color: #d6c8ff; background: #2b2644; border: 0; border-radius: 18rpx; font-size: 24rpx; } .refresh::after, .delete::after { border: 0; } .list { background: #211d35; border-radius: 26rpx; overflow: hidden; } .item { display: flex; align-items: center; justify-content: space-between; padding: 30rpx; border-bottom: 1rpx solid #38324f; } .item:last-child { border: 0; } .sequence { font-size: 30rpx; } .actions { display: flex; align-items: center; gap: 16rpx; } .delete { margin: 0; padding: 0 18rpx; color: #ffb9c1; background: #41263b; border: 0; border-radius: 14rpx; font-size: 22rpx; line-height: 56rpx; } .delete[disabled] { opacity: .45; } .arrow { color: #c7b4fb; font-size: 50rpx; } .empty { margin-top: 180rpx; text-align: center; color: #aaa5bd; font-size: 27rpx; } .empty text { display: block; margin: 12rpx; }
</style>
