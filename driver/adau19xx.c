#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "adau19xx.h"

static const unsigned int adau19xx_rates[] = {
    8000, 16000, 32000, 64000, 128000,
    11025, 22050, 44100, 88200, 172400,
    12000, 24000, 48000, 96000, 192000,
};

static const char *const adau19xx_sum_mode_texts[] = {
    "Normal Mode 4ch",
    "Sum Mode 2ch",
    "Sum Mode 1ch",
};

static const char *const adau19xx_boost_fs_rate_texts[] = {
    "8k|16k|32k|64k|128k",
    "11.025k|22.05k|44.1k|88.2k|176.4k",
    "12k|24k|48k|96k|192k",
};

static const char *const adau19xx_boost_sw_freq_texts[] = {
    "1.5MHz",
    "3.0MHz",
};

static const char *const adau19xx_boost_recovery_mode_texts[] = {
    "Automatic Fault Recovery",
    "Manual Fault Recovery",
};

static const char *const adau19xx_common_en_texts[] = {
    "Disable",
    "Enable",
};

static const char *const adau19xx_common_onoff_texts[] = {
    "Off",
    "On",
};

static const char *const adau19xx_micbias_volts_texts[] = {
    "5.0V",
    "5.5V",
    "6.0V",
    "6.5V",
    "7.0V",
    "7.5V",
    "8.0V",
    "8.5V",
    "9.0V",
};

static const char *const adau19xx_drv_hiz_texts[] = {
    "Low", //不用的输出驱动到低电平
    "Hi-Z", //不用的输出处于高阻态
};

static const struct soc_enum adau19xx_enum[] = {
    SOC_ENUM_SINGLE(ADAU19XX_REG_MISC_CONTROL, 6, 3, adau19xx_sum_mode_texts), //实现较高SNR信噪比的通道求和模式控制
    SOC_ENUM_SINGLE(ADAU19XX_REG_BOOST, 5, 3, adau19xx_boost_fs_rate_texts), //升压开关频率的采样速率控制
    SOC_ENUM_SINGLE(ADAU19XX_REG_BOOST, 4, 2, adau19xx_boost_sw_freq_texts), //升压调节器开关频率
    SOC_ENUM_SINGLE(ADAU19XX_REG_BOOST, 3, 2, adau19xx_common_en_texts), //过压故障保护 0=disable 1=enable
    SOC_ENUM_SINGLE(ADAU19XX_REG_BOOST, 1, 2, adau19xx_common_en_texts), //过流故障保护 0=disable 1=enable

    SOC_ENUM_SINGLE(ADAU19XX_REG_MICBIAS, 2, 2, adau19xx_common_onoff_texts), //Boost使能 0=off 1=on
    SOC_ENUM_SINGLE(ADAU19XX_REG_MICBIAS, 4, 9, adau19xx_micbias_volts_texts), //MICBIAS输出电压
    SOC_ENUM_SINGLE(ADAU19XX_REG_MICBIAS, 3, 2, adau19xx_common_onoff_texts), //Micbias使能 0=off 1=on
    SOC_ENUM_SINGLE(ADAU19XX_REG_MICBIAS, 0, 2, adau19xx_boost_recovery_mode_texts), //升压故障恢复模式 0=自动故障恢复 1=手动故障恢复

    SOC_ENUM_SINGLE(ADAU19XX_REG_SAI_OVERTEMP, 7, 2, adau19xx_common_onoff_texts), //通道4串行输出驱动使能
    SOC_ENUM_SINGLE(ADAU19XX_REG_SAI_OVERTEMP, 6, 2, adau19xx_common_onoff_texts), //通道3串行输出驱动使能
    SOC_ENUM_SINGLE(ADAU19XX_REG_SAI_OVERTEMP, 5, 2, adau19xx_common_onoff_texts), //通道2串行输出驱动使能
    SOC_ENUM_SINGLE(ADAU19XX_REG_SAI_OVERTEMP, 4, 2, adau19xx_common_onoff_texts), //通道1串行输出驱动使能
    SOC_ENUM_SINGLE(ADAU19XX_REG_SAI_OVERTEMP, 3, 2, adau19xx_drv_hiz_texts), //让不用的SAI通道处于三台还是积极驱动这些数据时隙

    SOC_ENUM_SINGLE(ADAU19XX_REG_DIAG_CONTROL, 3, 2, adau19xx_common_en_texts), //通道4诊断使能
    SOC_ENUM_SINGLE(ADAU19XX_REG_DIAG_CONTROL, 2, 2, adau19xx_common_en_texts), //通道3诊断使能
    SOC_ENUM_SINGLE(ADAU19XX_REG_DIAG_CONTROL, 1, 2, adau19xx_common_en_texts), //通道2诊断使能
    SOC_ENUM_SINGLE(ADAU19XX_REG_DIAG_CONTROL, 0, 2, adau19xx_common_en_texts), //通道1诊断使能
};


