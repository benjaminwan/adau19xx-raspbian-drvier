#ifndef _ADAU19XX_H
#define _ADAU19XX_H
#define CONFIG_ADAU19XX_DEBUG

#include <linux/regmap.h>

enum adau19xx_type {
    ADAU1977,
    ADAU1978,
    ADAU1979,
};

enum adau19xx_clk_id {
    ADAU19XX_SYSCLK,
};

enum adau19xx_sysclk_src {
    ADAU19XX_SYSCLK_SRC_MCLK,
    ADAU19XX_SYSCLK_SRC_LRCLK,
};

struct adau1977 {
    struct regmap *regmap;
    bool right_j;
    unsigned int sysclk;
    enum adau19xx_sysclk_src sysclk_src;
    struct gpio_desc *reset_gpio;
    enum adau19xx_type type;

    struct snd_pcm_hw_constraint_list constraints;

    struct device *dev;

    unsigned int max_master_fs;
    unsigned int slot_width;
    bool enabled;
    bool master;
};
extern void adau19xx_print_msg(u8 reg, int ret, int value);
extern void adau19xx_do_store(struct device *dev, const char *buf, size_t count);
extern void adau19xx_do_show(void);
extern int adau19xx_probe(struct i2c_client *i2c, struct regmap *regmap, enum adau19xx_type type);

#define ADAU19XX_CHANNELS_MAX  2  //range[1, 4],but we run in sum mode 2
#define ADAU19XX_RATES    SNDRV_PCM_RATE_KNOT
#define ADAU19XX_FORMATS   (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE)

//寄存器汇总
#define ADAU19XX_REG_POWER  0x00 //主电源和软件复位寄存器
#define ADAU19XX_REG_PLL  0x01//PLL控制寄存器
#define ADAU19XX_REG_BOOST  0x02//DC-DC升压转换器控制寄存器
#define ADAU19XX_REG_MICBIAS  0x03//MICBIAS和升压控制寄存器
#define ADAU19XX_REG_BLOCK_POWER_SAI 0x04//模块电源控制和串行端口控制寄存器
#define ADAU19XX_REG_SAI_CTRL0  0x05//串行端口控制寄存器1
#define ADAU19XX_REG_SAI_CTRL1  0x06//串行端口控制寄存器2
#define ADAU19XX_REG_CMAP12  0x07//输出串行端口通道映射寄存器
#define ADAU19XX_REG_CMAP34  0x08//输出串行端口通道映射寄存器
#define ADAU19XX_REG_SAI_OVERTEMP 0x09//串行输出驱动和过温保护控制寄存器
#define ADAU19XX_REG_POST_ADC_GAIN(x) (0x0a + (x))//后置ADC增益通道x控制寄存器
#define ADAU19XX_REG_MISC_CONTROL 0x0e//高通滤波器和直流失调控制寄存器以及主静音
#define ADAU19XX_REG_DIAG_CONTROL 0x10//诊断控制寄存器
#define ADAU19XX_REG_STATUS(x)  (0x11 + (x))//诊断报告寄存器通道x
#define ADAU19XX_REG_DIAG_IRQ1  0x15//诊断中断引脚控制寄存器1
#define ADAU19XX_REG_DIAG_IRQ2  0x16//诊断中断引脚控制寄存器2
#define ADAU19XX_REG_ADJUST1  0x17//诊断调整寄存器1
#define ADAU19XX_REG_ADJUST2  0x18//诊断调整寄存器2
#define ADAU19XX_REG_ADC_CLIP  0x19//ADC削波状态寄存器
#define ADAU19XX_REG_DC_HPF_CAL  0x1a//数字直流高通滤波器和校准寄存器

//other in tool
#define ADAU19XX_REG_ADC_BIAS_CONTROL 0x0f //未知
#define ADAU19XX_REG_TWEAK1 0x1b//升压转换器电压控制
#define ADAU19XX_REG_ADC_TWEAK 0x2c//未知

//0x00 主电源和软件复位寄存器
#define ADAU19XX_POWER_RESET   BIT(7)//软件复位 0=正常工作 1=软件复位
#define ADAU19XX_POWER_PWUP   BIT(0)//主机上电控制 0=完全关断 1=主机上电

