<template>
	<!-- 整体布局 -->
	<view class="wrap">
		<!-- 设备区域 -->
		<view class="dev-area">

			<!-- 设备卡片 - 温度 -->
			<view class="dev-cart">
				<view>
					<view class="dev-name">温度</view>
					<!-- 温度图标 -->
					<image class="dev-logo" src="../../static/temp.png" mode=""></image>
				</view>
				<!-- 温度数据显示 -->
				<view class="dev-data">{{temperature}} ℃</view>
			</view>

			<!-- 设备卡片 - 湿度 -->
			<view class="dev-cart">
				<view>
					<view class="dev-name">湿度</view>
					<!-- 湿度图标 -->
					<image class="dev-logo" src="../../static/humi.png" mode=""></image>
				</view>
				<!-- 湿度数据显示 -->
				<view class="dev-data">{{humi}} %</view>
			</view>

		</view>

		<!-- 趋势图区域 -->
		<view class="chart-area">
			<view class="chart-title">温度湿度趋势</view>
			<view class="chart-container">
				<canvas canvas-id="trendChart" style="width:100%;height:300px;"></canvas>
			</view>
		</view>
		
		<!-- 运动控制区域 -->
		<view class="move-controls">
			<view class="move-buttons">
				<button class="move-button" @click="move(5)">语音报警</button>
				<button class="move-button" :class="{'buzzer-on': buzzer}" @click="toggleBuzzer">蜂鸣器报警</button>
			</view>

		</view>
	</view>
</template>

