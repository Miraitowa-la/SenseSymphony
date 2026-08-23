<template>
	<view class="page">
		<view class="profile"><view class="avatar">芯</view><view><text class="name">芯露谷老农民</text><text class="id">ID: 10086</text></view></view>
		<view class="menu"><view v-for="item in items" :key="item.label" class="item"><text class="icon">{{ item.icon }}</text><text>{{ item.label }}</text><text class="arrow">›</text></view></view>
		<button class="logout">退出登录</button>
	</view>
</template>

<script>
	export default { data() { return { items: [{ icon: '⚙', label: '设备管理' }, { icon: 'i', label: '关于我们' }, { icon: '?', label: '使用帮助' }, { icon: '✉', label: '意见反馈' }, { icon: '⚙', label: '设置' }] } } }
</script>

<style>
	.page { min-height: 100vh; padding: 52rpx 32rpx; background: #080b1b; color: #f8f6ff; box-sizing: border-box; }
	.profile { display: flex; align-items: center; gap: 24rpx; margin: 34rpx 8rpx 42rpx; }
	.avatar { display: flex; align-items: center; justify-content: center; width: 112rpx; height: 112rpx; border: 3rpx solid #b96cff; border-radius: 50%; color: #fff; font-size: 44rpx; font-weight: 700; background: radial-gradient(circle at 35% 30%, #c99bff, #55409c 55%, #16142d); box-shadow: 0 0 24rpx #a766ff88; }
	.name, .id { display: block; } .name { font-size: 32rpx; font-weight: 700; } .id { margin-top: 10rpx; color: #9a96b2; font-size: 23rpx; }
	.menu { overflow: hidden; border: 1rpx solid #292743; border-radius: 18rpx; background: #12152a; }
	.item { display: flex; align-items: center; min-height: 92rpx; padding: 0 22rpx; border-bottom: 1rpx solid #292743; font-size: 27rpx; } .item:last-child { border: 0; }
	.icon { display: flex; align-items: center; justify-content: center; width: 34rpx; height: 34rpx; margin-right: 22rpx; border: 1rpx solid #aaa6c4; border-radius: 50%; color: #d9d3f3; font-size: 22rpx; } .arrow { margin-left: auto; color: #aaa6c4; font-size: 44rpx; font-weight: 200; }
	.logout { width: 100%; margin-top: 56rpx; border: 1rpx solid #ec5482; border-radius: 18rpx; color: #ff799e; font-size: 28rpx; background: transparent; } .logout::after { border: 0; }
</style>
