#include <linux/module.h>
#include <sound/soc.h>
#include "adau19xx.h"

char * adau_reg_name(u8 reg) {
    char * regname = "";
    switch (reg) {
        case ADAU19XX_REG_POWER:
            regname = "ADAU19XX_REG_POWER";
            break;
        case ADAU19XX_REG_PLL:
            regname = "ADAU19XX_REG_PLL";
            break;
        case ADAU19XX_REG_BOOST:
            regname = "ADAU19XX_REG_BOOST";
            break;
        case ADAU19XX_REG_MICBIAS:
            regname = "ADAU19XX_REG_MICBIAS";
            break;
        case ADAU19XX_REG_BLOCK_POWER_SAI:
            regname = "ADAU19XX_REG_BLOCK_POWER_SAI";
            break;
        case ADAU19XX_REG_SAI_CTRL0:
            regname = "ADAU19XX_REG_SAI_CTRL0";
            break;
        case ADAU19XX_REG_SAI_CTRL1:
            regname = "ADAU19XX_REG_SAI_CTRL1";
            break;
        case ADAU19XX_REG_CMAP12:
            regname = "ADAU19XX_REG_CMAP12";
            break;
        case ADAU19XX_REG_CMAP34:
            regname = "ADAU19XX_REG_CMAP34";
            break;
        case ADAU19XX_REG_SAI_OVERTEMP:
            regname = "ADAU19XX_REG_SAI_OVERTEMP";
            break;
        case ADAU19XX_REG_POST_ADC_GAIN(0):
            regname = "ADAU19XX_REG_POST_ADC_GAIN(0)";
            break;
        case ADAU19XX_REG_POST_ADC_GAIN(1):
            regname = "ADAU19XX_REG_POST_ADC_GAIN(1)";
            break;
        case ADAU19XX_REG_POST_ADC_GAIN(2):
            regname = "ADAU19XX_REG_POST_ADC_GAIN(2)";
            break;
        case ADAU19XX_REG_POST_ADC_GAIN(3):
            regname = "ADAU19XX_REG_POST_ADC_GAIN(3)";
            break;
        case ADAU19XX_REG_MISC_CONTROL:
            regname = "ADAU19XX_REG_MISC_CONTROL";
            break;
        case ADAU19XX_REG_ADC_BIAS_CONTROL:
            regname = "ADAU19XX_REG_ADC_BIAS_CONTROL";
            break;
        case ADAU19XX_REG_DIAG_CONTROL:
            regname = "ADAU19XX_REG_DIAG_CONTROL";
            break;
        case ADAU19XX_REG_STATUS(0):
            regname = "ADAU19XX_REG_STATUS(0)";
            break;
        case ADAU19XX_REG_STATUS(1):
            regname = "ADAU19XX_REG_STATUS(1)";
            break;
        case ADAU19XX_REG_STATUS(2):
            regname = "ADAU19XX_REG_STATUS(2)";
            break;
        case ADAU19XX_REG_STATUS(3):
            regname = "ADAU19XX_REG_STATUS(3)";
            break;
        case ADAU19XX_REG_DIAG_IRQ1:
            regname = "ADAU19XX_REG_DIAG_IRQ1";
            break;
        case ADAU19XX_REG_DIAG_IRQ2:
            regname = "ADAU19XX_REG_DIAG_IRQ2";
            break;
        case ADAU19XX_REG_ADJUST1:
            regname = "ADAU19XX_REG_ADJUST1";
            break;
        case ADAU19XX_REG_ADJUST2:
            regname = "ADAU19XX_REG_ADJUST2";
            break;
        case ADAU19XX_REG_ADC_CLIP:
            regname = "ADAU19XX_REG_ADC_CLIP";
            break;
        case ADAU19XX_REG_DC_HPF_CAL:
            regname = "ADAU19XX_REG_DC_HPF_CAL";
            break;
        case ADAU19XX_REG_TWEAK1:
            regname = "ADAU19XX_REG_TWEAK1";
            break;
        default:
            break;
    }
    return regname;
}

void adau19xx_print_msg(u8 reg, int ret, int value) {
    char *regname;
    regname = adau_reg_name(reg);
    if (ret) {
        pr_info("REG[0x%02x]:%s read/write failed", reg, regname);
    } else {
        pr_info("REG[0x%02x]:%s read/write successed: 0x%02x %d", reg, regname, value);
    }
}
EXPORT_SYMBOL_GPL(adau19xx_print_msg);

void adau19xx_do_store(struct device *dev, const char *buf, size_t count) {
    struct adau1977 *adau = dev_get_drvdata(dev);
    int val = 0, flag = 0;
    u8 i = 0, reg, num, value_w;
    int ret = 0, value_r;

    val = simple_strtol(buf, NULL, 16);
    flag = (val >> 16) & 0xF;

    pr_info("val= 0x%02x , flag=0x%02x\n", val, flag);


    if (flag) {
        reg = (val >> 8) & 0xFF;
        value_w = val & 0xFF;
        regcache_cache_bypass(adau->regmap, true);
        ret = regmap_write(adau->regmap, reg, value_w);
        regcache_cache_bypass(adau->regmap, false);
        adau19xx_print_msg(reg, ret, value_w);
    } else {
        reg = (val >> 8) & 0xFF;
        num = val & 0xff;
        pr_info("===================================\n");
        pr_info("Read: start REG:0x%02x,count:0x%02x\n", reg, num);

        do {
            value_r = 0;
            regcache_cache_bypass(adau->regmap, true);
            ret = regmap_read(adau->regmap, reg, &value_r);
            regcache_cache_bypass(adau->regmap, false);
            pr_info("-----------------------------------\n");
            adau19xx_print_msg(reg, ret, value_r);
            reg++;
            i++;
        } while (i <= num);
        pr_info("===================================\n");
    }
}
EXPORT_SYMBOL_GPL(adau19xx_do_store);

void adau19xx_do_show(void) {
    pr_info("echo flag|reg|val > adau\n");
    pr_info("eg read[0x00] all value:from reg[0x00] to reg[0x1a] :echo 0001a > adau\n");
    pr_info("eg read[0x00] ADAU19XX_REG_POWER value: reg[0x00] count[0x01] :echo 00001 > adau\n");
    pr_info("eg write[0x01] value[0x01] to reg[0x00] :echo 10001 > adau\n");
    //pr_info("ADAU19XX_POWER_RESET:0x%02x , ADAU19XX_POWER_PWUP:0x%02x\n", ADAU19XX_POWER_RESET, ADAU19XX_POWER_PWUP);
    pr_info("ADAU19XX_POWER_RESET:echo 10080 > adau\n");
    pr_info("ADAU19XX_POWER_PWUP:echo 10001 > adau\n");
}
EXPORT_SYMBOL_GPL(adau19xx_do_show);

MODULE_DESCRIPTION("ASoC ADAU19XX driver");
MODULE_AUTHOR("Benjamin Wan<32132145@qq.com>");
MODULE_LICENSE("GPL");