//0x01 PLL控制寄存器
#define ADAU19XX_PLL_CLK_S   BIT(4)//PLL时钟源选择 0=MCLK用于PLL输入 1=LRCLK用于PLL输入,仅支持大于32kHz的采样率
#define ADAU19XX_PLL_MCS_MASK   0x7//主时钟选择,MCS位决定PLL的倍频系数,必须根据输入MCLK频率和采样速率设置.
//001=256x Fs MCLK (32kHz至48kHz)
//010=384x Fs MCLK (32kHz至48kHz)
//011=512x Fs MCLK (32kHz至48kHz)
//100=768x Fs MCLK (32kHz至48kHz)
//000=128x Fs MCLK (32kHz至48kHz)

//0x02 DC-DC升压转换器控制
//升压转换器为麦克风偏置电驴产生一个电源电压.
//使用来自PLL的时钟,并且开关频率取决于ADC的采样速率
#define ADAU19XX_MICBIAS_FS_RATE_MASK  (0x03 << 5)//升压开关频率的采样速率控制
//00=8k/16k/32k/64k/128k
//01=11.025k/22.025k/44.1k/88.2k/176.4k
//10=12k/24k/48k/96k/192k

//开关频率可选1.5MHz或3MHz,1.5MHz对应电感值4.7uH,3MHz对应电感值2.2uH
#define ADAU19XX_MICBIAS_BOOST_SW_FREQ BIT(4)//升压调节器开关频率

#define ADAU19XX_MICBIAS_OV_EN BIT(3)//过压故障保护 0=disable 1=enable

#define ADAU19XX_MICBIAS_OC_EN BIT(1)//过流故障保护 0=disable 1=enable

//0x04 模块电源控制和串行端口控制寄存器
#define ADAU19XX_BLOCK_POWER_SAI_LR_POL  BIT(7)//设置LRCLK极性 0=LRCLK先低后高 1=LRCLK先高后低
#define ADAU19XX_BLOCK_POWER_SAI_BCLK_EDGE BIT(6)//设置数据改变的位时钟边沿 0=数据在下降沿改变 1=数据在上升沿改变
#define ADAU19XX_BLOCK_POWER_SAI_LDO_EN  BIT(5)//LDO调机器使能 0=LDO关断 1=LDO使能

//0x05 串行端口控制寄存器1
#define ADAU19XX_SAI_CTRL0_FMT_MASK  (0x3 << 6)//串行数据格式
#define ADAU19XX_SAI_CTRL0_FMT_I2S  (0x0 << 6)//00=I2S数据相对于LRCLK边沿延迟1BCLK
#define ADAU19XX_SAI_CTRL0_FMT_LJ  (0x1 << 6)//左对齐
#define ADAU19XX_SAI_CTRL0_FMT_RJ_24BIT  (0x2 << 6)//右对齐,24位数据
#define ADAU19XX_SAI_CTRL0_FMT_RJ_16BIT  (0x3 << 6)//右对齐,16位数据

#define ADAU19XX_SAI_CTRL0_SAI_MASK  (0x7 << 3)//串行端口模式
#define ADAU19XX_SAI_CTRL0_SAI_I2S  (0x0 << 3)//000=立体声(I2S\LJ\RJ)
#define ADAU19XX_SAI_CTRL0_SAI_TDM_2  (0x1 << 3)//001=TDM2
#define ADAU19XX_SAI_CTRL0_SAI_TDM_4  (0x2 << 3)//010=TDM4
#define ADAU19XX_SAI_CTRL0_SAI_TDM_8  (0x3 << 3)//011=TDM8
#define ADAU19XX_SAI_CTRL0_SAI_TDM_16  (0x4 << 3)//100=TDM16

#define ADAU19XX_SAI_CTRL0_FS_MASK  (0x7)//采样速率
#define ADAU19XX_SAI_CTRL0_FS_8000_12000 (0x0)//000=8kHz至12kHz
#define ADAU19XX_SAI_CTRL0_FS_16000_24000 (0x1)//001=16kHz至24kHz
#define ADAU19XX_SAI_CTRL0_FS_32000_48000 (0x2)//010=32kHz至48kHz
#define ADAU19XX_SAI_CTRL0_FS_64000_96000 (0x3)//011=64kHz至96kHz
#define ADAU19XX_SAI_CTRL0_FS_128000_192000 (0x4)//100=128kHz至192kHz

