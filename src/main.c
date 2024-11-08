#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
//#include <zephyr/drivers/i2c.h>
//#include "Adafruit_VEML7700.h"
//#include "simple_math.h"

/* The devicetree node identifier for the "led0" alias. */
//#define LED0_NODE DT_ALIAS(led0)

//static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);


int main(void)
{
        while(1){
                // int result1 = add(5, 3);
                // int result2 = subtract(10, 4);

                // printf("5 + 3 = %d\n", result1);
                // printf("10 - 4 = %d\n", result2);
                printf("Hello\n");
                k_msleep(100);
        }

        // int ret;
        // uint32_t timestamp = k_uptime_get();

        // if (!gpio_is_ready_dt(&led)) {
	// 	return 0;
	// }

	// ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	// if (ret < 0) {
	// 	return 0;
	// }

        // while(1){
        //         ret = gpio_pin_toggle_dt(&led);
	// 	if (ret < 0) {
	// 		return 0;
	// 	}
                
        //         if (k_uptime_get() - timestamp < 1000){
        //                 //do nothing
        //         }
        //         else{
        //                 timestamp = k_uptime_get();
        //                 printf("%d\tHello\n", timestamp);
        //         }
        // }
        return 0;
}
