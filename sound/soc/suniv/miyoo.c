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
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/gpio/consumer.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <sound/dmaengine_pcm.h>
#include <linux/gpio.h>

#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch-suniv/dma.h>
#include <asm/arch-suniv/cpu.h>
#include <asm/arch-suniv/gpio.h>
#include <asm/arch-suniv/clock.h>
#include <asm/arch-suniv/codec.h>
#include <asm/arch-suniv/common.h>

#define USE_EARPHONE          1		// set to 0 if UART pin becomes inaccessible
#define DMA_SIZE              (128 * 2 * PAGE_SIZE)
#define MIYOO_KBD_GET_HOTKEY  _IOWR(0x100, 0, unsigned long)
#define MIYOO_FB0_PUT_OSD     _IOWR(0x100, 0, unsigned long)
#define MIYOO_SND_SET_VOLUME  _IOWR(0x100, 0, unsigned long)
#define MIYOO_SND_GET_VOLUME  _IOWR(0x101, 0, unsigned long)

struct mypcm {
  uint32_t dma_period;
  dma_addr_t dma_start;
  dma_addr_t dma_pos;
  dma_addr_t dma_end;
};

struct suniv_iomm {
  uint8_t *dma;
  uint8_t *ccm;
  uint8_t *gpio;
  uint8_t *codec;

	void* dma_virt;
  dma_addr_t dma_phys;
  uint8_t dma_trigger;

  int irq;
  struct snd_pcm_substream *substream;
};
static struct suniv_iomm iomm={0};

static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;

static unsigned long MIYOO_VOLUME = 5;
static uint32_t miyoo_snd=1;
module_param(miyoo_snd,uint,0660);

static void suniv_ioremap(void)
{
  iomm.dma_trigger = 0;
  iomm.dma = (uint8_t*)ioremap(SUNIV_DMA_BASE, 4096);
  iomm.ccm = (uint8_t*)ioremap(SUNIV_CCM_BASE, 4096);
  iomm.gpio = (uint8_t*)ioremap(SUNIV_GPIO_BASE, 4096);
  iomm.codec = (uint8_t*)ioremap(SUNIV_CODEC_BASE, 4096);
}

static void suniv_codec_init(void)
{
  suniv_setbits(iomm.ccm + PLL_AUDIO_CTRL_REG, (1 << 31));
  while(!(readl(iomm.ccm + PLL_AUDIO_CTRL_REG) & (1 << 28)));
  suniv_setbits(iomm.ccm + BUS_SOFT_RST_REG2, (1 << 0) | (1 << 12));
  suniv_setbits(iomm.ccm + BUS_CLK_GATING_REG2, (1 << 0) | (1 << 12));
  suniv_setbits(iomm.ccm + AUDIO_CODEC_CLK_REG, (1 << 31));

  writel((1 << 31), iomm.codec + AC_DAC_DPC_REG);
  writel((1 << 26) | (1 << 24) | (0x03 << 21) | (0x3f << 8) | (1 << 4) | (1 << 0), iomm.codec + AC_DAC_FIFOC_REG);
  writel(0xffffffff, iomm.codec + AC_DAC_FIFOS_REG);
  writel((1 << 31) | (1 << 30) | (1 << 27) | (1 << 26) | (1 << 17) | (1 << 15) | (1 << 9) | (10 << 0), iomm.codec + DAC_MIXER_CTRL_REG);
}

static void suniv_gpio_init(void)
{
  uint32_t ret;
  switch(miyoo_snd) {
      case 1:
      ret = readl(iomm.gpio + PA_CFG0);
#if defined(USE_EARPHONE)
      ret&= 0xfffff0f0;
#else
      ret &= 0xfffffff0;
#endif
      ret |= 0x00000001;
      writel(ret, iomm.gpio + PA_CFG0);
      suniv_setbits(iomm.gpio + PA_DATA, (1 << 0));
      break;
      case 2:
        ret = readl(iomm.gpio + PD_CFG0);
        ret &= 0xfffffff0;
        ret |= 0x00000001;
        writel(ret, iomm.gpio + PD_CFG0);
        suniv_setbits(iomm.gpio + PD_DATA, (1 << 0));

        ret = readl(iomm.gpio + PD_CFG1);
        ret &= 0xffffff1f;
        ret |= 0x00000010;
        writel(ret, iomm.gpio + PD_CFG1);
        suniv_setbits(iomm.gpio + PD_DATA, (1 << 9));
        break;
  }

}