<script>
	// 引入字符串处理函数
	import {
		stringify
	} from 'querystring';
	// 引入创建通用令牌函数
	const {
		createCommonToken
	} = require('@/key.js')


	// 产品ID和设备名称要替换成自己的
	const product_id = 'dU5jVg1L9b';
	const device_name = 'test';

	// Vue组件导出
	export default {
		// 数据部分
		data() {
			return {
				// 温度、湿度状态
				temperature: '--',
				humi: '--',
				// 接口请求token
				token: '',
				// 湿度和温度的阈值
				humi_th: 70,
				temp_th: 28,
				// 控制状态
				led: false,
				buzzer: false,
				Car_flag: null,
				currentAction: '待命',
				key_th: {},
				// 数据更新定时器
				dataTimer: null,
				// 图表相关
				chart: null,
				chartData: {
					categories: [],
					series: [
						{
							name: '温度',
							data: []
						},
						{
							name: '湿度',
							data: []
						}
					]
				}
			}
		},

		// 页面加载时执行的钩子函数
		onLoad() {
			// 初始化token
			const params = {
				author_key: 'MjZlMmMzNGVmNWI4NDBjYzg4ZDU4OTgyZTdkYjY0ODk=', //用户级秘钥
				version: '2022-05-01',
				user_id: '486732', //用户ID
			}
			this.token = createCommonToken(params);
			// 初始化图表
			this.initChart();
		},

		// 页面显示时执行的钩子函数
		onShow() {
			this.startDataUpdate();
		},

		// 页面隐藏时执行的钩子函数
		onHide() {
			this.stopDataUpdate();
		},

		// 页面卸载时执行的钩子函数
		onUnload() {
			this.stopDataUpdate();
		},

		// 方法部分
		methods: {
			// 初始化图表
			initChart() {
				// 图表初始化已在updateChart中处理
			},

			// 更新图表数据
			updateChart() {
				const ctx = uni.createCanvasContext('trendChart');
				const canvasWidth = uni.upx2px(750);
				const canvasHeight = uni.upx2px(300);

				// 清空画布
				ctx.clearRect(0, 0, canvasWidth, canvasHeight);

				// 绘制背景
				ctx.fillStyle = '#ffffff';
				ctx.fillRect(0, 0, canvasWidth, canvasHeight);

				// 绘制标题
				ctx.fillStyle = '#6d6d6d';
				ctx.font = '16px Arial';
				ctx.textAlign = 'center';
				ctx.fillText('温度湿度趋势', canvasWidth / 2, 20);

				// 绘制坐标轴
				ctx.strokeStyle = '#cccccc';
				ctx.lineWidth = 1;

				// X轴
				ctx.beginPath();
				ctx.moveTo(50, canvasHeight - 50);
				ctx.lineTo(canvasWidth - 50, canvasHeight - 50);
				ctx.stroke();

				// Y轴（温度）
				ctx.beginPath();
				ctx.moveTo(50, 30);
				ctx.lineTo(50, canvasHeight - 50);
				ctx.stroke();

				// Y轴（湿度）
				ctx.beginPath();
				ctx.moveTo(canvasWidth - 50, 30);
				ctx.lineTo(canvasWidth - 50, canvasHeight - 50);
				ctx.stroke();

				// 绘制数据点和线条
				if (this.chartData.series[0].data.length > 0) {
					// 温度数据 - 先绘制线条
					ctx.strokeStyle = '#ff6b6b';
					ctx.lineWidth = 2;

					ctx.beginPath();
					const tempData = this.chartData.series[0].data;
					const tempMax = 50;
					const tempMin = 0;
					const tempRange = tempMax - tempMin;

					for (let i = 0; i < tempData.length; i++) {
						const x = 50 + (canvasWidth - 100) * (i / (tempData.length - 1));
						const y = canvasHeight - 50 - (tempData[i] - tempMin) / tempRange * (canvasHeight - 80);

						if (i === 0) {
							ctx.moveTo(x, y);
						} else {
							ctx.lineTo(x, y);
						}
					}
					ctx.stroke();

					// 温度数据 - 再绘制数据点
					ctx.fillStyle = '#ff6b6b';
					for (let i = 0; i < tempData.length; i++) {
						const x = 50 + (canvasWidth - 100) * (i / (tempData.length - 1));
						const y = canvasHeight - 50 - (tempData[i] - tempMin) / tempRange * (canvasHeight - 80);

						ctx.beginPath();
						ctx.arc(x, y, 3, 0, 2 * Math.PI);
						ctx.fill();
					}

					// 湿度数据 - 先绘制线条
					ctx.strokeStyle = '#4ecdc4';
					ctx.lineWidth = 2;

					ctx.beginPath();
					const humiData = this.chartData.series[1].data;
					const humiMax = 100;
					const humiMin = 0;
					const humiRange = humiMax - humiMin;

					for (let i = 0; i < humiData.length; i++) {
						const x = 50 + (canvasWidth - 100) * (i / (humiData.length - 1));
						const y = canvasHeight - 50 - (humiData[i] - humiMin) / humiRange * (canvasHeight - 80);

						if (i === 0) {
							ctx.moveTo(x, y);
						} else {
							ctx.lineTo(x, y);
						}
					}
					ctx.stroke();

					// 湿度数据 - 再绘制数据点
					ctx.fillStyle = '#4ecdc4';
					for (let i = 0; i < humiData.length; i++) {
						const x = 50 + (canvasWidth - 100) * (i / (humiData.length - 1));
						const y = canvasHeight - 50 - (humiData[i] - humiMin) / humiRange * (canvasHeight - 80);

						ctx.beginPath();
						ctx.arc(x, y, 3, 0, 2 * Math.PI);
						ctx.fill();
					}
				}

				// 绘制图例
				ctx.font = '12px Arial';
				ctx.textAlign = 'left';

				// 温度图例
				ctx.fillStyle = '#ff6b6b';
				ctx.fillRect(100, 30, 10, 10);
				ctx.fillStyle = '#6d6d6d';
				ctx.fillText('温度(℃)', 120, 40);

				// 湿度图例
				ctx.fillStyle = '#4ecdc4';
				ctx.fillRect(200, 30, 10, 10);
				ctx.fillStyle = '#6d6d6d';
				ctx.fillText('湿度(%)', 220, 40);

				// 绘制Y轴标签
				ctx.font = '10px Arial';
				ctx.textAlign = 'right';

				// 温度Y轴标签
				for (let i = 0; i <= 5; i++) {
					const y = 30 + i * (canvasHeight - 80) / 5;
					const value = 50 - i * 10;
					ctx.fillText(value, 45, y + 4);
				}

				// 湿度Y轴标签
				ctx.textAlign = 'left';
				for (let i = 0; i <= 5; i++) {
					const y = 30 + i * (canvasHeight - 80) / 5;
					const value = 100 - i * 20;
					ctx.fillText(value, canvasWidth - 45, y + 4);
				}

				// 绘制X轴标签
				ctx.textAlign = 'center';
				const categories = this.chartData.categories;
				for (let i = 0; i < categories.length; i++) {
					const x = 50 + (canvasWidth - 100) * (i / (categories.length - 1));
					ctx.fillText(categories[i], x, canvasHeight - 30);
				}

				// 绘制完成
				ctx.draw();
			},

			// 开始数据更新
			startDataUpdate() {
				// 首次获取设备数据
				this.fetchDevData();
				// 定时（每500ms）获取设备数据
				this.dataTimer = setInterval(() => {
					this.fetchDevData();
				}, 500);
			},

			// 停止数据更新
			stopDataUpdate() {
				if (this.dataTimer) {
					clearInterval(this.dataTimer);
					this.dataTimer = null;
				}
			},

			// 获取设备数据的方法
			fetchDevData() {
				// 发送请求获取设备属性
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/query-device-property',
					method: 'GET',
					data: {
						product_id: product_id,
						device_name: device_name,
					},
					header: {
						'authorization': this.token
					},
					success: (res) => {
						console.log('设备数据:', res.data);

						// 使用forEach和switch解析所有数据，包括控制参数
						if (res.data.data && Array.isArray(res.data.data)) {
							res.data.data.forEach(item => {
								switch (item.identifier) {
									case 'temperature':
										this.temperature = item.value;
										break;
									case 'humidity':
										this.humi = item.value;
										break;
								}
							});

							// 更新图表数据
							this.updateChartData();
						}


					},
				});
			},

			// 更新图表数据
			updateChartData() {
				// 获取当前时间
				const now = new Date();
				const time = now.getHours() + ':' + (now.getMinutes() < 10 ? '0' + now.getMinutes() : now.getMinutes()) + ':' + (now.getSeconds() < 10 ? '0' + now.getSeconds() : now.getSeconds());

				// 添加时间标签
				this.chartData.categories.push(time);
				// 限制显示的数据点数量
				if (this.chartData.categories.length > 10) {
					this.chartData.categories.shift();
				}

				// 添加温度数据
				if (this.temperature !== '--') {
					this.chartData.series[0].data.push(this.temperature);
					if (this.chartData.series[0].data.length > 10) {
						this.chartData.series[0].data.shift();
					}
				}

				// 添加湿度数据
				if (this.humi !== '--') {
					this.chartData.series[1].data.push(this.humi);
					if (this.chartData.series[1].data.length > 10) {
						this.chartData.series[1].data.shift();
					}
				}

				// 更新图表
				this.updateChart();
			},

			// 滑动条变化事件的方法
			sliderChange(e, id) {
				console.log(id)
				console.log('value 发生变化：' + e.detail.value)

				// 设置对应滑动条的操作标志
				if (id == 'slider1') {

					this.temp_th = e.detail.value;
					this.key_th = {
						temp_th: this.temp_th,
					};
				} else if (id == 'slider2') {

					this.humi_th = e.detail.value;
					this.key_th = {
						humi_th: this.humi_th,
					};
				}

				// 向后端发送设备属性更新请求
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
					method: 'POST',
					data: {
						product_id: product_id,
						device_name: device_name,
						params: this.key_th
					},
					header: {
						'authorization': this.token
					},
				});
			},

			// 消毒开关切换的方法
			onLedSwitch(event) {
				// 正确获取开关值并打印
				let value = event.detail.value;


				this.led = value; // 立即更新本地状态

				// 发送请求更新设备属性
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
					method: 'POST',
					data: {
						product_id: product_id,
						device_name: device_name,
						params: {
							"led": value
						}
					},
					header: {
						'authorization': this.token
					}

				});
			},
			// 运动控制
			move(action) {
				const actions = ['前进', '左转', '停止', '右转', '后退', '避障模式', '循迹模式'];
				this.currentAction = actions[action];
				this.Car_flag = action;

				this.uploadCarFlag();
			},

			// 上传Car_flag
			uploadCarFlag() {
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
					method: 'POST',
					data: {
						product_id: product_id,
						device_name: device_name,
						params: {
							"Car_flag": this.Car_flag
						}
					},
					header: {
						'authorization': this.token
					},
				});
			},

			toggleBuzzer() {
				this.buzzer = !this.buzzer;
				uni.request({
					url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
					method: 'POST',
					data: {
						product_id: product_id,
						device_name: device_name,
						params: {
							"SET1": this.buzzer ? 1 : 0
						}
					},
					header: {
						'authorization': this.token
					},
				});
			}
		}
	}
