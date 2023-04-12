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
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/backlight.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/io.h>
#include <asm/arch-suniv/cpu.h>
#include <asm/arch-suniv/gpio.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>

//#define DEBUG
#define MIYOO_KBD_GET_HOTKEY  _IOWR(0x100, 0, unsigned long)
#define MIYOO_KBD_SET_VER     _IOWR(0x101, 0, unsigned long)
#define MIYOO_KBD_LOCK_KEY    _IOWR(0x102, 0, unsigned long)
#define MIYOO_LAY_SET_VER     _IOWR(0x103, 0, unsigned long)
#define MIYOO_KBD_GET_VER     _IOWR(0x104, 0, unsigned long)
#define MIYOO_LAY_GET_VER     _IOWR(0x105, 0, unsigned long)

//Keypad type
// CONFIG_KEYBOARD_MIYOO_TYPE:
//   1 -> "BittBoy" meaning ABXY flipped southwest <-> northeast
//   2 -> "POCKETGOV1" meaning ABXY flipped southeast <-> northwest
//   3 -> "SUP M3" meaning AB flipped - matrix multiplexation for inputs.
//   4 -> "XYC Q8" meaning AB flipped & with echo and debounce code - mapping through GPIO reads except from HOME/START/VOLUME 
//   5 -> V90 meaning additional L2/R2 physical buttons
//   6 -> Q20 meaning Lfunction/Rfunction button (similarly to Q90)
//   7 -> HYBRID meaning BittBoy shell with PocketGo components.

/* 
 * Hardware map (as observed from the working code)
 *
 * | pad | define  | v1 v2      | v3 v4  | init_pullup? | init_as_in? |
 * |-----+---------+------------+--------+--------------+-------------|
 * | PA1 | IN_PA1  |            | R2     |              |             |
 * | PA3 | IN_B    | B (noUART) |        |              | Y (noUART)  |
 * | PC0 | IN_TB   | Y (noUART) |        | Y            | Y?          |
 * | PC1 | IN_L1   | L1         | L1     | Y            | Y           |
 * | PC2 | IN_R1   | R1         | R1     | Y            | Y           |
 * | PC3 | IN_R2   | Y          | L2     | Y            | Y           |
 * | PD0 | IN_A    | A          | SELECT |              |             |
 * | PD9 | IN_TA   | X          | Y      |              |             |
 * | PE0 | IN_L2   | B          | START  | Y            | Y           |
 * | PE1 | IN_MENU |            | R      | Y            |             |
 * | PE2 | IN_1    | matx       | up     | Y            |             |
 * | PE3 | IN_2    | matx       | down   | Y            |             |
 * | PE4 | IN_3    | matx       | left   | Y            |             |
 * | PE5 | IN_4    | matx       | right  | Y            |             |
 * | PE7 | OUT_1   | matx       | A      | Y            |             |
 * | PE8 | OUT_2   | matx       | B      | Y            |             |
 * | PE9 | OUT_3   | matx       | X      | Y            |             |
 * 
 * Notes:
 *  - init_pullup? shows if an internal pull-up resistor has been
 *    enabled in kbd_init
 *  - init_as_in? shows in the pin has been designated by input
 *    by direct writing to GPIO registers in kbd_init
 *  - (noUART) only happens when USE_UART is undefined (so by default NOT)
 *  - v1 and v2 are identical (exists just to match the graphics driver)
 *  - TA=X, TB=Y
 *  - "matx" is a matrix with these keys: dpad,R,start,select
 *  - v1&v2 code swaps 'R' and 'left' after scanning the matrix
 */

//Bittboy inputs
#define MY_UP     0x0008
#define MY_DOWN   0x0800
#define MY_LEFT   0x0080
#define MY_RIGHT  0x0040
#define MY_A      0x0100
#define MY_B      0x0020
#define MY_TA     0x0010
#define MY_TB     0x0002
#define MY_SELECT 0x0400
#define MY_START  0x0200
#define MY_R      0x0004
#define MY_L1     0x1000
#define MY_R1     0x2000
#define MY_L2     0x4000
#define MY_R2     0x8000
#define MY_L3     0x10000
#define MY_R3     0x20000

#define IN_L1   ((32 * 2) + 1)
#define IN_R1   ((32 * 2) + 2)
#define IN_L2   ((32 * 4) + 0)
#define IN_R2   ((32 * 2) + 3)
#define IN_R2_M3 ((32 * 0) + 2)
#define OUT_1   ((32 * 4) + 7)
#define OUT_2   ((32 * 4) + 8)
#define OUT_3   ((32 * 4) + 9)
#define OUT_4   ((32 * 4) + 10)
#define IN_L1_Q8 ((32 * 4) + 12)
#define IN_1    ((32 * 4) + 2)
#define IN_2    ((32 * 4) + 3)
#define IN_3    ((32 * 4) + 4)
#define IN_4    ((32 * 4) + 5)
#define IN_A    ((32 * 3) + 0)
#define IN_A_M3 ((32 * 0) + 0)
#define IN_TA   ((32 * 3) + 9)
#define IN_B    ((32 * 0) + 3)
#define IN_TB   ((32 * 2) + 0)
#define IN_MENU ((32 * 4) + 1)
#define IN_PC3  ((32 * 2) + 3)
#define IN_PA1  ((32 * 0) + 1)