static irqreturn_t dma_irq_handler(int irq, void *arg)
{
  uint32_t ret;
  struct snd_pcm_substream *substream = arg;

#if defined(USE_EARPHONE)
  ret = readl(iomm.gpio + PA_DATA);
  if(ret & 4){
    suniv_setbits(iomm.gpio + PA_DATA, (1 << 0));
  }
  else{
    suniv_clrbits(iomm.gpio + PA_DATA, (1 << 0));
  }
#endif

  ret = readl(iomm.dma + DMA_INT_STA_REG);
  writel(0x03, iomm.dma + DMA_INT_STA_REG);
  if(ret & 2){
    if(iomm.dma_trigger){
      struct snd_pcm_runtime *runtime = substream->runtime;
      struct mypcm *prtd = runtime->private_data;

      snd_pcm_period_elapsed(substream);
      if (prtd->dma_pos == prtd->dma_end) {
        prtd->dma_pos = prtd->dma_start;
      }
      writel(prtd->dma_pos, iomm.dma + NDMA0_SRC_ADR_REG);
      writel(prtd->dma_period, iomm.dma + NDMA0_BYTE_CNT_REG);
      prtd->dma_pos+= prtd->dma_period;
      suniv_setbits(iomm.dma + NDMA0_CFG_REG, (1 << 31));
    }
  }
  return IRQ_HANDLED;
}

static int mycpu_dai_probe(struct snd_soc_dai *dai)
{
  return 0;
}

static int mycard_spk_event(struct snd_soc_dapm_widget *w, struct snd_kcontrol *k, int event)
{
  printk("%s\n", __func__);
  return 0;
}

static const struct snd_pcm_hardware mypcm_hardware = { 
  .info = SNDRV_PCM_INFO_MMAP |
    SNDRV_PCM_INFO_MMAP_VALID |
    SNDRV_PCM_INFO_INTERLEAVED |
    SNDRV_PCM_INFO_BLOCK_TRANSFER,
  .formats = SNDRV_PCM_FMTBIT_S16_LE,
  .rates = SNDRV_PCM_RATE_8000_192000,
  .channels_min = 2,
  .channels_max = 2,
  .period_bytes_min = 16, 
  .period_bytes_max = 2 * PAGE_SIZE,
  .periods_min = 2,
  .periods_max = 128,
  .buffer_bytes_max = 128 * 2 * PAGE_SIZE,
};

static int mypcm_open(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
  int ret;
  struct mypcm *prtd;
  struct snd_pcm_runtime *rtd = substream->runtime;

  prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);
 	if (prtd == NULL) {
		printk("%s, failed to allocate runtime data\n", __func__);
  	return -ENOMEM;
	}

  snd_soc_set_runtime_hwparams(substream, &mypcm_hardware);
	ret = snd_pcm_hw_constraint_integer(rtd, SNDRV_PCM_HW_PARAM_PERIODS);
  if (ret < 0) {
		printk("%s, failed to do snd_pcm_hw_constraint_integer(%d)\n", __func__, ret);
  	kfree(prtd);
  	return ret;
  }
  rtd->private_data = prtd;
  return 0;
}

static int mypcm_close(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
  struct mypcm *prtd = runtime->private_data;

 	kfree(prtd);
  return 0;
}