//后置ADC增益控制寄存器 范围0~255(-35.635dB~60dB 静音)
static const DECLARE_TLV_DB_MINMAX_MUTE(adau19xx_adc_gain, -3562, 6000);

#define ADAU_POST_ADC_GAIN(x)  SOC_SINGLE_TLV("POST ADC" #x " gain", ADAU19XX_REG_POST_ADC_GAIN((x) - 1), 0, 255, 1, adau19xx_adc_gain)
#define ADAU_HPF_SWITCH(x)  SOC_SINGLE("ADC" #x " Highpass-Filter Capture Switch",   ADAU19XX_REG_DC_HPF_CAL, (x) - 1, 1, 0)
#define ADAU_DC_SUB_SWITCH(x)  SOC_SINGLE("ADC" #x " DC Subtraction Capture Switch",   ADAU19XX_REG_DC_HPF_CAL, (x) + 3, 1, 0)

static const struct snd_kcontrol_new adau19xx_snd_controls[] = {
    //0x0A 后置ADC增益通道1控制寄存器
    ADAU_POST_ADC_GAIN(1),
    //0x0B 后置ADC增益通道1控制寄存器
    ADAU_POST_ADC_GAIN(2),
    //0x0C 后置ADC增益通道1控制寄存器
    ADAU_POST_ADC_GAIN(3),
    //0x0D 后置ADC增益通道1控制寄存器
    ADAU_POST_ADC_GAIN(4),

    //0x1A 数字直流高通滤波器
    ADAU_HPF_SWITCH(1),
    ADAU_HPF_SWITCH(2),
    ADAU_HPF_SWITCH(3),
    ADAU_HPF_SWITCH(4),
    // 0x1A 校准寄存器:扣除通道x校准产生的直流值
    ADAU_DC_SUB_SWITCH(1),
    ADAU_DC_SUB_SWITCH(2),
    ADAU_DC_SUB_SWITCH(3),
    ADAU_DC_SUB_SWITCH(4),

    //0x0E
    SOC_ENUM("Sum Mode", adau19xx_enum[0]), //通道求和模式控制

    //0x02
    SOC_ENUM("Boost Sample Rate", adau19xx_enum[1]), //升压开关频率的采样速率控制
    SOC_ENUM("Boost Switch Freq", adau19xx_enum[2]), //升压调节器开关频率
    SOC_ENUM("Boost Over Voltage Protect", adau19xx_enum[3]), //过压故障保护 0=disable 1=enable
    SOC_ENUM("Boost Over Current Protect", adau19xx_enum[4]), //过流故障保护 0=disable 1=enable

    //0x03
    SOC_ENUM("Boost Enable", adau19xx_enum[5]), //Boost开关
    SOC_ENUM("Micbias Voltage", adau19xx_enum[6]), //MICBIAS输出电压
    SOC_ENUM("Micbias Enable", adau19xx_enum[7]), //MICBIAS开关
    SOC_ENUM("Boost Recovery Mode", adau19xx_enum[8]), //升压故障恢复模式 0=自动故障恢复 1=手动故障恢复

    //0x09
    SOC_ENUM("Ch4 Drive", adau19xx_enum[9]), //通道4串行输出驱动使能
    SOC_ENUM("Ch3 Drive", adau19xx_enum[10]), //通道3串行输出驱动使能
    SOC_ENUM("Ch2 Drive", adau19xx_enum[11]), //通道2串行输出驱动使能
    SOC_ENUM("Ch1 Drive", adau19xx_enum[12]), //通道1串行输出驱动使能
    SOC_ENUM("Unused Outputs Status", adau19xx_enum[13]), //让不用的SAI通道处于三台还是积极驱动这些数据时隙

    //0x10
    SOC_ENUM("Ch4 Diagnostics", adau19xx_enum[14]), //通道4诊断使能
    SOC_ENUM("Ch3 Diagnostics", adau19xx_enum[15]), //通道3诊断使能
    SOC_ENUM("Ch2 Diagnostics", adau19xx_enum[16]), //通道2诊断使能
    SOC_ENUM("Ch1 Diagnostics", adau19xx_enum[17]), //通道1诊断使能
};