#define USE_UART	1

#define NO_RAW	1
#define TP_INT_FIFOC		0x10
#define FIFO_FLUSH(x)		((x) << 4)

static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;
static struct input_dev *mydev;
static struct timer_list mytimer;
static int myperiod=30;

static struct backlight_device *bd;
static uint32_t miyoo_ver=1;
static uint32_t miyoo_layout=1;
static unsigned long hotkey=0;
static unsigned long lockkey=0;
static uint8_t *gpio;
static uint8_t *touch = NULL;
bool hotkey_mod_last=false;
bool hotkey_actioned=false;
bool hotkey_down=false;
bool non_hotkey_first=false;
bool non_hotkey_menu=false;
module_param(miyoo_ver,uint,0660);
module_param(miyoo_layout,uint,0660);

static int do_input_request(uint32_t pin, const char*name)
{
  if(gpio_request(pin, name) < 0){
    printk("failed to request gpio: %s\n", name);
    return -1;
  }
  gpio_direction_input(pin);
  return 0;
}

static int do_output_request(uint32_t pin, const char* name)
{
  if(gpio_request(pin, name) < 0){
    printk("failed to request gpio: %s\n", name);
    return -1;
  }
  gpio_direction_output(pin, 1);
  return 0;
}

#if defined(DEBUG)
static void print_key(uint32_t val, uint8_t is_pressed)
{
  uint32_t i;
  uint32_t map_val[] = {MY_UP, MY_DOWN, MY_LEFT, MY_RIGHT, MY_A, MY_B, MY_TA, MY_TB, MY_SELECT, MY_START, MY_R, MY_L1, MY_R1, MY_L2, MY_R2, MY_L3, MY_R3, -1};
  char* map_key[] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "SELECT", "START", "MENU", "L1", "R1", "L2", "R2", "L3", "R3"};

  for(i=0; map_val[i]!=-1; i++){
    if(map_val[i] == val){
      if(is_pressed){
        printk("%s\n", map_key[i]);
      } 
      break;
    }
  }
}
#endif

static void report_key(uint32_t btn, uint32_t mask, uint8_t key)
{
  static uint32_t btn_pressed=0;
  static uint32_t btn_released=0xffff;
 
  if(btn & mask){
    btn_released&= ~mask;
    if((btn_pressed & mask) == 0){
      btn_pressed|= mask;
      input_report_key(mydev, key, 1);
      #if defined(DEBUG)
        print_key(btn & mask, 1);
      #endif
    }
  }
  else{
    btn_pressed&= ~mask;
    if((btn_released & mask) == 0){
      btn_released|= mask;
      input_report_key(mydev, key, 0);
      #if defined(DEBUG)
        print_key(btn & mask, 0);
      #endif
    }
  }
}