static int mypcm_hw_params(struct snd_soc_component *component, struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
  uint32_t ret;
  struct snd_pcm_runtime *runtime = substream->runtime;
  struct mypcm *prtd = runtime->private_data;

  switch (params_channels(params)) {
  case 2:
    suniv_clrbits(iomm.codec + AC_DAC_FIFOC_REG, (1 << 6));
    break;
  default:
		printk("%s, invalid channel %d\n", __func__, params_channels(params));
    return -EINVAL;
  }

  switch (params_format(params)) {
  case SNDRV_PCM_FORMAT_S16:
    suniv_clrbits(iomm.codec + AC_DAC_FIFOC_REG, (1 << 5));
    break;
  default:
		printk("%s, invalid format %d\n", __func__, params_format(params));
    return -EINVAL;
  }

  switch(params_rate(params)){
  case 44100:
  case 22050:
  case 11025:
    writel(0x90003f10, iomm.ccm + PLL_AUDIO_CTRL_REG);
    break;
  default:
    writel(0x90005514, iomm.ccm + PLL_AUDIO_CTRL_REG);
    break;
  }
  while(!(readl(iomm.ccm + PLL_AUDIO_CTRL_REG) & (1 << 28)));

  ret = readl(iomm.codec + AC_DAC_FIFOC_REG);
  ret&= 0x1fffffff;
	switch (params_rate(params)) {
	case 48000:
	case 44100:
    ret|= (0 << 29);
		break;
	case 32000:
    ret|= (1 << 29);
		break;
	case 24000:
  case 22050:
    ret|= (2 << 29);
		break;
	case 16000:
    ret|= (3 << 29);
		break;
	case 12000:
  case 11025:
    ret|= (4 << 29);
		break;
	case 8000:
    ret|= (5 << 29);
		break;
	case 192000:
    ret|= (6 << 29);
		break;
	case 96000:
    ret|= (7 << 29);
		break;
	default:
		printk("%s, invalid rate %d\n", __func__, params_rate(params));
		return -EINVAL;
	}
  writel(ret, iomm.codec + AC_DAC_FIFOC_REG);

  snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
  runtime->dma_bytes = params_buffer_bytes(params);
  prtd->dma_period = params_period_bytes(params);
  prtd->dma_start = runtime->dma_addr;
  prtd->dma_pos = prtd->dma_start;
  prtd->dma_end = prtd->dma_start + runtime->dma_bytes;
  writel(prtd->dma_period, iomm.dma + NDMA0_BYTE_CNT_REG);
  return 0;
}

static int mypcm_hw_free(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
  return 0;
}

static int mypcm_prepare(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
  struct mypcm *prtd = substream->runtime->private_data;

  prtd->dma_pos = prtd->dma_start;
  return 0;
}

static int mypcm_trigger(struct snd_soc_component *component, struct snd_pcm_substream *substream, int cmd)
{ 
  switch (cmd) {
  case SNDRV_PCM_TRIGGER_START:
  case SNDRV_PCM_TRIGGER_RESUME:
  case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
    iomm.dma_trigger = 1;
    suniv_setbits(iomm.codec + AC_DAC_FIFOC_REG, 1);
		suniv_setbits(iomm.dma + NDMA0_CFG_REG, (1 << 31));
    break;
  case SNDRV_PCM_TRIGGER_STOP:
  case SNDRV_PCM_TRIGGER_SUSPEND:
  case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
    iomm.dma_trigger = 0;
    break;
  default:
    break;
  }
  return 0;
}

static snd_pcm_uframes_t mypcm_pointer(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
  struct snd_pcm_runtime *runtime = substream->runtime;
  struct mypcm *prtd = runtime->private_data;
  unsigned long byte_offset;
  snd_pcm_uframes_t offset;

  byte_offset = prtd->dma_pos - prtd->dma_start;
  byte_offset-= readl(iomm.dma + NDMA0_BYTE_CNT_REG);
  offset = bytes_to_frames(runtime, byte_offset);
  if (offset >= runtime->buffer_size) {
    offset = 0;
	}
  return offset;
}

