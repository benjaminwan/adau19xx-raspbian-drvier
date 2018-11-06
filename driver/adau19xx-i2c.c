#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <sound/soc.h>

#include "adau19xx.h"

//regmap配置
static const struct reg_default adau19xx_reg_defaults[] = {
    { 0x00, 0x00},
    { 0x01, 0x41},
    { 0x02, 0x4a},
    { 0x03, 0x7d},
    { 0x04, 0x3d},
    { 0x05, 0x02},
    { 0x06, 0x00},
    { 0x07, 0x10},
    { 0x08, 0x32},
    { 0x09, 0xf0},
    { 0x0a, 0xa0},
    { 0x0b, 0xa0},
    { 0x0c, 0xa0},
    { 0x0d, 0xa0},
    { 0x0e, 0x02},
    { 0x10, 0x0f},
    { 0x15, 0x20},
    { 0x16, 0x00},
    { 0x17, 0x00},
    { 0x18, 0x00},
    { 0x1a, 0x00},
};

static bool adau19xx_register_volatile(struct device *dev, unsigned int reg) {
    /* volatile registers are not cached */
    switch (reg) {/* 只读的寄存器不做缓存 */
        case ADAU19XX_REG_STATUS(0):
        case ADAU19XX_REG_STATUS(1):
        case ADAU19XX_REG_STATUS(2):
        case ADAU19XX_REG_STATUS(3):
        case ADAU19XX_REG_ADC_CLIP:
            return true; /* always write-through */
    }

    return false;
}

static const struct regmap_config adau19xx_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = ADAU19XX_REG_DC_HPF_CAL,
    .volatile_reg = adau19xx_register_volatile,
    .cache_type = REGCACHE_RBTREE,
    .reg_defaults = adau19xx_reg_defaults,
    .num_reg_defaults = ARRAY_SIZE(adau19xx_reg_defaults),
};

static int adau19xx_i2c_probe(struct i2c_client *i2c,
        const struct i2c_device_id *i2c_id) {

    struct regmap *regmap;

    regmap = devm_regmap_init_i2c(i2c, &adau19xx_regmap_config);

    if (IS_ERR(regmap)) {
        return PTR_ERR(regmap);
    }

    return adau19xx_probe(i2c, regmap, i2c_id->driver_data);
}

static int adau19xx_i2c_remove(struct i2c_client *client) {
    snd_soc_unregister_codec(&client->dev);
    return 0;
}

static const struct i2c_device_id adau19xx_i2c_id[] = {
    { "adau19xx", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, adau19xx_i2c_id);

static const struct of_device_id adau19xx_of_match[] = {
    { .compatible = "adi,adau19xx"},
    {}
};
MODULE_DEVICE_TABLE(of, adau19xx_of_match);

static struct i2c_driver adau19xx_i2c_driver = {.driver =
    {
        .name = "adau19xx-i2c",
        .of_match_table = adau19xx_of_match,
    },
    .probe = adau19xx_i2c_probe,
    .remove = adau19xx_i2c_remove,
    .id_table = adau19xx_i2c_id,};

module_i2c_driver(adau19xx_i2c_driver);

MODULE_DESCRIPTION("ASoC ADAU19XX driver");
MODULE_AUTHOR("Benjamin Wan<32132145@qq.com>");
MODULE_LICENSE("GPL");
