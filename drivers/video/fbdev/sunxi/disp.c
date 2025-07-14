#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/uaccess.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/clk-provider.h>
#include <video/of_display_timing.h>
#include <linux/compiler.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <sound/pcm.h>
#include <sound/dmaengine_pcm.h>
#include <asm/arch-suniv/cpu.h>
#include <asm/arch-suniv/debe.h>
#include <asm/arch-suniv/common.h>
#include <asm/arch-suniv/tve.h>
#include <linux/miscdevice.h>

#define DISP_CMD_VERSION               0x00
#define DISP_CMD_LAYER_REQUEST         0x40
#define DISP_CMD_LAYER_RELEASE         0x41
#define DISP_CMD_LAYER_OPEN            0x42
#define DISP_CMD_LAYER_CLOSE           0x43
#define DISP_CMD_LAYER_SET_FB          0x44
#define DISP_CMD_LAYER_SET_SRC_WINDOW  0x46
#define DISP_CMD_LAYER_SET_SCN_WINDOW  0x48
#define DISP_CMD_LAYER_SET_PARA        0x4a
#define DISP_CMD_LAYER_GET_PARA        0x4b

struct __disp_fb_t {
	uint32_t addr[3];
	uint32_t size_width;
	uint32_t size_height;
	uint32_t format;
	uint32_t seq;
	uint32_t mode;
};

struct disp_rect {
    int x;
    int y;
    int width;
    int height;
};

struct suniv_iomm {
	uint8_t *debe;
	uint8_t *defe;
};



static struct suniv_iomm iomm={0};
struct __disp_fb_t fb;
uint16_t i;
uint32_t val;


static const uint32_t csc_tab[192] =
	{
		//Y/G   Y/G     Y/G     Y/G     U/R     U/R     U/R     U/R     V/B     V/B     V/B     V/B
		//bt601
		0x04a8, 0x1e70, 0x1cbf, 0x0878, 0x04a8, 0x0000, 0x0662, 0x3211, 0x04a8, 0x0812, 0x0000, 0x2eb1, //yuv2rgb
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
		0x0204, 0x0107, 0x0064, 0x0100, 0x1ed6, 0x1f68, 0x01c2, 0x0800, 0x1e87, 0x01c2, 0x1fb7, 0x0800, //rgb2yuv

		//bt709
		0x04a8, 0x1f26, 0x1ddd, 0x04d0, 0x04a8, 0x0000, 0x072c, 0x307e, 0x04a8, 0x0876, 0x0000, 0x2dea, //yuv2rgb
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
		0x0275, 0x00bb, 0x003f, 0x0100, 0x1ea6, 0x1f99, 0x01c2, 0x0800, 0x1e67, 0x01c2, 0x1fd7, 0x0800, //rgb2yuv

		//DISP_ YCC
		0x0400, 0x1e9e, 0x1d24, 0x087b, 0x0400, 0x0000, 0x059c, 0x34c8, 0x0400, 0x0716, 0x0000, 0x31d5, //yuv2rgb
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
		0x0259, 0x0132, 0x0075, 0x0000, 0x1ead, 0x1f53, 0x0200, 0x0800, 0x1e54, 0x0200, 0x1fac, 0x0800, //rgb2yuv

		//xvYCC
		0x04a8, 0x1f26, 0x1ddd, 0x04d0, 0x04a8, 0x0000, 0x072c, 0x307e, 0x04a8, 0x0876, 0x0000, 0x2dea, //yuv2rgb
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //yuv2yuv
		0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0400, 0x0000, //rgb2rgb
		0x0275, 0x00bb, 0x003f, 0x0100, 0x1ea6, 0x1f99, 0x01c2, 0x0800, 0x1e67, 0x01c2, 0x1fd7, 0x0800, //rgb2yuv
	};

void debe_layer_set_mode(uint8_t layer, debe_color_mode_e mode)
{
	if (layer > 3)
		return;
	if (mode == DEBE_MODE_DEFE_VIDEO)
	{
		val = readl(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4) & ~(3 << 1);
		writel(val | (1 << 1), iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4);

	}
	else if (mode == DEBE_MODE_YUV)
	{
		val = readl(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4) & ~(3 << 1);
		writel(val | (1 << 2),iomm.debe + DEBE_LAY0_ATT_CTRL_REG0 + layer*4);
	}
}

static inline uint32_t calc_scaling_factor(int input, int output)
{
    uint32_t ratio = (input << 16) / output;
    uint32_t int_part = (ratio >> 16) & 0xFF;
    uint32_t frac_part = ratio & 0xFFFF;
    return (int_part << 16) | frac_part;
}