//0x06 串行端口控制寄存器2
#define ADAU19XX_SAI_CTRL1_SLOT_WIDTH_MASK (0x3 << 5)//TDM模式下每个时隙的BCLK数
#define ADAU19XX_SAI_CTRL1_SLOT_WIDTH_32 (0x0 << 5)//00=每个TDM时隙32个BCLK
#define ADAU19XX_SAI_CTRL1_SLOT_WIDTH_24 (0x1 << 5)//01=每个TDM时隙24个BCLK
#define ADAU19XX_SAI_CTRL1_SLOT_WIDTH_16 (0x2 << 5)//10=每个TDM时隙16个BCLK
#define ADAU19XX_SAI_CTRL1_DATA_WIDTH_MASK (0x1 << 4)//输出数据位宽度
#define ADAU19XX_SAI_CTRL1_DATA_WIDTH_16BIT (0x1 << 4)//1=16位数据
#define ADAU19XX_SAI_CTRL1_DATA_WIDTH_24BIT (0x0 << 4)//0=24位数据
#define ADAU19XX_SAI_CTRL1_LRCLK_PULSE  BIT(3)//设置LRCLK模式
#define ADAU19XX_SAI_CTRL1_MSB   BIT(2)//设置数据以MSB或LSB优先方式输入/输出
#define ADAU19XX_SAI_CTRL1_BCLKRATE_MASK (0x1 << 1)//设置主模式下产生的每个数据通道的位时钟周期数
#define ADAU19XX_SAI_CTRL1_BCLKRATE_16  (0x1 << 1)//1=每通道16个BCLK
#define ADAU19XX_SAI_CTRL1_BCLKRATE_32  (0x0 << 1)//0=每通道32个BCLK
#define ADAU19XX_SAI_CTRL1_MASTER  BIT(0)//TDM4或更大模式下的SDATAOUTx引脚选择 0=SDATAOUT1用于输出 1=SDATAOUT2用于输出

//0x09 串行输出驱动和过温保护控制寄存器
#define ADAU19XX_SAI_OVERTEMP_DRV_C(x)  BIT(4 + (x))//通道x串行输出驱动使能 0=通道不在串行输出端口上驱动 1=通道在串行输出端口上驱动
#define ADAU19XX_SAI_OVERTEMP_DRV_HIZ  BIT(3)//让不用的SAI通道处于三态还是积极驱动这些数据时隙 0=不用的输出驱动到低电平 1=不用的输出处于高阻态

//0x0e MMUTE 主静音
#define ADAU19XX_MISC_CONTROL_MMUTE  BIT(4) //主静音 0=正常工作 1=所有通道静音
#define ADAU19XX_MISC_CONTROL_SUM_MODE_MASK (0x3 << 6)//实现较高SNR信噪比的通道求和模式控制
#define ADAU19XX_MISC_CONTROL_SUM_MODE_4 (0x0 << 6)//4通道正常工作
#define ADAU19XX_MISC_CONTROL_SUM_MODE_2 (0x1 << 6)//2通道求和工作
#define ADAU19XX_MISC_CONTROL_SUM_MODE_1 (0x2 << 6)//1通道求和工作

//0x07 输出串行端口通道映射寄存器
#define ADAU19XX_CHAN_MAP_SECOND_SLOT_OFFSET 4
#define ADAU19XX_CHAN_MAP_FIRST_SLOT_OFFSET 0

#define ADAU19XX_RATE_CONSTRAINT_MASK_32000 0x001f
#define ADAU19XX_RATE_CONSTRAINT_MASK_44100 0x03e0
#define ADAU19XX_RATE_CONSTRAINT_MASK_48000 0x7c00
/* All rates >= 32000 */
#define ADAU19XX_RATE_CONSTRAINT_MASK_LRCLK 0x739c

#endif