</script>


<style>
	/* 整体页面容器样式 */
	.wrap {
		padding: 30rpx;
		background-color: #4a90d9;
		min-height: 100vh;
	}

	/* 设备区域样式 */
	.dev-area {
		display: flex;
		/* 使用弹性盒子布局 */
		justify-content: space-between;
		/* 在弹性容器中均匀分布子元素，两端对齐 */
		flex-wrap: wrap;
		/* 如果子元素溢出容器，则折叠到下一行 */
	}

	/* 设备卡片样式 */
	.dev-cart {
		height: 150rpx;
		/* 设置高度为150像素 */
		width: 320rpx;
		/* 设置宽度为320像素 */
		border-radius: 30rpx;
		/* 设置边框圆角为30像素 */
		margin-top: 30rpx;
		/* 设置上外边距为30像素 */
		display: flex;
		/* 使用弹性盒子布局 */
		justify-content: space-around;
		/* 在弹性容器中均匀分布子元素，两端对齐 */
		align-items: center;
		/* 在弹性容器中垂直居中对齐子元素 */
		box-shadow: 0 0 15rpx #ccc;
		/* 设置盒子阴影，颜色为灰色 */
	}

	/* 长设备卡片样式 */
	.device-cart-l {
		height: 150rpx;
		/* 设置高度为150像素 */
		width: 700rpx;
		/* 设置宽度为700像素 */
		border-radius: 30rpx;
		/* 设置边框圆角为30像素 */
		margin-top: 30rpx;
		/* 设置上外边距为30像素 */
		display: flex;
		/* 使用弹性盒子布局 */
		justify-content: space-around;
		/* 在弹性容器中均匀分布子元素，两端对齐 */
		align-items: center;
		/* 在弹性容器中垂直居中对齐子元素 */
		box-shadow: 0 0 15rpx #ccc;
		/* 设置盒子阴影，颜色为灰色 */
	}

	/* 设备名称样式 */
	.dev-name {
		font-size: 20rpx;
		/* 设置字体大小为20像素 */
		text-align: center;
		/* 文本居中对齐 */
		color: #6d6d6d;
		/* 字体颜色为灰色 */
	}

	/* 设备图标样式 */
	.dev-logo {
		width: 70rpx;
		/* 设置宽度为70像素 */
		height: 70rpx;
		/* 设置高度为70像素 */
		margin-top: 10rpx;
		/* 设置上外边距为10像素 */
	}

	/* 设备数据样式 */
	.dev-data {
		font-size: 50rpx;
		/* 设置字体大小为50像素 */
		color: #6d6d6d;
		/* 字体颜色为灰色 */
	}

	/* 滑动条样式 */
	.ctrl-slider {
		width: 580rpx;
		/* 设置宽度为580像素 */
	}

	/* 运动控制区域样式 */
	.move-controls {
		margin-top: 50rpx;
		display: flex;
		flex-direction: column;
		align-items: center;
	}

	.move-buttons {
		display: flex;
		justify-content: center;
		width: 100%;
	}

	.move-button,
	.turn-button {
		width: 210rpx;
		height: 110rpx;
		margin: 10rpx;
		font-size: 20rpx;
		background-color: #4CAF50;
		color: #fff;
	}

	.buzzer-on {
		background-color: #f44336 !important;
	}

	.turning-buttons {
		display: flex;
		justify-content: space-between;
		width: 100%;
		margin-top: 10rpx;
	}

	.current-action {
		margin-top: 20rpx;
		font-size: 18rpx;
		color: #333;
		width: 100%;
		text-align: center;
	}

	/* 图表区域样式 */
	.chart-area {
		margin-top: 30rpx;
		background-color: #ffffff;
		border-radius: 30rpx;
		padding: 20rpx;
		box-shadow: 0 0 15rpx #ccc;
	}

	/* 图表标题样式 */
	.chart-title {
		font-size: 24rpx;
		color: #6d6d6d;
		text-align: center;
		margin-bottom: 20rpx;
	}

	/* 图表容器样式 */
	.chart-container {
		width: 100%;
		height: 300px;
	}
</style>