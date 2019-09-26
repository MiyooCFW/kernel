/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch-suniv/cpu.h>
#include <asm/arch-suniv/dma.h>
#include <asm/arch-suniv/gpio.h>
#include <asm/arch-suniv/intc.h>
#include <asm/arch-suniv/lcdc.h>
#include <asm/arch-suniv/debe.h>
#include <asm/arch-suniv/clock.h>
#include <asm/arch-suniv/common.h>

struct suniv_device_info {
	struct device *dev;
	struct power_supply *bat;
	struct power_supply_desc bat_desc;
};
uint8_t *lradc;

static int suniv_battery_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = (readl(lradc + 0x0c) * 3 * 3 * 10);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property suniv_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int suniv_battery_probe(struct platform_device *pdev)
{
	struct suniv_device_info *di=NULL;
	struct power_supply_config psy_cfg={0};

	di = devm_kzalloc(&pdev->dev, sizeof(*di), GFP_KERNEL);
	if (!di) {
		return -ENOMEM;
  }
	platform_set_drvdata(pdev, di);
  
  lradc = (uint8_t*)ioremap(SUNIV_LRADC_BASE, 4096);
  suniv_setbits(lradc, 1);

	di->dev = &pdev->dev;
	di->bat_desc.name = "miyoo-battery";
	di->bat_desc.type = POWER_SUPPLY_TYPE_BATTERY;
	di->bat_desc.properties = suniv_battery_props;
	di->bat_desc.num_properties = ARRAY_SIZE(suniv_battery_props);
	di->bat_desc.get_property = suniv_battery_get_property;
	psy_cfg.drv_data = di;
	di->bat = power_supply_register(di->dev, &di->bat_desc, &psy_cfg);
	return 0;
}

static int suniv_battery_remove(struct platform_device *pdev)
{
	struct suniv_device_info *di = platform_get_drvdata(pdev);

	power_supply_unregister(di->bat);
  iounmap(lradc);
	return 0;
}

static const struct of_device_id miyoo_battery_of_match[] = {
	{.compatible = "allwinner,suniv-f1c500s-battery", },{},
};
MODULE_DEVICE_TABLE(of, miyoo_battery_of_match);

static struct platform_driver suniv_battery_driver = {
	.probe = suniv_battery_probe,
	.remove = suniv_battery_remove,
	.driver = {
		.name = "miyoo-battery",
		.of_match_table = of_match_ptr(miyoo_battery_of_match),
	},
};
module_platform_driver(suniv_battery_driver);

MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_DESCRIPTION("Miyoo battery driver");
MODULE_LICENSE("GPL");