static long disp_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	uint32_t
	__user * uarg = (uint32_t
	__user *)arg;
	uint32_t tmp[4];

	if (copy_from_user(tmp, uarg, sizeof(tmp)))
		return -EFAULT;

	switch (cmd) {

		case DISP_CMD_VERSION:
			tmp[0] = 0x202406;
			if (copy_to_user(uarg, tmp, sizeof(uint32_t)))
				return -EFAULT;
			break;

		case DISP_CMD_LAYER_REQUEST:
		case DISP_CMD_LAYER_RELEASE:
		case DISP_CMD_LAYER_SET_PARA:
		case DISP_CMD_LAYER_GET_PARA:
		case DISP_CMD_LAYER_SET_SRC_WINDOW: {
			unsigned int __user
			*argp;
			unsigned int tmp[3];
			struct disp_rect *user_rect;
			struct disp_rect rect;
			argp = (unsigned int __user
			*)arg;
			if (copy_from_user(tmp, argp, sizeof(tmp)))
				return -EFAULT;
			user_rect = (struct disp_rect *) (uintptr_t) tmp[2];
			if (tmp[2] != 0) {
				if (copy_from_user(&rect, user_rect, sizeof(rect)))
					return -EFAULT;
				writel(rect.width, iomm.defe + DEFE_STRIDE0);
				writel(rect.width / 2, iomm.defe + DEFE_STRIDE1);
				writel(rect.width / 2, iomm.defe + DEFE_STRIDE2);
				writel((rect.width - 1) | ((rect.height - 1) << 16), iomm.defe + DEFE_IN_SIZE);
				writel((320 - 1) | ((240 - 1) << 16), iomm.defe + DEFE_OUT_SIZE);
				writel(calc_scaling_factor(rect.width, 320), iomm.defe + DEFE_H_FACT);

				//Sunxi VPU aligns the buffer height to a multiple of 16 pixels.
				switch (rect.height) {
					case 256:
						writel(calc_scaling_factor(240, 240), iomm.defe + DEFE_V_FACT);
						break;
					case 384:
						writel(calc_scaling_factor(360, 240), iomm.defe + DEFE_V_FACT);
						break;
					default:
						writel(calc_scaling_factor(rect.height, 240), iomm.defe + DEFE_V_FACT);
						break;
				}
			}
			break;
		}
    

	case DISP_CMD_LAYER_SET_SCN_WINDOW:
		break;

	case DISP_CMD_LAYER_SET_FB:
		if (copy_from_user(&fb, (void __user *)tmp[2], sizeof(fb)))
			return -EFAULT;
		writel((uint32_t)(fb.addr[0] ), iomm.defe+DEFE_ADDR0);
		writel((uint32_t)(fb.addr[1] ), iomm.defe+DEFE_ADDR1);
		writel((uint32_t)(fb.addr[2] ), iomm.defe+DEFE_ADDR2);

		break;
	case DISP_CMD_LAYER_OPEN:
		suniv_setbits(iomm.defe+DEFE_EN, 0x01); // Enable DEFE
		suniv_setbits(iomm.defe+DEFE_EN, (1 << 31)); // Enable CPU access
		writel((0 << 0) | (0 << 1), iomm.defe+DEFE_BYPASS); // CSC/scaler bypass disabled
		writel( (0 << 8 ) | (2 << 4) | (0 << 0) , iomm.defe+DEFE_IN_FMT); // YV12
		suniv_setbits(iomm.defe+DEFE_FRM_CTRL, (1 << 23)); // Enable CPU access to filter RAM (if enabled, filter is bypassed?)
		suniv_clrbits(iomm.defe+DEFE_EN, (1 << 31)); // Disable CPU access (?)
		suniv_setbits(iomm.defe+DEFE_FRM_CTRL, (1 << 0)); // Registers ready
		suniv_setbits(iomm.defe+DEFE_FRM_CTRL, (1 << 16)); // Start frame processing
		debe_layer_set_mode(0, DEBE_MODE_DEFE_VIDEO);
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
		suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));

		for(i = 0; i < 4; i++) // Color conversion table
		{
			writel(csc_tab[2 * 48 + i], iomm.defe + DEFE_CSC_COEF + i * 4 + 0 * 4);
			writel(csc_tab[2 * 48 + i + 4], iomm.defe + DEFE_CSC_COEF + i * 4 + 4 * 4);
			writel(csc_tab[2 * 48 + i + 8], iomm.defe + DEFE_CSC_COEF + i * 4 + 8 * 4);
		}
		suniv_setbits(iomm.debe + DEBE_REGBUFF_CTRL_REG, (1 << 0));
		break;

	case DISP_CMD_LAYER_CLOSE:
		suniv_setbits(iomm.defe+DEFE_EN, 0x0);
		suniv_setbits(iomm.defe+DEFE_EN, (0 << 31));
		writel((0 << 0) | (1 << 1), iomm.defe+DEFE_BYPASS);\
		suniv_clrbits(iomm.debe + DEBE_LAY0_ATT_CTRL_REG0, (1 << 1));
		suniv_clrbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
		suniv_setbits(iomm.debe + DEBE_MODE_CTRL_REG, (1 << 8));
		suniv_setbits(iomm.debe + DEBE_REGBUFF_CTRL_REG, (1 << 0));
		break;
	default:
		pr_warn("disp: unknown ioctl %u\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static void suniv_ioremap(void)
{
	iomm.debe = (uint8_t*)ioremap(SUNIV_DEBE_BASE, 4096);
	iomm.defe = (uint8_t*)ioremap(SUNIV_DEFE_BASE, 4096);

}

static void suniv_iounmap(void)
{
	iounmap(iomm.debe);
	iounmap(iomm.defe);
}

static const struct file_operations disp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = disp_ioctl,
};

static struct miscdevice disp_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "disp",
	.fops = &disp_fops,
};

static int __init disp_init(void)
{
	suniv_ioremap();
	return misc_register(&disp_miscdev);
}

static void __exit disp_exit(void)
{
	suniv_iounmap();
	misc_deregister(&disp_miscdev);

}

module_init(disp_init);
module_exit(disp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tiopex tiopxyz@gmail.com");
MODULE_DESCRIPTION("sunxi DRM display driver");
