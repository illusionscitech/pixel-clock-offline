# pixel-clock-offline

<b>像素时钟离线版本</b>
<b>AWTRIX2.0-Controller-offline</b>

✔️项目程序文件源于https://github.com/yinbaiyuan/AWTRIX2.0-Controller基于AWTRIX2.0版本基础上做了离线版修改

本次在原版基础上做了如下修改：
1. 增加SHT30温湿度传感器，由显示温度湿度到添加了气压动态显示。
2. 添加了光照传感器多阶梯亮度的显示，即使黑暗环境也可以处于最低亮度显示状态
3. 增加天气图标显示，目前可以显示“晴”，“阴”，“多云”，“雨”，“雪”，“雾”等
4. 增加了开机轻音乐，（不选删除储存卡对应文件）
5. 离线模式添加了两种动画
6.时间日期显示由单一白色，变为随机颜色显示，增强氛围感

设计的第一版pcb文件在文件夹中，本版本MCU使用wemos mini（主控esp8266）
外围模块包括RTC时钟模块，DFPLAYER音乐播放模块，SHT30温湿度模块，bme280模块等，所有模块使用可拆卸设计，

外壳使用积木设计，光栅使用雪糕杆（不建议太费力），面板为茶色亚克力（不过建议使用黑茶色，看起来更有质感）

原版配置：
1.修改B站UID
打开 B 站自己的频道，就可以在 URL 中看到自己频道的 ID
2.修改Youtube频道ID
可以在自己频道的 URL 地址内得到，CHANNEL 后面是频道 ID
3.修改Youtube APIKEY
youtube data api v3 的秘钥，需要登录谷歌申请。可以参考 https://zhuanlan.zhihu.com/p/96096543 获取
4.和风天气创建 APIKEY 方法
https://dev.heweather.com/docs/start/get-api-key
5.和风天气获取城市编码查询
https://github.com/heweather/LocationList/blob/master/China-City-List-latest.csv