static void scan_handler(unsigned long unused)
{
  static uint32_t pre=0;
  uint32_t scan=0, val=0, debounce=0;
  static uint32_t touchRead=0, touchReadPrev=0;
  extern void MIYOO_INCREASE_VOLUME(void);
  extern void MIYOO_DECREASE_VOLUME(void);

  switch(miyoo_ver){
      case 1:
      for(scan=0; scan<3; scan++){
          gpio_set_value(OUT_1, 1);
          gpio_set_value(OUT_2, 1);
          gpio_set_value(OUT_3, 1);
          gpio_direction_input(OUT_1);
          gpio_direction_input(OUT_2);
          gpio_direction_input(OUT_3);
          switch(scan){
          case 0:
            gpio_direction_output(OUT_1, 0);
            break;
          case 1:
            gpio_direction_output(OUT_2, 0);
            break;
	  case 2: case 5: case 6: case 7:
            gpio_direction_output(OUT_3, 0);
            break;
          }
          if (gpio_get_value(IN_1) == 0){
            val|= ((1 << 0) << (scan << 2));
          }
          if (gpio_get_value(IN_2) == 0){
            val|= ((1 << 1) << (scan << 2));
          }
          if (gpio_get_value(IN_3) == 0){
            val|= ((1 << 2) << (scan << 2));
          }
          if (gpio_get_value(IN_4) == 0){
            val|= ((1 << 3) << (scan << 2));
          }
        }
        if (gpio_get_value(IN_L1) == 0){
          val|= MY_L1;
        }
        if (gpio_get_value(IN_R1) == 0){
          val|= MY_R1;
        }
        if (gpio_get_value(IN_L2) == 0){
          //val|= MY_L2;
          val|= MY_B;
        }
        if (gpio_get_value(IN_R2) == 0){
          //val|= MY_R2;
          val|= MY_TB;
        }
        if (gpio_get_value(IN_A) == 0){
          val|= MY_A;
        }
        if (gpio_get_value(IN_TA) == 0){
          val|= MY_TA;
        }
      #if !defined(USE_UART)
        if (gpio_get_value(IN_B) == 0){
          val|= MY_B;
        }
        if (gpio_get_value(IN_TB) == 0){
          val|= MY_TB;
        }
      #endif
        #if !defined(RAW)
        if ((miyoo_ver <= 2 || miyoo_ver == 5 || miyoo_ver == 6)  && val & MY_R) {
          if (! (val & MY_LEFT) ) {
            val&= ~MY_R;
            val|= MY_LEFT;
          }
        } else if ((miyoo_ver <= 2 || miyoo_ver == 5 || miyoo_ver == 6) && val & MY_LEFT) {
          if (! (val & MY_R) ) {
            val&= ~MY_LEFT;
            val|= MY_R;
          }
        }
        #endif
    break;
      case 2: case 5: case 7:
          gpio_direction_input(IN_1);
          gpio_direction_input(IN_2);
          gpio_direction_input(IN_3);
          gpio_direction_input(IN_4);
          gpio_direction_input(OUT_1);
          gpio_direction_input(OUT_2);
          gpio_direction_input(OUT_3);
          gpio_direction_input(IN_A);
          gpio_direction_input(IN_TA);
          gpio_direction_input(IN_PC3);
          gpio_direction_input(IN_PA1);
          gpio_direction_input(IN_L1);
          gpio_direction_input(IN_R1);
          gpio_direction_input(IN_MENU);

          if(gpio_get_value(IN_1) == 0){
              val|= MY_UP;
          }
          if(gpio_get_value(IN_2) == 0){
              val|= MY_DOWN;
          }
          if(gpio_get_value(IN_3) == 0){
              val|= MY_LEFT;
          }
          if(gpio_get_value(IN_4) == 0){
              val|= MY_RIGHT;
          }
          if(gpio_get_value(OUT_1) == 0){
              val|= MY_A;
          }
          if(gpio_get_value(OUT_2) == 0){
              val|= MY_B;
          }
          if(gpio_get_value(OUT_3) == 0){
              val|= MY_TA;
          }
          if(gpio_get_value(IN_TA) == 0){
              val|= MY_TB;
          }
          if(gpio_get_value(IN_A) == 0){
              val|= MY_SELECT;
          }
          if(gpio_get_value(IN_L2) == 0){
              val|= MY_START;
          }
          if(gpio_get_value(IN_L1) == 0){
              val|= MY_L1;
          }
          if(gpio_get_value(IN_R1) == 0){
              val|= MY_R1;
          }
          if(gpio_get_value(IN_PC3) == 0){
              val|= MY_L2;
          }
          if(gpio_get_value(IN_PA1) == 0){
              val|= MY_R2;
          }
          if(gpio_get_value(IN_MENU) == 0){
              val|= MY_R;
          }
          break;
      case 3:
          gpio_direction_input(IN_4);
          gpio_direction_input(IN_A_M3);
          gpio_direction_input(IN_PA1);
          gpio_direction_output(IN_3,1);
          if(gpio_get_value(IN_1) == 1){
              val|= MY_UP;
          }
          if(gpio_get_value(IN_2) == 1){
              val|= MY_LEFT;
          }
          if(gpio_get_value(IN_A_M3) == 1){
              val|= MY_TB;
          }
          if(gpio_get_value(IN_PA1) == 1){
              val|= MY_B;
          }

          gpio_direction_input(IN_3);
          gpio_direction_output(IN_4,1);
          if(gpio_get_value(IN_1) == 1){
              val|= MY_DOWN;
          }
          if(gpio_get_value(IN_2) == 1){
              val|= MY_RIGHT;
          }
          if(gpio_get_value(IN_A_M3) == 1){
              val|= MY_A;
          }
          if(gpio_get_value(IN_PA1) == 1){
              val|= MY_TA;
          }

          gpio_direction_input(IN_4);
          gpio_direction_output(OUT_2,1);
          gpio_direction_output(OUT_3,1);
          if(gpio_get_value(IN_PA1) == 1){
              val|= MY_R;
          }
          if(gpio_get_value(IN_A_M3) == 1){
              val|= MY_SELECT;
          }
          if(gpio_get_value(IN_2) == 1){
              val|= MY_L1;
          }
          if(gpio_get_value(IN_1) == 1 && gpio_get_value(IN_2) == 0){
              val|= MY_START;
          }


          gpio_direction_output(OUT_2, 0);
          gpio_direction_output(OUT_3, 0);
          gpio_direction_output(IN_4,1);
          gpio_direction_output(IN_A_M3,1);
          gpio_direction_input(IN_R2_M3);
          if(gpio_get_value(IN_R2_M3) == 1){
              val|= MY_R1;
          }
          break;
      case 4:
          gpio_direction_input(OUT_1);
          gpio_direction_input(OUT_2);
          gpio_direction_input(OUT_3);
          gpio_direction_input(OUT_4);
          gpio_direction_input(IN_1);
          gpio_direction_input(IN_2);
          gpio_direction_input(IN_A);
          gpio_direction_input(IN_A_M3);
          gpio_direction_input(IN_TA);
          gpio_direction_input(IN_L1_Q8);

          if(gpio_get_value(IN_1) == 0){
              val|= MY_A;
          }
          if(gpio_get_value(IN_A_M3) == 0){
              val|= MY_TA;
          }
          if(gpio_get_value(IN_2) == 0){
              val|= MY_TB;
          }
          if(gpio_get_value(IN_A) == 0){
              val|= MY_B;
          }
          if(gpio_get_value(OUT_1) == 0){
              val|= MY_RIGHT;
          }
          if(gpio_get_value(OUT_2) == 0){
              val|= MY_LEFT;
          }
          if(gpio_get_value(OUT_3) == 0){
              val|= MY_DOWN;
          }
          if(gpio_get_value(OUT_4) == 0){
              val|= MY_UP;
          }
          if(gpio_get_value(IN_TA) == 0){
              val|= MY_R1;
          }
          if(gpio_get_value(IN_L1_Q8) == 0){
              val|= MY_L1;
          }
          touchRead = (readl(touch + 0x24) >> 4) & 0xff;
          debounce = abs(touchRead - touchReadPrev);
          writel( FIFO_FLUSH(1), touch + TP_INT_FIFOC);
          if(debounce <= 3){
            if((touchRead < 0x30) && (touchRead >= 0x00)){
              val|= MY_START;
            }
            else if((touchRead < 0x60) && (touchRead >= 0x30)){
              val|= MY_SELECT;
            }
            else if((touchRead < 0xEF) && (touchRead >= 0x60)){
              val|= MY_R;
            }
          }
          touchReadPrev = touchRead;
          break;
      case 6:
          gpio_direction_input(IN_1);
          gpio_direction_input(IN_2);
          gpio_direction_input(IN_3);
          gpio_direction_input(IN_4);
          gpio_direction_input(OUT_1);
          gpio_direction_input(OUT_2);
          gpio_direction_input(OUT_3);
          gpio_direction_input(IN_A);
          gpio_direction_input(IN_TA);
          gpio_direction_input(IN_PC3);
          gpio_direction_input(IN_PA1);
          gpio_direction_input(IN_L1);
          gpio_direction_input(IN_R1);
          gpio_direction_input(IN_MENU);

          if(gpio_get_value(IN_1) == 0){
              val|= MY_UP;
          }
          if(gpio_get_value(IN_2) == 0){
              val|= MY_DOWN;
          }
          if(gpio_get_value(IN_3) == 0){
              val|= MY_LEFT;
          }
          if(gpio_get_value(IN_4) == 0){
              val|= MY_RIGHT;
          }
          if(gpio_get_value(OUT_1) == 0){
              val|= MY_A;
          }
          if(gpio_get_value(OUT_2) == 0){
              val|= MY_B;
          }
          if(gpio_get_value(OUT_3) == 0){
              val|= MY_TA;
          }
          if(gpio_get_value(IN_TA) == 0){
              val|= MY_TB;
          }
          if(gpio_get_value(IN_A) == 0){
              val|= MY_SELECT;
          }
          if(gpio_get_value(IN_L2) == 0){
              val|= MY_START;
          }
          if(gpio_get_value(IN_L1) == 0){
              val|= MY_L1;
          }
          if(gpio_get_value(IN_R1) == 0){
              val|= MY_R1;
          }
          if(gpio_get_value(IN_PC3) == 0){
              val|= MY_R2;
          }
          if(gpio_get_value(IN_MENU) == 0){
              val|= MY_L3;
          }
          if(gpio_get_value(IN_PA1) == 0){
              val|= MY_R;
          }
          break; 
  }

  if(lockkey){
    val = val & MY_R ? MY_R : 0;
  }

#if !defined(RAW)
  if(miyoo_ver == 2)  {
    if((val & MY_R) && (val & MY_B)) {
      val&= ~MY_R;
      val&= ~MY_B;
      val|= MY_L3;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_TB)) {
      val&= ~MY_R;
      val&= ~MY_TB;
      val|= MY_R3;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_L1)) {
      val&= ~MY_R;
      val&= ~MY_L1;
      val|= MY_L2;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_R1)) {
      val&= ~MY_R;
      val&= ~MY_R1;
      val|= MY_R2;
      hotkey_actioned = true;
    }
  } else if(miyoo_ver == 3) {
    if((val & MY_R) && (val & MY_L1)) {
      val&= ~MY_R;
      val&= ~MY_L1;
      val|= MY_L2;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_R1)) {
      val&= ~MY_R;
      val&= ~MY_R1;
      val|= MY_R2;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_TB)) {
      val&= ~MY_R;
      val&= ~MY_TB;
      val|= MY_R3;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_B)) {
      val&= ~MY_R;
      val&= ~MY_B;
      val|= MY_L3;
      hotkey_actioned = true;
    }
  } else if(miyoo_ver == 4) {
    if((val & MY_R) && (val & MY_A)) {
      if(!hotkey_down) {
        static char * shutdown8_argv[] = { "/bin/sh", "-c", "/bin/kill -9 $(/bin/ps -al | /bin/grep \"/mnt/\")" , NULL };
        call_usermodehelper(shutdown8_argv[0], shutdown8_argv, NULL, UMH_NO_WAIT);
        hotkey_down = true;
      }
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_TA)) {
      if(!hotkey_down) {
        static char * screenshot_argv[] = {"/bin/sh", "-c", "/mnt/apps/fbgrab/screenshot.sh", NULL};
        call_usermodehelper(screenshot_argv[0], screenshot_argv, NULL, UMH_NO_WAIT);
        hotkey_down = true;
      }
      hotkey_actioned = true;   
    }
    if((val & MY_R) && (val & MY_B)) {
      val&= ~MY_R;
      val&= ~MY_B;
      val|= MY_L3;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_TB)) {
      val&= ~MY_R;
      val&= ~MY_TB;
      val|= MY_R3;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_L1)) {
      val&= ~MY_R;
      val&= ~MY_L1;
      val|= MY_L2;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_R1)) {
      val&= ~MY_R;
      val&= ~MY_R1;
      val|= MY_R2;
      hotkey_actioned = true;
    }
  } else if(miyoo_ver == 5) {
    if((val & MY_R) && (val & MY_L2)) {
		if(!hotkey_down) {
			static char * shutdown_argv[] = { "/bin/sh", "-c", "/bin/kill -2 $(/bin/ps -al | /bin/grep \"/mnt/\")" , NULL };
			static char * shutdown2_argv[] = { "/bin/sh", "-c", "/bin/kill -9 $(/bin/ps -al | /bin/grep \"/mnt/hard/\")" , NULL };
			call_usermodehelper(shutdown_argv[0], shutdown_argv, NULL, UMH_NO_WAIT);
			call_usermodehelper(shutdown2_argv[0], shutdown2_argv, NULL, UMH_NO_WAIT);
			hotkey_down = true;
      }
			hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_R2)) {
       		if(!hotkey_down) {
			static char * shutdown3_argv[] = { "/bin/sh", "-c", "/bin/kill -9 $(/bin/ps -al | /bin/grep \"/mnt/\" | /bin/grep -v \"/kernel/\" | /usr/bin/tr -s [:blank:] | /usr/bin/cut -d \" \" -f 2) ; /bin/sleep 0.1 ; /bin/sync ; /bin/swapoff -a ; /sbin/poweroff",  NULL };
			call_usermodehelper(shutdown3_argv[0], shutdown3_argv, NULL, UMH_NO_WAIT);
			hotkey_down = true;
      }
			hotkey_actioned = true;	  
    }
	if((val & MY_R) && (val & MY_L1)) {
      val&= ~MY_R;
      val&= ~MY_L1;
      val|= MY_L3;
      hotkey_actioned = true; 
	}
    if((val & MY_R) && (val & MY_R1)) {
      val&= ~MY_R;
      val&= ~MY_R1;
      val|= MY_R3;
      hotkey_actioned = true;
	}
  } else if(miyoo_ver == 6) {
    if((val & MY_R) && (val & MY_L1)) {
      val&= ~MY_R;
      val&= ~MY_L1;
      val|= MY_L2;
      hotkey_actioned = true; 
	}
    if((val & MY_R) && (val & MY_R1)) {
      val&= ~MY_R;
      val&= ~MY_R1;
      val|= MY_R2;
      hotkey_actioned = true;
	}
    if((val & MY_R) && (val & MY_L3)) {
      val&= ~MY_R;
      val&= ~MY_L3;
      val|= MY_R3;
      hotkey_actioned = true;
	}
  } else {
    if((val & MY_R) && (val & MY_B)) {
      val&= ~MY_R;
      val&= ~MY_B;
      val|= MY_L1;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_A)) {
      val&= ~MY_R;
      val&= ~MY_A;
      val|= MY_R1;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_TB)) {
      val&= ~MY_R;
      val&= ~MY_TB;
      val|= MY_L2;
      hotkey_actioned = true;
    }
    if((val & MY_R) && (val & MY_TA)) {
      val&= ~MY_R;
      val&= ~MY_TA;
      val|= MY_R2;
      hotkey_actioned = true;
    }
  }

  if(val > 0 && !(val & MY_R)) {
	  non_hotkey_first = true;
  }

  if((val & MY_R) && non_hotkey_first) {
    non_hotkey_menu = true;
  }

  if(non_hotkey_menu) {
    if(val & MY_R) {
	    val&= ~MY_R;
    } else if(!hotkey_actioned) {
      val|= MY_R;
      non_hotkey_menu = false;
    } else {
      hotkey_actioned = false;
      non_hotkey_menu = false;
    }
  }

  if(val & MY_R && !non_hotkey_first) {
	  if((val & MY_R) && (val & MY_B)){
      if(miyoo_ver == 5 || miyoo_ver == 6)  {
			  hotkey_actioned = true;
	  	  hotkey = hotkey == 0 ? 3 : hotkey;
      }
	 	}
	 	else if((val & MY_R) && (val & MY_A)){
      if(miyoo_ver == 2 || miyoo_ver == 3 || miyoo_ver == 5 || miyoo_ver == 6)  {
	  	  hotkey_actioned = true;
	  	  hotkey = hotkey == 0 ? 4 : hotkey;
      }
	 	}
		else if((val & MY_R) && (val & MY_TB)){
      if(miyoo_ver == 5 || miyoo_ver == 6)  {
        hotkey_actioned = true;
        hotkey = hotkey == 0 ? 1 : hotkey;
      }
		}
		else if((val & MY_R) && (val & MY_TA)){
      if(miyoo_ver == 2 || miyoo_ver == 3 || miyoo_ver == 5 || miyoo_ver == 6)  {
        hotkey_actioned = true;
        hotkey = hotkey == 0 ? 2 : hotkey;
      }
		}
		else if((val & MY_R) && (val & MY_UP)){
      if(!hotkey_down) {
        MIYOO_INCREASE_VOLUME();
        hotkey_down = true;
      }
			hotkey_actioned = true;
			//hotkey = hotkey == 0 ? 5 : hotkey;
		}
		else if((val & MY_R) && (val & MY_DOWN)){
      if(!hotkey_down) {
        MIYOO_DECREASE_VOLUME();
        hotkey_down = true;
      }
			hotkey_actioned = true;
			//hotkey = hotkey == 0 ? 6 : hotkey;
		}
		else if((val & MY_R) && (val & MY_LEFT)){
      if(!hotkey_down) {
        bd = backlight_device_get_by_type(BACKLIGHT_RAW);
        if(bd->props.brightness > 1) {
          backlight_device_set_brightness(bd, bd->props.brightness - 1);
        }
        hotkey_down = true;
      }
			hotkey_actioned = true;
			//hotkey = hotkey == 0 ? 7 : hotkey;
		}
	 else if((val & MY_R) && (val & MY_RIGHT)){
      if(!hotkey_down) {
        bd = backlight_device_get_by_type(BACKLIGHT_RAW);
        if(bd->props.brightness < 2) {
          backlight_device_set_brightness(bd, 3);
        } else if (bd->props.brightness < 11) {
          backlight_device_set_brightness(bd, bd->props.brightness + 1);
        }
        hotkey_down = true;
      }
			hotkey_actioned = true;
			//hotkey = hotkey == 0 ? 8 : hotkey;
		}
		else if((val & MY_R) && (val & MY_SELECT)){
      if(!hotkey_down) {
	static char * shutdown4_argv[] = { "/bin/sh", "-c", "/bin/kill -9 $(/bin/ps -al | /bin/grep \"/mnt/\")" , NULL };
	call_usermodehelper(shutdown4_argv[0], shutdown4_argv, NULL, UMH_NO_WAIT);
        hotkey_down = true;
      }
			hotkey_actioned = true;
			//hotkey = hotkey == 0 ? 9 : hotkey;
		}
		else if((val & MY_R) && (val & MY_START)){
      if(!hotkey_down) {
        static char * screenshot_argv[] = {"/bin/sh", "-c", "/mnt/apps/fbgrab/screenshot.sh", NULL};
        call_usermodehelper(screenshot_argv[0], screenshot_argv, NULL, UMH_NO_WAIT);
        hotkey_down = true;
      }
			hotkey_actioned = true;
      //hotkey = hotkey == 0 ? 10 : hotkey;
		}
    hotkey_mod_last = true;

  } else if(pre != val || hotkey_mod_last){
    if (hotkey_mod_last) {
      if (!hotkey_actioned) {
        val |= MY_R;
      } else {
        val &= ~(MY_R);
        hotkey_actioned = false;
      }
    }
#endif
#if defined(RAW)
  if(pre != val) {
#endif
    pre = val;

    report_key(pre, MY_UP, KEY_UP);
    report_key(pre, MY_DOWN, KEY_DOWN);
    report_key(pre, MY_LEFT, KEY_LEFT);
    report_key(pre, MY_RIGHT, KEY_RIGHT);
    report_key(pre, MY_R, KEY_RIGHTCTRL); // "HOME/RESET" button
    switch (miyoo_layout) {
        case 1:
            //MiyooCFW 2.0 default layout (as seen on PocketGO)
            report_key(pre, MY_A, KEY_LEFTCTRL); // "B" - bottom face button
            report_key(pre, MY_B, KEY_SPACE); // "Y" - left face button
            report_key(pre, MY_TA, KEY_LEFTALT); // "A" - right face button
            report_key(pre, MY_TB, KEY_LEFTSHIFT); // "X" - upper face button
            break;
        case 2:
            //CFW 1.3.3 legacy layout (swapped A-B & Y-X)
            report_key(pre, MY_A, KEY_LEFTALT);
            report_key(pre, MY_B, KEY_LEFTSHIFT);
            report_key(pre, MY_TA, KEY_LEFTCTRL);
            report_key(pre, MY_TB, KEY_SPACE);
            break;
        case 3:
            //Bittboy layout (moved A->B B->Y & Y->X)
            report_key(pre, MY_A, KEY_LEFTALT);
            report_key(pre, MY_B, KEY_LEFTCTRL);
            report_key(pre, MY_TA, KEY_LEFTSHIFT);
            report_key(pre, MY_TB, KEY_SPACE);
            break;
        case 4:
            //SUP M3 & XYC Q8 layout (swapped A-B )
            report_key(pre, MY_A, KEY_LEFTALT);
            report_key(pre, MY_B, KEY_SPACE);
            report_key(pre, MY_TA, KEY_LEFTCTRL);
            report_key(pre, MY_TB, KEY_LEFTSHIFT); 
            break;
        case 5:
            //Custom I (swapped Y-X)
            report_key(pre, MY_A, KEY_LEFTCTRL);
            report_key(pre, MY_B, KEY_LEFTSHIFT);
            report_key(pre, MY_TA, KEY_LEFTALT);
            report_key(pre, MY_TB, KEY_SPACE);
            break;
        case 6:
            //Custom II (moved A->X & Y->A & X->Y)
            report_key(pre, MY_A, KEY_LEFTCTRL);
            report_key(pre, MY_B, KEY_LEFTSHIFT);
            report_key(pre, MY_TA, KEY_SPACE);
            report_key(pre, MY_TB, KEY_LEFTALT);
            break;
    }

    report_key(pre, MY_SELECT, KEY_ESC);
    report_key(pre, MY_START, KEY_ENTER);

    report_key(pre, MY_L1, KEY_TAB);
    report_key(pre, MY_R1, KEY_BACKSPACE);
    report_key(pre, MY_L2, KEY_PAGEUP);
    report_key(pre, MY_R2, KEY_PAGEDOWN);
    report_key(pre, MY_L3, KEY_RIGHTALT);
    report_key(pre, MY_R3, KEY_RIGHTSHIFT);
	
    input_sync(mydev);
    hotkey_mod_last = false;
  }

  mod_timer(&mytimer, jiffies + msecs_to_jiffies(myperiod));

#if !defined(RAW)
  if(!(val & MY_R)) {
    hotkey_mod_last = false;
    hotkey_down = false;
  }

  if((val & MY_R) && ! ( (val & MY_DOWN) || (val & MY_UP) || (val & MY_LEFT) || (val & MY_RIGHT) || (val & MY_SELECT) ) ) {
    hotkey_down = false;
  }

  if(val == 0 && non_hotkey_first) {
	  non_hotkey_first = false;
  }
#endif
}