static const struct snd_soc_dapm_widget adau19xx_dapm_widgets[] = {
    //input widgets
    SND_SOC_DAPM_INPUT("AIN1"),
    SND_SOC_DAPM_INPUT("AIN2"),
    SND_SOC_DAPM_INPUT("AIN3"),
    SND_SOC_DAPM_INPUT("AIN4"),

    //模块电源控制和串行端口控制寄存器
    SND_SOC_DAPM_SUPPLY("Vref", ADAU19XX_REG_BLOCK_POWER_SAI, 4, 0, NULL, 0), //基准电压使能
    SND_SOC_DAPM_ADC("ADC1", "Capture", ADAU19XX_REG_BLOCK_POWER_SAI, 0, 0), //ADC通道1使能
    SND_SOC_DAPM_ADC("ADC2", "Capture", ADAU19XX_REG_BLOCK_POWER_SAI, 1, 0), //ADC通道2使能
    SND_SOC_DAPM_ADC("ADC3", "Capture", ADAU19XX_REG_BLOCK_POWER_SAI, 2, 0), //ADC通道3使能
    SND_SOC_DAPM_ADC("ADC4", "Capture", ADAU19XX_REG_BLOCK_POWER_SAI, 3, 0), //ADC通道4使能

    SND_SOC_DAPM_OUTPUT("VREF"),
};

static const struct snd_soc_dapm_route adau19xx_dapm_routes[] = {
    { "ADC1", NULL, "AIN1"},
    { "ADC2", NULL, "AIN2"},
    { "ADC3", NULL, "AIN3"},
    { "ADC4", NULL, "AIN4"},

    { "ADC1", NULL, "Vref"},
    { "ADC2", NULL, "Vref"},
    { "ADC3", NULL, "Vref"},
    { "ADC4", NULL, "Vref"},

    { "VREF", NULL, "Vref"},
};

static int adau19xx_reset(struct adau1977 *adau19xx) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    int ret;
    regcache_cache_bypass(adau19xx->regmap, true);
    ret = regmap_write(adau19xx->regmap, ADAU19XX_REG_POWER, ADAU19XX_POWER_RESET); //软件复位
    regcache_cache_bypass(adau19xx->regmap, false);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("soft reset error\n");
#endif
        return ret;
    }
    return ret;
}

static int adau19xx_power_disable(struct adau1977 *adau19xx) {
    int ret = 0;

    if (!adau19xx->enabled)
        return 0;

    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_POWER,
            ADAU19XX_POWER_PWUP, 0);
    if (ret)
        return ret;

    regcache_mark_dirty(adau19xx->regmap);

    if (adau19xx->reset_gpio)
        gpiod_set_value_cansleep(adau19xx->reset_gpio, 0);

    regcache_cache_only(adau19xx->regmap, true);

    adau19xx->enabled = false;

    return 0;
}

static int adau19xx_power_enable(struct adau1977 *adau19xx) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx:%s \n", __FUNCTION__);
    pr_info("adau19xx->enabled init:%d \n", adau19xx->enabled);
    pr_info("adau19xx->reset_gpio:0x%x \n", adau19xx->reset_gpio);
#endif
    unsigned int val;
    int ret = 0;

    if (adau19xx->enabled)
        return 0;

    if (adau19xx->reset_gpio)
        gpiod_set_value_cansleep(adau19xx->reset_gpio, 1);

    regcache_cache_only(adau19xx->regmap, false); //cache only mode, 在这种模式下，写操作将仅更新CACHE值，不会真正设置到硬件中

    ret = adau19xx_reset(adau19xx);
    if (ret) {
        return ret;
    }

    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_POWER, ADAU19XX_POWER_PWUP, ADAU19XX_POWER_PWUP);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("power up error! \n");
#endif
        return ret;
    }
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("power up ok! \n");
#endif
    ret = regcache_sync(adau19xx->regmap);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("regcache_sync error! \n");
#endif
        return ret;
    }

    ret = regmap_read(adau19xx->regmap, ADAU19XX_REG_PLL, &val);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("read PLL status error! \n");
