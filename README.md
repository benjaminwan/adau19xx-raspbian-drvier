# ADAU1977 Raspbain Driver
支持Raspbian内核版本4.14

## 截图
![image](https://github.com/benjaminwan/adau19xx-raspbian-drvier/raw/master/screenshots/RecordTest.png)

![image](https://github.com/benjaminwan/adau19xx-raspbian-drvier/raw/master/screenshots/AudioSettings.png)

![image](https://github.com/benjaminwan/adau19xx-raspbian-drvier/raw/master/screenshots/AudioControls.png)

## 准备环境
安装编译工具、依赖文件等  
```
sudo apt update
sudo apt-get -y install raspberrypi-kernel-headers raspberrypi-kernel
sudo apt-get -y install dkms
```

## 编译dts文件
编译并安装  
```
cd dts
sudo ./install-dtbo.sh
```
从文件管理器确认/boot/overlays里有如下文件  
adau19xx-2ch.dtbo  

## 编译ADAU19xx驱动
编译并安装  
```
cd driver
sudo ./install-adau19xx-2ch.sh
```

如何关闭调试信息：  
adau19xx.h中注释此行并重新编译安装  
```
//#define CONFIG_ADAU19XX_DEBUG
```

## 修改boot/config.txt
sudo pcmanfm #打开root权限的文件管理器  
在新弹出的文件管理器窗口打开/boot/config.txt  
确保有如下内容  
```
# Enable audio
dtparam=audio=on

# ADAU19XX
dtoverlay=adau19xx-2ch
```

然后重启以生效  

## 驱动寄存器调试
配置调试串口来获取调试信息(推荐)  
或者从/proc/kmsg中cat内核调试信息  

调试命令格式:echo flag|reg|val > adau  
```
su
cd /sys/class/i2c-adapter/i2c-1/1-0071/adau19xx_debug
echo 1a > adau //获取所有寄存器值
echo 10001 > adau //把值0x01写入寄存器0x00
echo 10080 > adau //ADAU19XX_POWER_RESET
echo 10001 > adau //ADAU19XX_POWER_PWUP
```

## 测试工具
```
apt-get -y install audacity
arecord -D hw:1,0 -f S32_LE -r 44100 -c 2 44.1k_S32LE.wav
```

## 卸载驱动
修改boot/config.txt去除相关内容  
```
sudo ./uninstall-dtbo.sh //卸载dtbo文件
sudo ./uninstall-kadau19xx-2ch.sh //卸载驱动ko文件
```

## I2C与I2S连接说明
此驱动以树莓派做为主控，芯片ADAU19xx工作在从机模式(I2C和I2S
均是如此)  

| 引脚定义 | ADAU1977引脚 | 树莓派引脚 |
| :-: | :-: | :-: |
| SDA(I2C) | 17 | 03 |
| SLC(I2C) | 18 | 05 |
| BCLK(I2S) | 16 | 12 |
| LRCLK(I2S) | 15 | 35 |
| SDATAOUT(I2S) | 13 | 38 |
| GND | GND | 39 |
| GPIO5 | RESET | 29(可选) |
硬复位功能不是必须，可以不接  

## 音频关于采样参考源的选择
可以从MCLK分频也可以取树莓派的LRCLK。  
ADI范例使用的有源晶振是12.288MHz的，驱动默认也是使用此频率。  
根据芯片手册： 
 
* MCLK模式支持的采样率：8K,16K,32K,64K,128K,12K,24K,48K,96K,192K  
* MCLK模式不支持的采样率：11.025K,44.1K,88.2K,172.4K  

* LRCLK模式支持32K以上的采样率，包括32K,64K,128K,44.1K,88.2K,172.4K,48K,96K,192K  
* LRCLK模式不支持的采样率：8K,16K,11.025K,22.05K,12K,24K  

驱动默认选择的方案是LRCLK模式  
根据你的实际硬件来配置I2C地址和采样参考源：  
打开adau19xx-2ch-overlay.dts  
```
adau_codec: adau1977@71{
		compatible = "adi,adau19xx";
		reg = <0x71>; //配置I2C地址
		reset-gpios = <&gpio 5 0>; //硬复位，可选
		#sound-dai-cells = <0>;
		sysclk-src = <1>;//0=SYSCLK_SRC_MCLK 1=SYSCLK_SRC_LRCLK
};
```

## ALSA音频驱动设置项说明
打开树莓派系统的开始菜单，选择Preferences -> Audio Device Settings  
Sound card:选中krs-adau-card(Alsa mixer)  
左下角点开Select Controls，并选中全部复选框，按Close  
出现3个页签  

### Playback(滑动条)
说明：用于增益控制  
POST ADC1 gain:对应第1声道增益  
POST ADC2 gain:对应第2声道增益  
以此类推  

### Switches(开关)
ADCx DC Subtraction:  
默认：不钩  
说明：校准寄存器，扣除通道x校准产生的直流值  
选项：不钩=无直流扣除 钩中=扣除直流校准产生的直流值  

ADCX Highpass-Filter:  
默认：不钩  
说明：通道x数字直流高通滤波器  
选项：不钩=关闭 钩中=开启  

### Options(选项)
Micbias Enable:  
默认：Off  
说明：麦克风偏置使能  
选项：On=启用 Off=关闭  

Micbias Voltage:  
默认：默认8.5V  
说明：MICBIAS输出电压  
选项：5.0V~9.0V  

Boost Enable:  
默认：Off  
说明：升压转换器使能  
选项：On=启用 Off=关闭  

Boost Over Current Protect:  
默认：Enable  
说明：升压转换器过流故障保护  
选项：Enable=启用 Disable=禁用  

Boost Over Voltage Protect:  
默认：Enable  
说明：升压转换器过压故障保护  
选项：Enable=启用 Disable=禁用  

Boost Recovery Mode:  
默认：AutoMatic  
说明：升压故障恢复模式  
选项：AutoMatic=自动恢复 Manual=手动恢复  

Boost Sample Rate:  
默认：12k  
说明：升压转换器开关频率的采样速率控制  
选项：8k或倍数、11.025k或倍数、12k或倍数  

Boost Switch Freq:  
默认：1.5MHz  
说明：升压转换器开关频率  
选项：1.5MHz，3.0MHz  

Chx Diagnostics:  
默认：Disable  
说明：通道x诊断使能  
选项：Enable=启用 Disable=禁用  

Chx Drive:  
默认：On  
说明：通道x串行输出驱动使能  
选项：On=启用 Off=关闭  

Sum Mode:  
默认：2ch  
说明：通道求和模式  
选项：4ch=4通道独立工作 2ch=1和2通道求和 3和4通道求和 1ch=4个通道求和  

Unused Outputs Status:  
默认：Low  
说明：让不用的SAI通道处于三台还是积极驱动这些数据时隙  
选项：Low=不用的输出驱动到低点评 Hi-Z:不用的输出处于高阻态  

## 如何查看并配置增益项
ADAU1977芯片提供了4个通道独立的数字增益调节寄存器。  
树莓派因为硬件上的寄存器位数限制，无法支持4声道，最多只能支持2个声道。  
Alsa提供了可视化带数值的增益配置界面  
在终端中执行sudo alsamixer  
按F6并用上下键切换声卡adau19xx-card  
按F4切换到Capture设备
再用左右键移动，即可找到POST ADC 4个调节条  

![image](https://github.com/benjaminwan/adau19xx-raspbian-drvier/raw/master/screenshots/AlsaMixer.png)

## 音量与增益调节范围
### 增益范围
-35.625dB~60dB，增益步长：0.375dB，共有(35.625+60)/0.375=255个增益值

### 增益与底噪
个人测试当音量调节大于50时，即增益12.375dB，开始有明显底噪，各个声道或有些许差异。  

### 音量与增益关系表
操作系统的音量调节条范围是0~100，所以这边有一个换算关系。  
如下表，系统音量37对应0dB，有几个值因为无法除尽所以滑动音量条时系统会跳过  

| 系统音量(%) | 增益值(dB) | 寄存器值 |
| :-: | :-: | :-: |
| 0 | -35.625 | 255 |
| 1 | -34.875 | 253 |
| 2 | -33.75 | 250 |
| 3 | -32.625 | 247 |
| 4 | -31.5 | 244 |
| 5 | -30.375 | 241 |
| 6 |  |  |
| 7 | -29.25 | 238 |
| 8 | -28.125 | 235 |
| 9 | -27 | 232 |
| 10 | -25.875 | 229 |
| 11 | -24.75 | 226 |
| 12 |  |  |
| 13 | -23.625 | 223 |
| 14 | -22.5 | 220 |
| 15 | -21.375 | 217 |
| 16 | -20.25 | 214 |
| 17 | -19.125 | 211 |
| 18 | -18 | 208 |
| 19 |  |  |
| 20 | -16.875 | 205 |
| 21 | -15.75 | 202 |
| 22 | -14.625 | 199 |
| 23 | -13.5 | 196 |
| 24 | -12.375 | 193 |
| 25 | -11.25 | 190 |
| 26 |  |  |
| 27 | -10.125 | 187 |
| 28 | -9 | 184 |
| 29 | -7.875 | 181 |
| 30 | -6.75 | 178 |
| 31 | -5.625 | 175 |
| 32 |  |  |
| 33 | -4.5 | 172 |
| 34 | -3.375 | 169 |
| 35 | -2.25 | 166 |
| 36 | -1.125 | 163 |
| 37 | 0 | 160 |
| 38 | -1.125 | 157 |
| 39 |  |  |
| 40 | 2.25 | 154 |
| 41 | 3.375 | 151 |
| 42 | 4.5 | 148 |
| 43 | 5.625 | 145 |
| 44 | 6.75 | 142 |
| 45 | 7.875 | 139 |
| 46 |  |  |
| 47 | 9 | 136 |
| 48 | 10.125 | 133 |
| 49 | 11.25 | 130 |
| 50 | 12.375 | 127 |
| 51 | 13.5 | 124 |
| 52 |  |  |
| 53 | 14.625 | 121 |
| 54 | 15.75 | 118 |
| 55 | 16.875 | 115 |
| 56 | 18 | 112 |
| 57 | 19.125 | 109 |
| 58 | 20.25 | 106 |
| 59 |  |  |
| 60 | 21.375 | 103 |
| 61 | 22.5 | 100 |
| 62 | 23.625 | 97 |
| 63 | 24.75 | 94 |
| 64 | 25.875 | 91 |
| 65 | 27 | 88 |
| 66 |  |  |
| 67 | 28.125 | 85 |
| 68 | 29.25 | 82 |
| 69 | 30.375 | 79 |
| 70 | 31.5 | 76 |
| 71 | 32.625 | 73 |
| 72 |  |  |
| 73 | 33.75 | 70 |
| 74 | 34.875 | 67 |
| 75 | 36 | 64 |
| 76 | 37.125 | 61 |
| 77 | 38.25 | 58 |
| 78 | 39.375 | 55 |
| 79 |  |  |
| 80 | 40.5 | 52 |
| 81 | 41.625 | 49 |
| 82 | 42.75 | 46 |
| 83 | 43.875 | 43 |
| 84 | 45 | 40 |
| 85 | 46.125 | 37 |
| 86 |  |  |
| 87 | 47.25 | 34 |
| 88 | 48.375 | 31 |
| 89 | 49.5 | 28 |
| 90 | 50.625 | 25 |
| 91 | 51.75 | 22 |
| 92 |  |  |
| 93 | 52.875 | 19 |
| 94 | 54 | 16 |
| 95 | 55.125 | 13 |
| 96 | 56.25 | 10 |
| 97 | 57.375 | 7 |
| 98 | 58.5 | 4 |
| 99 |  |  |
| 100 | 59.625 | 1 |

## 关于PCM编码格式的支持测试。
缩写定义：S是有符号 U是无符号 BE是大端 LE是小端 
ADAU19xx官方参考驱动支持16位、24位、32位数据宽度。  
从官方提供的参考驱动代码里可以明确知道，芯片支持S16_LE、S24_LE、S32_LE 三种采样精度。  
但是实测采用S24_LE参数录音时，结果都是噪音，所以已在驱动中屏蔽了24位采样精度支持。  
举例：在I2S总线中，树莓派作为主设备，ADAU1977作为从设备。  
树莓派输出BCLK和LRCLK，并从ADAU1977接收SDATA。  
当模式设置为48K_S16时，LRCLK频率就是48K，LRCLK高低电平对应左声道和右声道，1个LRCLK周期对应32个BCLK周期，即左声道16个bit数据位，右声道16个bit数据位。  

![image](https://github.com/benjaminwan/adau19xx-raspbian-drvier/raw/master/screenshots/48K_S16_LE.png)  

当模式设置为48K_S32时，1个LRCLK周期对应64个BCLK周期，即左声道32个bit数据位，右声道32个bit数据位。  

![image](https://github.com/benjaminwan/adau19xx-raspbian-drvier/raw/master/screenshots/48K_S32LE.png)  