static int myopen(struct inode *inode, struct file *file)
{
  return 0;
}

static int myclose(struct inode *inode, struct file *file)
{
  return 0;
}

static long myioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  int ret;

  switch(cmd){
  case MIYOO_KBD_GET_HOTKEY:
    ret = copy_to_user((void*)arg, &hotkey, sizeof(unsigned long));
    hotkey = 0;
    break;
  case MIYOO_KBD_SET_VER:
    miyoo_ver = arg;
#if defined(DEBUG)
    printk("miyoo keypad version config as v%d\n", (int)miyoo_ver);
#endif
    break;
  case MIYOO_KBD_GET_VER:
    ret = copy_to_user((void*)arg, &miyoo_ver, sizeof(unsigned long));
    break;
  case MIYOO_LAY_SET_VER:
    miyoo_layout = arg;
#if defined(DEBUG)  
    printk("miyoo keypad layout config as v%d\n", (int)miyoo_layout);
#endif
    break;
  case MIYOO_LAY_GET_VER:
    ret = copy_to_user((void*)arg, &miyoo_layout, sizeof(unsigned long));
    break;
  case MIYOO_KBD_LOCK_KEY:
    lockkey = arg;
    break;
  }
  return 0;
}

static const struct file_operations myfops = {
  .owner = THIS_MODULE,
  .open = myopen,
  .release = myclose,
  .unlocked_ioctl = myioctl,
};