static int mypcm_new(struct snd_soc_component *component, struct snd_soc_pcm_runtime *rtd)
{
  struct snd_pcm *pcm = rtd->pcm;
  struct snd_card *card = rtd->card->snd_card;
  struct snd_pcm_substream *substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
  struct snd_dma_buffer *buf = &substream->dma_buffer;
  suniv_ioremap();
  suniv_setbits(iomm.ccm + BUS_CLK_GATING_REG0, (1 << 6));
  suniv_setbits(iomm.ccm + BUS_SOFT_RST_REG0, (1 << 6));
  writel(0x00000000, iomm.dma + DMA_INT_CTRL_REG);
  writel(0xffffffff, iomm.dma + DMA_INT_STA_REG);
  writel(0x00000002, iomm.dma + DMA_INT_CTRL_REG);
  writel(0x01ac8190, iomm.dma + NDMA0_CFG_REG);
  writel(SUNIV_CODEC_BASE + AC_DAC_TXDATA_REG, iomm.dma + NDMA0_DES_ADR_REG);
  writel(DMA_SIZE, iomm.dma + NDMA0_BYTE_CNT_REG);
  iomm.dma_virt = dma_alloc_coherent(card->dev, DMA_SIZE, (resource_size_t*)&iomm.dma_phys, GFP_KERNEL | GFP_DMA);
  if(iomm.dma_virt == NULL){
    printk("%s, failed to allocate DMA memory\n", __func__);
    return -ENOMEM;
  }
  writel(iomm.dma_phys, iomm.dma + NDMA0_SRC_ADR_REG);

  buf->dev.type = SNDRV_DMA_TYPE_DEV;
  buf->dev.dev = pcm->card->dev;
  buf->private_data = NULL;
  buf->area = iomm.dma_virt;
	buf->addr = iomm.dma_phys;
  buf->bytes = DMA_SIZE;
  iomm.substream = substream;
  return 0;
}

static const struct snd_soc_component_driver mycodec_comp = {
  .name = "miyoo codec",
};

static struct snd_soc_dai_driver mycpu_dai = {
  .name = "miyoo cpu dai",
  .probe = mycpu_dai_probe,
  .playback = {
    .stream_name = "miyoo cpu playback",
    .channels_min = 2,
    .channels_max = 2,
    .rates = SNDRV_PCM_RATE_8000_192000,
    .formats = SNDRV_PCM_FMTBIT_S16_LE,
  },
};

static const struct snd_soc_dapm_widget mycard_dapm_widgets[] = {
  SND_SOC_DAPM_SPK("Speaker", mycard_spk_event),
};


static const struct snd_soc_component_driver myplatform = {
	.open = mypcm_open,
	.close = mypcm_close,
	.hw_params = mypcm_hw_params,
	.hw_free = mypcm_hw_free,
	.prepare = mypcm_prepare,
	.trigger = mypcm_trigger,
	.pointer = mypcm_pointer,
	.pcm_construct = mypcm_new,
};

static int myopen(struct inode *inode, struct file *file)
{
  return 0;
}

static int myclose(struct inode *inode, struct file *file)
{
  return 0;
}

extern void MIYOO_SET_VOLUME(unsigned long arg) {
  uint32_t ret;
  ret = readl(iomm.codec + DAC_MIXER_CTRL_REG);
  ret&= ~0x3f;
  ret|= (arg * 6);
  writel(ret, iomm.codec + DAC_MIXER_CTRL_REG);
  MIYOO_VOLUME = arg;
  //printk("MIYOO_SET_VOLUME %ld %ld %ld\n", arg, (arg * 6),MIYOO_VOLUME);
}
EXPORT_SYMBOL_GPL(MIYOO_SET_VOLUME);

extern void MIYOO_INCREASE_VOLUME(void){
  if(MIYOO_VOLUME < 9) {
    MIYOO_SET_VOLUME(MIYOO_VOLUME+1);
  }
}
EXPORT_SYMBOL_GPL(MIYOO_INCREASE_VOLUME);

extern void MIYOO_DECREASE_VOLUME(void){
  if(MIYOO_VOLUME > 0) {
    MIYOO_SET_VOLUME(MIYOO_VOLUME-1);
  }
}
EXPORT_SYMBOL_GPL(MIYOO_DECREASE_VOLUME);