#endif
        return ret;
    }
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("read PLL status ok!\n");
#endif
    if (val == 0x41) {
        regcache_cache_bypass(adau19xx->regmap, true); // 设置读写寄存器不通过cache模式而是bypass模式，读写立即生效，一般在audio等确保时序性驱动中用到 
        ret = regmap_write(adau19xx->regmap, ADAU19XX_REG_PLL, 0x41);
        regcache_cache_bypass(adau19xx->regmap, false);
        if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
            pr_err("write PLL status = 0x41 failed!\n");
#endif
            return ret;
        }
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_info("write PLL status = 0x41 ok!\n");
#endif
    }

    adau19xx->enabled = true;

    return ret;
}

static bool adau19xx_check_sysclk(unsigned int mclk, unsigned int base_freq) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    unsigned int mcs;

    if (mclk % (base_freq * 128) != 0)
        return false;

    mcs = mclk / (128 * base_freq);
    if (mcs < 1 || mcs > 6 || mcs == 5)
        return false;

    return true;
}

static int adau_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id, unsigned int freq, int dir) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    int ret = 0;
    struct snd_soc_codec *codec = dai->codec;
    struct adau1977 *adau19xx = snd_soc_codec_get_drvdata(codec);
    unsigned int mask = 0;
    unsigned int clk_src;
    int source = adau19xx->sysclk_src;
    freq = 12288000;
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("dir=%d \n", dir);
    pr_info("clk_id=%d \n", clk_id);
    pr_info("freq=%d \n", freq);
#endif

    if (dir != SND_SOC_CLOCK_IN)
        return -EINVAL;

    if (clk_id != ADAU19XX_SYSCLK)
        return -EINVAL;

    switch (source) {
        case ADAU19XX_SYSCLK_SRC_MCLK:
            clk_src = 0;
#ifdef CONFIG_ADAU19XX_DEBUG
            pr_info("ADAU19XX_SYSCLK_SRC_MCLK\n");
#endif
            break;
        case ADAU19XX_SYSCLK_SRC_LRCLK:
            clk_src = ADAU19XX_PLL_CLK_S;
#ifdef CONFIG_ADAU19XX_DEBUG
            pr_info("ADAU19XX_SYSCLK_SRC_LRCLK\n");
#endif
            break;
        default:
            return -EINVAL;
    }

    if (freq != 0 && source == ADAU19XX_SYSCLK_SRC_MCLK) {
        if (freq < 4000000 || freq > 36864000)
            return -EINVAL;

        if (adau19xx_check_sysclk(freq, 32000))
            mask |= ADAU19XX_RATE_CONSTRAINT_MASK_32000;
        if (adau19xx_check_sysclk(freq, 44100))
            mask |= ADAU19XX_RATE_CONSTRAINT_MASK_44100;
        if (adau19xx_check_sysclk(freq, 48000))
            mask |= ADAU19XX_RATE_CONSTRAINT_MASK_48000;

        if (mask == 0)
            return -EINVAL;
    } else if (source == ADAU19XX_SYSCLK_SRC_LRCLK) {
        mask = ADAU19XX_RATE_CONSTRAINT_MASK_LRCLK;
    }
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("mask = 0x%2x\n", mask);
#endif
    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_PLL,
            ADAU19XX_PLL_CLK_S, clk_src);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_info("ADAU19XX_REG_PLL set failed! \n");
#endif
        return ret;
    }

    adau19xx->constraints.mask = mask;
    //adau19xx->sysclk_src = source;
    adau19xx->sysclk = freq;
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx->constraints.mask=%d \n", adau19xx->constraints.mask);
    pr_info("adau19xx->sysclk_src=%d \n", adau19xx->sysclk_src);
    pr_info("adau19xx->sysclk=%d \n", adau19xx->sysclk);
#endif
    return 0;
}

static int adau19xx_startup(struct snd_pcm_substream *substream,
        struct snd_soc_dai *dai) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    struct adau1977 *adau19xx = snd_soc_codec_get_drvdata(dai->codec);

    snd_pcm_hw_constraint_list(substream->runtime, 0,
            SNDRV_PCM_HW_PARAM_RATE, &adau19xx->constraints);

    if (adau19xx->master)
        snd_pcm_hw_constraint_minmax(substream->runtime,
            SNDRV_PCM_HW_PARAM_RATE, 8000, adau19xx->max_master_fs);

    return 0;
}