static int __init kbd_init(void)
{
  uint32_t ret;

  // initialise some of the GPIO pins directly by writing to F1C100S registers
  // datasheet: https://linux-sunxi.org/images/8/85/Allwinner_F1C600_User_Manual_V1.0.pdf
  // pages 116 onwards
  gpio = ioremap(0x01c20800, 4096);      // PIO block
  ret = readl(gpio + (2 * 0x24 + 0x00)); // PC_CFG0
  ret&= 0xffff0000;                      // set (PC0?) PC1 PC2 PC3 as inputs
  writel(ret, gpio + (2 * 0x24 + 0x00)); // (somewhat dirty, writes into some reserved)

  ret = readl(gpio + (2 * 0x24 + 0x1c)); // PC_PULL0
  //ret&= 0xffffff00;
  //ret|= 0x00000055;
  ret = 0x55555555;                      // enable pull-ups on PC0 - PC3
  writel(ret, gpio + (2 * 0x24 + 0x1c)); // (dirty again)

  ret = readl(gpio + (4 * 0x24 + 0x00)); // PE_CFG0
  if (miyoo_ver == 3) {
      ret&= 0x0f00000f;
  } else {
      ret&= 0xfffffff0;                      // set PE0 as input
  }
  writel(ret, gpio + (4 * 0x24 + 0x00));

  ret = readl(gpio + (4 * 0x24 + 0x1c)); // PE_PULL0
  //ret&= 0xffffffff0;
  //ret|= 0x000000001;
  ret = 0x55555555;                      // pull-ups on PE0 - PE12
  writel(ret, gpio + (4 * 0x24 + 0x1c));
    if (miyoo_ver == 3) {
        ret = 0x55555555;                      // pull-ups on PA0 - PA12
        writel(ret, gpio + (0 * 0x24 + 0x1c));
    }
    if (miyoo_ver == 4) {
        touch = (uint8_t *) ioremap(0x01c24800, 4096);
        ret = readl(gpio + (32 * 0) + 0);
        ret &= 0xffffff0f;
        ret |= 0x00000020;
        writel(ret, gpio + (32 * 0) + 0);

        writel((3 << 20) | (1 << 22) | (1 << 0), touch + 0x00);
        writel((1 << 5) | (1 << 4) | (1 << 1), touch + 0x04);
    }

#if !defined(USE_UART)
  ret = readl(gpio + (0 * 0x24 + 0x00)); // PA_CFG0
  ret&= 0xffff0fff;                      // set PA3 as input
  writel(ret, gpio + (0 * 0x24 + 0x00));
#endif

  do_input_request(IN_L1, 	"gpio_l1");
  do_input_request(IN_R1, 	"gpio_r1");
  do_input_request(IN_L2, 	"gpio_l2");
  do_input_request(IN_1, 		"gpio_pe2");
  do_input_request(IN_2, 		"gpio_pe3");
  do_input_request(IN_3, 		"gpio_pe4");
  do_input_request(IN_4, 		"gpio_pe5");
    if (miyoo_ver != 3) {
        do_input_request(IN_A, "gpio_a");
        do_input_request(IN_R2, "gpio_r2");
        do_output_request(OUT_1, "gpio_pe7");
        } else {
        do_input_request(IN_A_M3, 	"gpio_a");
        do_input_request(IN_R2_M3, 	"gpio_r2");
    }

  do_input_request(IN_TA, 	"gpio_ta");
#if !defined(USE_UART)
  do_input_request(IN_B, 		"gpio_b");
  do_input_request(IN_TB, 	"gpio_tb");
#endif
  do_output_request(OUT_2, "gpio_pe8");
  do_output_request(OUT_3, "gpio_pe9");
  mydev = input_allocate_device();
  set_bit(EV_KEY,mydev-> evbit);
  set_bit(KEY_UP, mydev->keybit);
  set_bit(KEY_DOWN, mydev->keybit);
  set_bit(KEY_LEFT, mydev->keybit);
  set_bit(KEY_RIGHT, mydev->keybit);
  set_bit(KEY_ENTER, mydev->keybit);
  set_bit(KEY_ESC, mydev->keybit);
  set_bit(KEY_LEFTCTRL, mydev->keybit);
  set_bit(KEY_LEFTALT, mydev->keybit);
  set_bit(KEY_SPACE, mydev->keybit);
  set_bit(KEY_LEFTSHIFT, mydev->keybit);
  set_bit(KEY_TAB, mydev->keybit);
  set_bit(KEY_BACKSPACE, mydev->keybit);
  set_bit(KEY_RIGHTCTRL, mydev->keybit);
  set_bit(KEY_RIGHTALT, mydev->keybit);
  set_bit(KEY_RIGHTSHIFT, mydev->keybit);  
  set_bit(KEY_PAGEUP, mydev->keybit);
  set_bit(KEY_PAGEDOWN, mydev->keybit);
  mydev->name = "miyoo_keypad";
  mydev->id.bustype = BUS_HOST;
  ret = input_register_device(mydev);
 
  alloc_chrdev_region(&major, 0, 1, "miyoo_kbd");
  myclass = class_create(THIS_MODULE, "miyoo_kbd");
  device_create(myclass, NULL, major, NULL, "miyoo_kbd");
  cdev_init(&mycdev, &myfops);
  cdev_add(&mycdev, major, 1);
  
	setup_timer(&mytimer, scan_handler, 0);
  mod_timer(&mytimer, jiffies + msecs_to_jiffies(myperiod));
  return 0;
}
  
static void __exit kbd_exit(void)
{
  input_unregister_device(mydev);
  del_timer(&mytimer);

  device_destroy(myclass, major);
  cdev_del(&mycdev);
  class_destroy(myclass);
  unregister_chrdev_region(major, 1);
  iounmap(gpio);
}
  
module_init(kbd_init);
module_exit(kbd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_DESCRIPTION("Keyboard Driver for Miyoo handheld");
 
