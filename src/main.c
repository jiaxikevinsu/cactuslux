#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>

#define I2C0_NODE DT_NODELABEL(veml7700)

static const struct device *get_veml7700_device(void)
{  
    /* You can use either of these to get the device */
    const struct device *dev = DEVICE_DT_GET(DT_INST(0,vishay_veml7700));
    //const struct device *const dev = DEVICE_DT_GET_ANY(st_lsm6dsl);


    //veml7700_init(dev);
    if (dev == NULL) {
        /* No such node, or the node does not have status "okay". */
        printk("\nError: no device found.\n");
        return NULL;
    }

    if (!device_is_ready(dev)) {
        printk("\nError: Device \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               dev->name);
        return NULL;
    }

    printk("Found device \"%s\", getting sensor data\n", dev->name);
    return dev;
}

int main(void)
{
        //retrieve the API-specific device structure
        const struct device *dev = get_veml7700_device();

        //check if the device is ready to use
        // if (!device_is_ready(dev_i2c.bus)){
        //         printk("I2C bus %s is not ready!\n\r", dev_i2c.bus->name);
        //         return;
        // }

        while(1){
                static struct sensor_value lux;        
                sensor_sample_fetch_chan(dev, SENSOR_CHAN_LIGHT);
                sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &lux);
                printk("lux: %d.%06d\n",lux.val1, lux.val2);
                k_sleep(K_MSEC(1000));
        }

        return 0;
}