static int adau19xx_lookup_fs(unsigned int rate) {
    int ret = -EINVAL;

    if (rate >= 8000 && rate <= 12000)
        ret = ADAU19XX_SAI_CTRL0_FS_8000_12000;
    else if (rate >= 16000 && rate <= 24000)
        ret = ADAU19XX_SAI_CTRL0_FS_16000_24000;
    else if (rate >= 32000 && rate <= 48000)
        ret = ADAU19XX_SAI_CTRL0_FS_32000_48000;
    else if (rate >= 64000 && rate <= 96000)
        ret = ADAU19XX_SAI_CTRL0_FS_64000_96000;
    else if (rate >= 128000 && rate <= 192000)
        ret = ADAU19XX_SAI_CTRL0_FS_128000_192000;
    else
        ret = -EINVAL;

#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("+adau19xx:%s \n", __FUNCTION__);
    pr_info("fs rate reg:0x%02x\n", ret);
#endif
    return ret;
}

static int adau19xx_lookup_mcs(struct adau1977 *adau19xx, unsigned int rate, unsigned int fs) {
    unsigned int mcs;
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("+adau19xx:%s \n", __FUNCTION__);
    pr_info("rate=%d fs=%d\n", rate, fs);
#endif
    /*
     * rate = sysclk / (512 * mcs_lut[mcs]) * 2**fs
     * => mcs_lut[mcs] = sysclk / (512 * rate) * 2**fs
     * => mcs_lut[mcs] = sysclk / ((512 / 2**fs) * rate)
     */
    rate *= 512 >> fs;

#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("rate final=%d \n", rate);
    pr_info("adau19xx->sysclk=%d \n", adau19xx->sysclk);
#endif

    if (adau19xx->sysclk % rate != 0) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("lookup mcs failed\n");
#endif
        return -EINVAL;
    }

    mcs = adau19xx->sysclk / rate;

    /* The factors configured by MCS are 1, 2, 3, 4, 6 */
    if (mcs < 1 || mcs > 6 || mcs == 5) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_info("lookup mcs failed\n");
#endif
        return -EINVAL;
    }

    mcs = mcs - 1;
    if (mcs == 5)
        mcs = 4;

#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("mcs=%d\n", mcs);
#endif

    return mcs;
}

static int adau19xx_hw_params(struct snd_pcm_substream *substream,
        struct snd_pcm_hw_params *params, struct snd_soc_dai *dai) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    struct snd_soc_codec *codec = dai->codec;
    struct adau1977 *adau19xx = snd_soc_codec_get_drvdata(codec);
    unsigned int rate = params_rate(params);
    unsigned int slot_width;
    unsigned int ctrl0, ctrl0_mask;
    unsigned int ctrl1;
    int mcs, fs;
    int ret;

#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx->sysclk_src =%d \n", adau19xx->sysclk_src);
#endif

    fs = adau19xx_lookup_fs(rate);
    if (fs < 0)
        return fs;

    if (adau19xx->sysclk_src == ADAU19XX_SYSCLK_SRC_MCLK) {
        mcs = adau19xx_lookup_mcs(adau19xx, rate, fs);
        if (mcs < 0)
            return mcs;
    } else {
        mcs = 0;
    }
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("final mcs =%d \n", mcs);
#endif
    ctrl0_mask = ADAU19XX_SAI_CTRL0_FS_MASK;
    ctrl0 = fs;

    if (adau19xx->right_j) {
        switch (params_width(params)) {
            case 16:
                ctrl0 |= ADAU19XX_SAI_CTRL0_FMT_RJ_16BIT;
                break;
            case 24:
                ctrl0 |= ADAU19XX_SAI_CTRL0_FMT_RJ_24BIT;
                break;
            default:
                return -EINVAL;
        }
        ctrl0_mask |= ADAU19XX_SAI_CTRL0_FMT_MASK;
    }

#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx->right_j =%d \n", adau19xx->right_j);
    pr_info("params_width(params) =%d \n", params_width(params));
#endif

    if (adau19xx->master) {
        switch (params_width(params)) {
            case 16:
                ctrl1 = ADAU19XX_SAI_CTRL1_DATA_WIDTH_16BIT;
                slot_width = 16;
                break;
            case 24:
            case 32:
                ctrl1 = ADAU19XX_SAI_CTRL1_DATA_WIDTH_24BIT;
                slot_width = 32;
                break;
            default:
                return -EINVAL;
        }

        /* In TDM mode there is a fixed slot width */
        if (adau19xx->slot_width)
            slot_width = adau19xx->slot_width;

        if (slot_width == 16)
            ctrl1 |= ADAU19XX_SAI_CTRL1_BCLKRATE_16;
        else
            ctrl1 |= ADAU19XX_SAI_CTRL1_BCLKRATE_32;

        ret = regmap_update_bits(adau19xx->regmap,
                ADAU19XX_REG_SAI_CTRL1,
                ADAU19XX_SAI_CTRL1_DATA_WIDTH_MASK |
                ADAU19XX_SAI_CTRL1_BCLKRATE_MASK,
                ctrl1);
        if (ret < 0)
            return ret;
    }

