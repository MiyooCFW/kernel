// Steward Fu
// steward.fu@gmail.com
// https://steward-fu.github.io/website/index.htm
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/gpio.h>

#define TITLE "LED driver for Lichee Pi Nano"
MODULE_AUTHOR("Steward_Fu");
MODULE_DESCRIPTION(TITLE);
MODULE_LICENSE("GPL");

// PE4, PE5, PE6
#define LED_G ((32 * 4) + 4)
#define LED_B ((32 * 4) + 5)
#define LED_R ((32 * 4) + 6)
 
static int g_blink_period=300;
static struct timer_list g_blink_timer;
 
void blink_handler(unsigned long unused)
{
  static int index=0;

  gpio_set_value(LED_R, 1);
  gpio_set_value(LED_G, 1);
  gpio_set_value(LED_B, 1);
  switch(index){
  case 0:
    gpio_set_value(LED_R, 0);
    break;
  case 1:
    gpio_set_value(LED_G, 0);
    break;
  case 2:
    gpio_set_value(LED_B, 0);
    break;
  default:
    break;
  }
  index+= 1;
  if(index > 3){
    index = 0;
  }
  mod_timer(&g_blink_timer, jiffies + msecs_to_jiffies(g_blink_period));
}
 
static int __init main_init(void)
{
  printk(TITLE);
  gpio_request(LED_R, "led_r"); gpio_direction_output(LED_R, 1); gpio_set_value(LED_R, 1);
  gpio_request(LED_G, "led_g"); gpio_direction_output(LED_G, 1); gpio_set_value(LED_G, 1);
  gpio_request(LED_B, "led_b"); gpio_direction_output(LED_B, 1); gpio_set_value(LED_B, 1);
  setup_timer(&g_blink_timer, blink_handler, 0);
  mod_timer(&g_blink_timer, jiffies + msecs_to_jiffies(g_blink_period));
  return 0;
}
  
static void __exit main_exit(void)
{
  del_timer(&g_blink_timer);
  gpio_free(LED_R);
  gpio_free(LED_G);
  gpio_free(LED_B);
  printk("Unload it!\n");
}
  
module_init(main_init);
module_exit(main_exit);

