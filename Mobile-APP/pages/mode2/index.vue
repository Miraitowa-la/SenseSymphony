<template>
	<view class="page">
		<text class="eyebrow">MODE 02</text>
		<text class="title">歌曲与成绩</text>
		<text class="copy">管理开发板内置曲目，查看挑战结果。</text>
		<view class="card primary" @click="openSongs"><text class="index">01</text><text class="card-title">曲目管理</text><text class="card-copy">查看并切换开发板内置曲目</text></view>
		<view class="card" @click="openScores"><text class="index">02</text><text class="card-title">挑战成绩</text><text class="card-copy">查看和删除历史挑战成绩</text></view>
	</view>
</template>

<script>
	export default {
		methods: {
			openSongs() { uni.navigateTo({ url: '/pages/songs/songs' }) },
			openScores() { uni.navigateTo({ url: '/pages/scores/scores' }) }
		}
	}
</script>

<style>
	.page { min-height: 100vh; padding: 72rpx 40rpx; background: #161424; color: #f8f6ff; box-sizing: border-box; } .eyebrow { color: #bfa7ff; font-size: 20rpx; letter-spacing: 4rpx; } .title { display: block; margin-top: 18rpx; font-size: 52rpx; font-weight: 700; } .copy { display: block; margin: 12rpx 0 60rpx; color: #aaa5bd; font-size: 24rpx; } .card { margin-bottom: 24rpx; padding: 38rpx 34rpx; border: 2rpx solid #35304d; border-radius: 28rpx; } .primary { background: linear-gradient(135deg, #7654d7, #b36bef); border: 0; } .index { color: #c7b8ea; font-size: 22rpx; } .card-title { display: block; margin: 18rpx 0 8rpx; font-size: 38rpx; font-weight: 700; } .card-copy { display: block; color: #aaa5bd; font-size: 23rpx; } .primary .index, .primary .card-copy { color: #f0eaff; }
</style>