#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx->master =%d \n", adau19xx->master);
    pr_info("adau19xx->slot_width =%d \n", adau19xx->slot_width);
    pr_info("slot_width =%d \n", slot_width);
#endif

    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_SAI_CTRL0, ctrl0_mask, ctrl0);
    if (ret < 0)
        return ret;

    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_PLL, ADAU19XX_PLL_MCS_MASK, mcs);
    return ret;
}

static int adau19xx_mute(struct snd_soc_dai *dai, int mute, int stream) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    struct snd_soc_codec *codec = dai->codec;
    struct adau1977 *adau19xx = snd_soc_codec_get_drvdata(dai->codec);
    unsigned int val;

    if (mute) {
        val = ADAU19XX_MISC_CONTROL_MMUTE;
    } else {
        val = 0; //关闭静音
    }
    return regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_MISC_CONTROL, ADAU19XX_MISC_CONTROL_MMUTE, val);
}

static int adau19xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    int ret;
    struct snd_soc_codec *codec = dai->codec;
    struct adau1977 *adau19xx = snd_soc_codec_get_drvdata(dai->codec);
    unsigned int ctrl0 = 0, ctrl1 = 0, block_power = 0;
    bool invert_lrclk;

    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBS_CFS:
            adau19xx->master = false;
            break;
        case SND_SOC_DAIFMT_CBM_CFM:
            ctrl1 |= ADAU19XX_SAI_CTRL1_MASTER;
            adau19xx->master = true;
            break;
        default:
            return -EINVAL;
    }
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("fmt & SND_SOC_DAIFMT_MASTER_MASK=%d \n", fmt & SND_SOC_DAIFMT_MASTER_MASK);
    pr_info("adau19xx->master=%d \n", adau19xx->master);
#endif
    switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
        case SND_SOC_DAIFMT_NB_NF:
            invert_lrclk = false;
            break;
        case SND_SOC_DAIFMT_IB_NF:
            block_power |= ADAU19XX_BLOCK_POWER_SAI_BCLK_EDGE;
            invert_lrclk = false;
            break;
        case SND_SOC_DAIFMT_NB_IF:
            invert_lrclk = true;
            break;
        case SND_SOC_DAIFMT_IB_IF:
            block_power |= ADAU19XX_BLOCK_POWER_SAI_BCLK_EDGE;
            invert_lrclk = true;
            break;
        default:
            return -EINVAL;
    }
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("fmt & SND_SOC_DAIFMT_INV_MASK=%d \n", fmt & SND_SOC_DAIFMT_INV_MASK);
#endif
    adau19xx->right_j = false;
    switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
        case SND_SOC_DAIFMT_I2S:
            ctrl0 |= ADAU19XX_SAI_CTRL0_FMT_I2S;
            break;
        case SND_SOC_DAIFMT_LEFT_J:
            ctrl0 |= ADAU19XX_SAI_CTRL0_FMT_LJ;
            invert_lrclk = !invert_lrclk;
            break;
        case SND_SOC_DAIFMT_RIGHT_J:
            ctrl0 |= ADAU19XX_SAI_CTRL0_FMT_RJ_24BIT;
            adau19xx->right_j = true;
            invert_lrclk = !invert_lrclk;
            break;
        case SND_SOC_DAIFMT_DSP_A:
            ctrl1 |= ADAU19XX_SAI_CTRL1_LRCLK_PULSE;
            ctrl0 |= ADAU19XX_SAI_CTRL0_FMT_I2S;
            invert_lrclk = false;
            break;
        case SND_SOC_DAIFMT_DSP_B:
            ctrl1 |= ADAU19XX_SAI_CTRL1_LRCLK_PULSE;
            ctrl0 |= ADAU19XX_SAI_CTRL0_FMT_LJ;
            invert_lrclk = false;
            break;
        default:
            return -EINVAL;
    }

    if (invert_lrclk)
        block_power |= ADAU19XX_BLOCK_POWER_SAI_LR_POL;
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("fmt & SND_SOC_DAIFMT_FORMAT_MASK=%d \n", fmt & SND_SOC_DAIFMT_FORMAT_MASK);
    pr_info("block_power=%d invert_lrclk=%d\n", block_power, invert_lrclk);
    pr_info("ctrl0=%d ctrl1=%d\n", ctrl0, ctrl1);
    pr_info("adau19xx->right_j=%d\n", adau19xx->right_j);