static long myioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  uint32_t ret;

  switch(cmd){
  case MIYOO_SND_SET_VOLUME:
    MIYOO_SET_VOLUME(arg);
    break;
  case MIYOO_SND_GET_VOLUME:
    ret = copy_to_user((void*)arg, &MIYOO_VOLUME, sizeof(unsigned long));
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

static int myaudio_probe(struct platform_device *pdev)
{
  int ret;
  struct snd_soc_card *card;
  struct snd_soc_dai_link_component *compnent;


  card = devm_kzalloc(&pdev->dev, sizeof(*card), GFP_KERNEL);
  if (card == NULL) {
    dev_err(&pdev->dev, "%s, failed to allocate memory for card\n", __func__);
    return -ENOMEM; // sorry, no any error handling
  }
  card->dai_link = devm_kzalloc(&pdev->dev, sizeof(struct snd_soc_dai_link), GFP_KERNEL);
  if (card->dai_link == NULL) {
    dev_err(&pdev->dev, "%s, faild to create link\n", __func__);
    return -ENOMEM; // sorry, no any error handling
  }

  ret = snd_soc_register_component(&pdev->dev, &myplatform, &mycpu_dai, 0);
  if (ret < 0) {
    dev_err(&pdev->dev, "%s, failed to register platform(%d)\n", __func__, ret);
    return ret; // sorry, no any error handling
  }

  ret = snd_soc_register_component(&pdev->dev, &mycodec_comp, &mycpu_dai, 1);
  if (ret < 0) {
    dev_err(&pdev->dev, "%s, failed to register dai\n", __func__);
    return ret; // sorry, no any error handling
  }

  compnent = devm_kzalloc(&pdev->dev, 3 * sizeof(*compnent), GFP_KERNEL);
  if (!compnent)
      return -ENOMEM;
  card->dai_link->cpus		= &compnent[0];
  card->dai_link->num_cpus	= 1;
  card->dai_link->codecs		= &compnent[1];
  card->dai_link->num_codecs	= 1;
  card->dai_link->platforms	= &compnent[2];
  card->dai_link->num_platforms	= 1;
  card->dev = &pdev->dev;
  card->name = "miyoo audio card";
  card->num_links = 1;
  card->dai_link->name = dev_name(&pdev->dev);
  card->dai_link->stream_name = dev_name(&pdev->dev);
  card->dai_link->codecs->dai_name = dev_name(&pdev->dev);
  card->dai_link->cpus->dai_name = dev_name(&pdev->dev);
  card->dai_link->codecs->name = dev_name(&pdev->dev);
  card->dai_link->platforms->name = dev_name(&pdev->dev);
  card->dai_link->dai_fmt = SND_SOC_DAIFMT_I2S;
  card->dapm_widgets = mycard_dapm_widgets;
  card->num_dapm_widgets = ARRAY_SIZE(mycard_dapm_widgets);
  card->fully_routed = true;
  snd_soc_card_set_drvdata(card, NULL);

  ret = snd_soc_register_card(card);
  if (ret) {
    dev_err(&pdev->dev, "%s, failed to register card\n", __func__);
    return ret; // sorry, no any error handling
  }
  
  alloc_chrdev_region(&major, 0, 1, "miyoo_snd");
  myclass = class_create(THIS_MODULE, "miyoo_snd");
  device_create(myclass, NULL, major, NULL, "miyoo_snd");
  cdev_init(&mycdev, &myfops);
  cdev_add(&mycdev, major, 1);
  suniv_ioremap();
  suniv_gpio_init();
  suniv_codec_init();
  for(ret=0; ret<of_clk_get_parent_count(pdev->dev.of_node); ret++){
    clk_prepare_enable(of_clk_get(pdev->dev.of_node, ret));
  }
  iomm.dma_trigger = 0;

  iomm.irq = platform_get_irq(pdev, 0);
  if (iomm.irq < 0) {
    printk("%s, failed to get irq number\n", __func__);
  }
  else{
	  if(request_irq(iomm.irq, dma_irq_handler, IRQF_SHARED, "miyoo_dma_irq", iomm.substream)) {
      printk("%s, failed to register DMA interrupt\n", __func__);
    }
  }
  return 0;
}

static const struct of_device_id myaudio_driver_of_match[] = {
  {
    .compatible = "allwinner,suniv-f1c100s-codec",
  },{}
};
MODULE_DEVICE_TABLE(of, myaudio_driver_of_match);

static struct platform_driver myaudio_driver = {
  .driver = {
    .name = "miyoo audio driver",
    .of_match_table = myaudio_driver_of_match,
  },
  .probe = myaudio_probe,
};
module_platform_driver(myaudio_driver);

MODULE_DESCRIPTION("Allwinner f1c100s audio codec driver");
MODULE_AUTHOR("Steward Fu <steward.fu@gmail.com>");
MODULE_LICENSE("GPL");
