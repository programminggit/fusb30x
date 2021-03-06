/*
 * File:   fusb30x_driver.c
 * Author: Tim Bremm <tim.bremm@fairchildsemi.com>
 * Company: Fairchild Semiconductor
 *
 * Created on September 2, 2015, 10:22 AM
 */

/* Standard Linux includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/of_device.h>

/* Driver-specific includes */
#include "fusb30x_global.h"
#include "platform_helpers.h"
#include "core.h"
#include "fusb30x_driver.h"

/******************************************************************************
* Driver functions
******************************************************************************/
static int __init fusb30x_init(void)
{
	printk(KERN_DEBUG "FUSB  %s - Start driver initialization...\n", __func__);

	return i2c_add_driver(&fusb30x_driver);
}

static void __exit fusb30x_exit(void)
{
	i2c_del_driver(&fusb30x_driver);
	printk(KERN_DEBUG "FUSB  %s - Driver deleted...\n", __func__);
}

static int fusb30x_probe (struct i2c_client* client,
                          const struct i2c_device_id* id)
{
	int ret = 0;
	struct fusb30x_chip* chip;
	struct i2c_adapter* adapter;

	if (!client) {
		printk(KERN_ALERT "FUSB  %s - Error: Client structure is NULL!\n", __func__);
		return -EINVAL;
	}
	dev_info(&client->dev, "%s\n", __func__);

	/* Make sure probe was called on a compatible device */
	if (!of_match_device(fusb30x_dt_match, &client->dev)) {
		dev_err(&client->dev, "FUSB  %s - Error: Device tree mismatch!\n", __func__);
		return -EINVAL;
	}
	printk(KERN_DEBUG "FUSB  %s - Device tree matched!\n", __func__);

	/* Verify that the system has our required I2C/SMBUS functionality (see <linux/i2c.h> for definitions) */
	adapter = to_i2c_adapter(client->dev.parent);
	if (!i2c_check_functionality(adapter, FUSB30X_I2C_SMBUS_REQUIRED_FUNC)) {
		dev_err(&client->dev, "FUSB  %s - Error: Required I2C/SMBus functionality not supported! Driver required func. mask: 0x%x\n", __func__, FUSB30X_I2C_SMBUS_REQUIRED_FUNC);
		dev_err(&client->dev, "FUSB  %s - I2C Supported Functionality Mask: 0x%x\n", __func__, i2c_get_functionality(adapter));
		return -EIO;
	}
	printk(KERN_DEBUG "FUSB  %s - I2C Functionality check passed!\n", __func__);

	/* Allocate space for our chip structure (devm_* is managed by the device) */
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "FUSB  %s - Error: Unable to allocate memory for g_chip!\n", __func__);
		return -ENOMEM;
	}
	// Assign our client handle to our chip
	chip->client = client;
	// Set our global chip's address to the newly allocated memory
	fusb30x_SetChip(chip);
	printk(KERN_DEBUG "FUSB  %s - Chip structure is set! Chip: %p ... g_chip: %p\n", __func__, chip, fusb30x_GetChip());

	/* Initialize the chip lock */
	mutex_init(&chip->lock);

	/* Initialize the chip's data members */
	fusb_InitChipData();
	printk(KERN_DEBUG "FUSB  %s - Chip struct data initialized!\n", __func__);

	/* Assign our struct as the client's driverdata */
	i2c_set_clientdata(client, chip);
	printk(KERN_DEBUG "FUSB  %s - I2C client data set!\n", __func__);

	/* Verify that our device exists and that it's what we expect */
	if (!fusb_IsDeviceValid()) {
		dev_err(&client->dev, "FUSB  %s - Error: Unable to communicate with device!\n", __func__);
		return -EIO;
	}
	printk(KERN_DEBUG "FUSB  %s - Device check passed!\n", __func__);

	/* Initialize the platform's GPIO pins */
	ret = fusb_InitializeGPIO();
	if (ret != 0) {
		dev_err(&client->dev, "FUSB  %s - Error: Unable to initialize GPIO!\n", __func__);
		return ret;
	}
	printk(KERN_DEBUG "FUSB  %s - GPIO initialized!\n", __func__);

	/* Init our workers, but don't start them yet */
	fusb_InitializeWorkers();
	printk(KERN_DEBUG "FUSB  %s - Workers initialized!\n", __func__);

	/* Initialize our timer */
	fusb_InitializeTimer();
	printk(KERN_DEBUG "FUSB  %s - Timers initialized!\n", __func__);

	/* Initialize sysfs file accessors */
	fusb_Sysfs_Init();
	printk(KERN_DEBUG "FUSB  %s - Sysfs device file created!\n", __func__);

	/* Initialize the core and enable the state machine */
	fusb_InitializeCore();
	printk(KERN_DEBUG "FUSB  %s - Core is initialized!\n", __func__);

	/* Start worker threads after successful initialization */
	fusb_ScheduleWork();

	dev_info(&client->dev, "FUSB  %s - FUSB30X Driver loaded successfully!\n", __func__);
	return ret;
}

static int fusb30x_remove(struct i2c_client* client)
{
	struct fusb30x_chip* chip = NULL;
	printk(KERN_DEBUG "FUSB  %s - Removing fusb30x device!\n", __func__);
	chip = i2c_get_clientdata(client);
	fusb_StopTimers();
	fusb_StopThreads();
	fusb_GPIO_Cleanup();
	printk(KERN_DEBUG "FUSB  %s - FUSB30x device removed from driver...\n", __func__);
	return 0;
}

/*******************************************************************************
 * Driver macros
 ******************************************************************************/
module_init(fusb30x_init);
module_exit(fusb30x_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fairchild FUSB30x Driver");
MODULE_AUTHOR("Tim Bremm <tim.bremm@fairchildsemi.com>");