#endif

    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_BLOCK_POWER_SAI, ADAU19XX_BLOCK_POWER_SAI_LR_POL |
            ADAU19XX_BLOCK_POWER_SAI_BCLK_EDGE, block_power);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("write LR_POL|BCLK_EDGE failed\n");
#endif
        return ret;
    }

    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_SAI_CTRL0,
            ADAU19XX_SAI_CTRL0_FMT_MASK, ctrl0);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("write ADAU19XX_SAI_CTRL0_FMT_MASK failed\n");
#endif
        return ret;
    }

    ret = regmap_update_bits(adau19xx->regmap, ADAU19XX_REG_SAI_CTRL1,
            ADAU19XX_SAI_CTRL1_MASTER | ADAU19XX_SAI_CTRL1_LRCLK_PULSE, ctrl1);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("ADAU19XX_REG_SAI_CTRL1 set fail!\n");
#endif
        return ret;
    }

    return 0;
}

static const struct snd_soc_dai_ops adau19xx_dai_ops = {
    //DAI clocking configuration
    .set_sysclk = adau_set_dai_sysclk,

    .startup = adau19xx_startup,
    //ALSA PCM audio operations
    .hw_params = adau19xx_hw_params,
    .mute_stream = adau19xx_mute,
    //DAI format configuration
    .set_fmt = adau19xx_set_fmt,
};

static struct snd_soc_dai_driver adau19xx_dai = {
    .name = "adau19xx-codec",
    .capture =
    {
        .stream_name = "Capture",
        .channels_min = 1,
        .channels_max = ADAU19XX_CHANNELS_MAX,
        .rates = ADAU19XX_RATES,
        .formats = ADAU19XX_FORMATS,
        .sig_bits = 24,
    },
    .ops = &adau19xx_dai_ops,
};

static int adau19xx_add_widgets(struct snd_soc_codec *codec) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
    snd_soc_add_codec_controls(codec, adau19xx_snd_controls, ARRAY_SIZE(adau19xx_snd_controls));
    snd_soc_dapm_new_controls(dapm, adau19xx_dapm_widgets, ARRAY_SIZE(adau19xx_dapm_widgets));
    snd_soc_dapm_add_routes(dapm, adau19xx_dapm_routes, ARRAY_SIZE(adau19xx_dapm_routes));
    return 0;
}

static int adau19xx_codec_probe(struct snd_soc_codec *codec) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    adau19xx_add_widgets(codec);
    return 0;
}

static int adau19xx_set_bias_level(struct snd_soc_codec *codec, enum snd_soc_bias_level level) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    struct adau1977 *adau19xx = dev_get_drvdata(codec->dev);
    switch (level) {
        case SND_SOC_BIAS_ON:
#ifdef CONFIG_ADAU19XX_DEBUG
            pr_info("bias level = SND_SOC_BIAS_ON \n");
#endif
            if (adau19xx->sysclk_src == 0) {//MCLK
                mdelay(60); //防止噼啪声
            }
            break;
        case SND_SOC_BIAS_PREPARE:
#ifdef CONFIG_ADAU19XX_DEBUG
            pr_info("bias level = SND_SOC_BIAS_PREPARE \n");
#endif
            break;
        case SND_SOC_BIAS_STANDBY:
#ifdef CONFIG_ADAU19XX_DEBUG
            pr_info("bias level = SND_SOC_BIAS_STANDBY \n");
#endif
            break;
        case SND_SOC_BIAS_OFF:
#ifdef CONFIG_ADAU19XX_DEBUG
            pr_info("bias level = SND_SOC_BIAS_OFF \n");
#endif
            break;
    }

    return 0;
}

static unsigned int adau19xx_codec_read(struct snd_soc_codec *codec, unsigned int reg) {
    unsigned int value_r = 0;
    int ret = 0;
    struct adau1977 *adau19xx = dev_get_drvdata(codec->dev);
    ret = regmap_read(adau19xx->regmap, reg, &value_r);
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
    adau19xx_print_msg(reg, ret, value_r);
#endif
    return value_r;
}

static int adau19xx_codec_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int val) {
    int ret = 0;
    struct adau1977 *adau19xx = dev_get_drvdata(codec->dev);
    ret = regmap_write(adau19xx->regmap, reg, val);
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
    adau19xx_print_msg(reg, ret, val);
#endif
    return 0;
}

static const struct snd_soc_codec_driver adau19xx_soc_codec_driver = {
    .probe = adau19xx_codec_probe,
    .set_bias_level = adau19xx_set_bias_level,
    //.read = adau19xx_codec_read,
    //.write = adau19xx_codec_write,
    .idle_bias_off = true,
};

//------------------------------------------------------------------------
#ifdef CONFIG_ADAU19XX_DEBUG
//调试用

static ssize_t adau19xx_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
    adau19xx_do_store(dev, buf, count);
    return count;
}

static ssize_t adau19xx_show(struct device *dev, struct device_attribute *attr, char *buf) {
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
    adau19xx_do_show();
}

static DEVICE_ATTR(adau, 0664, adau19xx_show, adau19xx_store);

static struct attribute * adau19xx_debug_attrs[] = {&dev_attr_adau.attr, NULL,};

static struct attribute_group adau19xx_debug_attr_group = {
    .name = "adau19xx_debug",
    .attrs = adau19xx_debug_attrs,
};
#endif
//------------------------------------------------------------------------

int adau19xx_probe(struct i2c_client *i2c, struct regmap *regmap, enum adau19xx_type type) {
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("-----------------------------------\n");
    pr_info("adau19xx:%s \n", __FUNCTION__);
#endif
    unsigned int power_off_mask;
    int ret = 0, val = 0;
    struct adau1977 *adau19xx;
    struct device_node *np = i2c->dev.of_node;
    adau19xx = devm_kzalloc(&i2c->dev, sizeof (*adau19xx), GFP_KERNEL);
    if (adau19xx == NULL) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("Unable to allocate adau private data\n");
#endif
        return -ENOMEM;
    }

    ret = of_property_read_u32(np, "sysclk-src", &val);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("Please set sysclk-src.\n");
#endif
        return -EINVAL;
    }

    adau19xx->sysclk_src = val;
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx->sysclk_src :%d\n", adau19xx->sysclk_src);
#endif

    adau19xx->dev = &i2c->dev;
    adau19xx->type = type;
    adau19xx->regmap = regmap;
    adau19xx->max_master_fs = 192000;
    adau19xx->constraints.list = adau19xx_rates;
    adau19xx->constraints.count = ARRAY_SIZE(adau19xx_rates);

#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("i2c_id->driver_data :%d\n", type);
    pr_info("i2c->name :%s\n", i2c->name);
    pr_info("i2c->flags :%d\n", i2c->flags);
    pr_info("i2c->addr :%d\n", i2c->addr);
    pr_info("i2c->irq :%d\n", i2c->irq);
#endif

    adau19xx->reset_gpio = devm_gpiod_get_optional(&i2c->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(adau19xx->reset_gpio)) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("adau19xx->reset_gpio read error!\n");
#endif
        return PTR_ERR(adau19xx->reset_gpio);
    }
#ifdef CONFIG_ADAU19XX_DEBUG
    pr_info("adau19xx->reset_gpio=0x%x!\n", adau19xx->reset_gpio);
#endif

    dev_set_drvdata(&i2c->dev, adau19xx);

    ret = adau19xx_power_enable(adau19xx);
    if (ret) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("power enable fail!\n");
#endif
        adau19xx_power_disable(adau19xx);
        return ret;
    }

#ifdef CONFIG_ADAU19XX_DEBUG
    //debug调试
    ret = sysfs_create_group(&i2c->dev.kobj, &adau19xx_debug_attr_group);
    if (ret) {
        pr_err("failed to create attr adau\n");
    } else {
        pr_info("create sysfs OK in /sys/class/i2c-adapter/i2c-1/1-0071/adau_debug");
    }
#endif

    ret = snd_soc_register_codec(&i2c->dev, &adau19xx_soc_codec_driver, &adau19xx_dai, 1);
    if (ret < 0) {
#ifdef CONFIG_ADAU19XX_DEBUG
        pr_err("Failed to register adau codec: %d\n", ret);
#endif
    }
    return ret;
}

EXPORT_SYMBOL_GPL(adau19xx_probe);

MODULE_DESCRIPTION("ASoC ADAU19xx driver");
MODULE_AUTHOR("Benjamin Wan<32132145@qq.com>");
MODULE_LICENSE("GPL");
