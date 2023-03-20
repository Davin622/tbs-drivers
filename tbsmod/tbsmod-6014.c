#include <linux/pci.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/kfifo.h>
#include <linux/dvb/frontend.h>

#define TBSMOD_MAX_DEVICES	16

#define	TBSMOD_FIFOSIZE		(697 * 188)	/* ~128K, 27MS at QAM256 */
#define	TBSMOD_DMASIZE		(32 * 1024)
#define TBSMOD_BLOCKSIZE	(188 * 96)

#define TBSMOD_REG_SPI_CONFIG				0x0010
#define TBSMOD_REG_SPI_STATUS				0x0010
#define TBSMOD_REG_SPI_COMMAND				0x0014
#define TBSMOD_REG_SPI_WRITE				0x0018
#define TBSMOD_REG_SPI_READ					0x001c
#define TBSMOD_REG_SPI_ENABLE				0x001c
#define TBSMOD_REG_SPI_DEVICE				0x0020			// spi choose: 0 is for 9789, 1 is for fpga , default is 0
#define TBSMOD_REG_AD9789_RESET				0x0024			// spi config 9789 reset , default 0, 1 is valid. read for qamb is mod and control
#define TBSMOD_REG_AD9789_MODULATION		0x0028			// spi mod set
#define TBSMOD_REG_SPI_AD4351				0x002c			// AD4351 SPI write

#define TBSMOD_REG_RESET_IPCORE				0x0038			// qamb reset ipcore

#define TBSMOD_REG_DMA_BASE(_n)				(0x8000 + 0x1000 * _n)
#define TBSMOD_REG_DMA_STAT					0x0000
#define TBSMOD_REG_DMA_EN					0x0000
#define TBSMOD_REG_DMA_SIZE					0x0004
#define TBSMOD_REG_DMA_ADDR_HIGH			0x0008
#define TBSMOD_REG_DMA_ADDR_LOW				0x000c
#define TBSMOD_REG_DMA_BSIZE				0x0010
#define TBSMOD_REG_DMA_DELAY				0x0014
#define TBSMOD_REG_DMA_DELAYSHORT			0x0018
#define TBSMOD_REG_DMA_INT_MONITOR			0x001c
#define TBSMOD_REG_DMA_SPEED_CTRL			0x0020

#define TBSMOD_REG_INT_BASE					0xc000
#define TBSMOD_REG_INT_CLEAR				(TBSMOD_REG_INT_BASE + 0x00)
#define TBSMOD_REG_INT_EN					(TBSMOD_REG_INT_BASE + 0x04)
#define TBSMOD_REG_INT_MASK					(TBSMOD_REG_INT_BASE + 0x08)
#define TBSMOD_REG_INT_STATUS				(TBSMOD_REG_INT_BASE + 0x0c)
#define TBSMOD_REG_DMA_IE(_n)				(TBSMOD_REG_INT_BASE + 0x0018 + (4 * _n))
#define TBSMOD_REG_DMA_IF(_n)				(0x10 << _n)

#define TBSMOD_AD9789_SPI_CTL	 				0x00
#define TBSMOD_AD9789_SATURA_CNT				0x01
#define TBSMOD_AD9789_PARITY_CNT				0x02
#define TBSMOD_AD9789_INT_ENABLE				0x03
#define TBSMOD_AD9789_INT_STATUS				0x04
#define TBSMOD_AD9789_CHANNEL_ENABLE			0x05
#define TBSMOD_AD9789_FILTER_BYPASS				0x06
#define TBSMOD_AD9789_QAM_CONFIG				0x07
#define TBSMOD_AD9789_SUM_SCALAR				0x08
#define TBSMOD_AD9789_INPUT_SCALAR				0x09
#define TBSMOD_AD9789_NCO_0_FRE					0x0a
#define TBSMOD_AD9789_NCO_1_FRE					0x0d
#define TBSMOD_AD9789_NCO_2_FRE					0x10
#define TBSMOD_AD9789_NCO_3_FRE					0x13
#define TBSMOD_AD9789_RATE_CONVERT_Q			0x16
#define TBSMOD_AD9789_RATE_CONVERT_P			0x19
#define TBSMOD_AD9789_CENTER_FRE_BPF  			0x1c
#define TBSMOD_AD9789_FRE_UPDATE				0x1e
#define TBSMOD_AD9789_HARDWARE_VERSION			0x1f
#define TBSMOD_AD9789_INTERFACE_CONFIG			0x20
#define TBSMOD_AD9789_DATA_CONTROL				0x21
#define TBSMOD_AD9789_DCO_FRE					0x22
#define TBSMOD_AD9789_INTERNAL_COLCK_ADJUST		0x23
#define TBSMOD_AD9789_PARAMETER_UPDATE			0x24
#define TBSMOD_AD9789_CHANNEL_0_GAIN			0x25
#define TBSMOD_AD9789_CHANNEL_1_GAIN			0x26
#define TBSMOD_AD9789_CHANNEL_2_GAIN			0x27	
#define TBSMOD_AD9789_CHANNEL_3_GAIN			0x28
#define TBSMOD_AD9789_SPEC_SHAPING				0x29
#define TBSMOD_AD9789_Mu_DELAY_CONTROL_1		0x2f
#define TBSMOD_AD9789_Mu_CONTROL_DUTY_CYCLE		0x30
#define TBSMOD_AD9789_CLOCK_RECIVER_1			0x31
#define TBSMOD_AD9789_CLOCK_RECIVER_2			0x32
#define TBSMOD_AD9789_Mu_DELAY_CONTROL_2		0x33
#define TBSMOD_AD9789_DAC_BIAS					0x36
#define TBSMOD_AD9789_DAC_DECODER				0x38
#define TBSMOD_AD9789_Mu_DELAY_CONTROL_3		0x39
#define TBSMOD_AD9789_Mu_DELAY_CONTROL_4		0x3a
#define TBSMOD_AD9789_FULL_SCALE_CURRENT_1		0x3c
#define TBSMOD_AD9789_FULL_SCALE_CURRENT_2		0x3d
#define TBSMOD_AD9789_PHASE_DETECTOR_CONTROL	0x3e
#define TBSMOD_AD9789_BIST_CONTROL				0x40
#define TBSMOD_AD9789_BIST_STATUS				0x41
#define TBSMOD_AD9789_BIST_ZERO_LENGTH			0x42
#define TBSMOD_AD9789_BIST_VECTOR_LENGTH		0x44
#define TBSMOD_AD9789_BIST_CLOCK_ADJUST			0x47
#define TBSMOD_AD9789_SIGN_0_CONTROL			0x48
#define TBSMOD_AD9789_SIGN_0_CLOCK_ADJUST		0x49
#define TBSMOD_AD9789_SIGN_1_CONTROL			0x4a
#define TBSMOD_AD9789_SIGN_1_CLOCK_ADJUST		0x4b
#define TBSMOD_AD9789_REGFNL_0_FREQ				0x4c
#define TBSMOD_AD9789_REGFNL_1_FREQ				0x4d
#define TBSMOD_AD9789_BITS_SIGNATURE_0			0x50
#define TBSMOD_AD9789_BITS_SIGNATURE_1			0x53

/* Choosing frequencies:
 * The channels do not need to be adjacent, but they do need
 * to fit into ~80Mhz? it seems. So what we need to do is check
 * the channel frequencies against each other to make sure we stay in
 * this block. When we set a channel frequency, we need to check
 * against the current center freq and change if need be.
 */

static int tbsmod_major;
static struct cdev tbsmod_cdev;
static struct class *tbsmod_class;

struct tbsmod_channel {
	struct tbsmod		*priv;
	int					num;

	void __iomem 		*dma_reg;
	__le32				*dmavirt;
	dma_addr_t			dmaphy;	
	u8 					dma_active;
	spinlock_t			dma_lock; //  dma lock

	struct mutex		lock;
	u8					open;

	u8					enabled;
	u32					frequency;

	struct kfifo 		fifo; 
	wait_queue_head_t	wait_queue;
};

struct tbsmod {
	struct pci_dev		*pdev;
	void __iomem		*mmio;

	int					card_num;

	struct mutex		spi_mutex;		// lock spi access
	struct mutex		ioctl_mutex;	// lock ioctls access

	struct tbsmod_channel	channel[4];
};

static struct tbsmod *tbsmod_cards[4];

static int tbsmod_ad4351_write(struct tbsmod *priv, u32 val)
{
	u32 v;
	int i;

	dev_dbg(&priv->pdev->dev, "ad4351 write: 0x%x\n", val);

	writel(val, priv->mmio + TBSMOD_REG_SPI_AD4351);
	msleep(10);
	for (i = 0; i < 50; ++i) {
		v = readl(priv->mmio + TBSMOD_REG_SPI_AD4351);
		if ((v & 0xff ) == 1)
			break;

		msleep(10);
	}

	return (i == 50 ? -ETIMEDOUT : 0);
}

static int tbsmod_ad4351_init(struct tbsmod *priv)
{
	int ret;

	ret = tbsmod_ad4351_write(priv, 0x05005800);
	if (ret)
		return ret;
	ret = tbsmod_ad4351_write(priv, 0x3c809c00);
	if (ret)
		return ret;
	ret = tbsmod_ad4351_write(priv, 0xb3040000);
	if (ret)
		return ret;
	ret = tbsmod_ad4351_write(priv, 0x424e0000);
	if (ret)
		return ret;
	ret = tbsmod_ad4351_write(priv, 0xc9800008);
	if (ret)
		return ret;
	ret = tbsmod_ad4351_write(priv, 0xb0003d00);
	if (ret)
		return ret;

	return 0;
}

static int tbsmod_ad9789_reg_read(struct tbsmod *priv, u8 reg, u8 *val)
{
	u32 tmp;
	int i;

	mutex_lock(&priv->spi_mutex);

	// reg 13bit;
	tmp = 0x80 | (reg << 8);		//3'b1xx; read
	writel(tmp, priv->mmio + TBSMOD_REG_SPI_COMMAND);

	// lower byte: cs low, cs high, write + read
	// upper byte: tx/rx count. upper nibble is tc, lower nibble rx
	writel(0x21f0, priv->mmio + TBSMOD_REG_SPI_CONFIG);

	// Wait for SPI ready
	for (i=0; i < 20; ++i) {
		tmp = readl(priv->mmio + TBSMOD_REG_SPI_STATUS);
		if (tmp & 0x1)
			break;
		msleep(5);
	}
	if (i == 20) {
		mutex_unlock(&priv->spi_mutex);
		dev_err(&priv->pdev->dev, "tbsmod_ad9789_reg_read() timeout\n");
		return -ETIMEDOUT;
	}

	*val = readl(priv->mmio + TBSMOD_REG_SPI_READ);

	dev_dbg(&priv->pdev->dev, "SPI read: 0x%x -> 0x%x\n", reg, *val);

	msleep(1);

	mutex_unlock(&priv->spi_mutex);

	return 0;
}

static int tbsmod_ad9789_reg_write(struct tbsmod *priv, u8 reg, u8 val)
{
	u32 tmp;
	int i;

	mutex_lock(&priv->spi_mutex);

	dev_dbg(&priv->pdev->dev, "SPI write: 0x%x -> 0x%x\n", reg, val);

	tmp = (reg << 8) | (val << 16);		//3'b1xx; read
	writel(tmp, priv->mmio + TBSMOD_REG_SPI_COMMAND);
	writel(0, priv->mmio + TBSMOD_REG_SPI_WRITE);		// Only used for multi-byte xfers > 2 bytes

	// lower byte: cs low, cs high, write, no read
	// upper byte: tx/rx count. upper nibble is tc, lower nibble rx
	writel(0x30e0, priv->mmio + TBSMOD_REG_SPI_CONFIG);

	// Wait for SPI ready
	for (i=0; i < 20; ++i) {
		tmp = readl(priv->mmio + TBSMOD_REG_SPI_STATUS);
		if (tmp & 0x1)
			break;
		msleep(5);
	}
	if (i == 20) {
		mutex_unlock(&priv->spi_mutex);
		dev_err(&priv->pdev->dev, "tbsmod_ad9789_reg_read() timeout\n");
		return -ETIMEDOUT;
	}

	mutex_unlock(&priv->spi_mutex);

	return 0;
}

static int tbsmod_ad9789_update_freq(struct tbsmod *priv)
{
	u8 reg[4] = {TBSMOD_AD9789_NCO_0_FRE, TBSMOD_AD9789_NCO_1_FRE, TBSMOD_AD9789_NCO_2_FRE, TBSMOD_AD9789_NCO_3_FRE};
	u32 freq, f_min = 1000, f_max = 0;
	u8 ch_enable = 0;
	int i;
	int ret;

	/* Update all the channel frequencies. Find the min/max of all the channels */
	for (i = 0; i < 4; ++i) {
		if (priv->channel[i].enabled) {
			if (priv->channel[i].frequency < f_min)
				f_min = priv->channel[i].frequency;
			if (priv->channel[i].frequency > f_max)
				f_max = priv->channel[i].frequency;

			freq = div_u64(16777215ull * priv->channel[i].frequency, 96);

			ch_enable |= (0x1 << i);
		}
		else
			freq = 0;

		ret = tbsmod_ad9789_reg_write(priv, reg[i], freq & 0xff);
		if (ret)
			return ret;
		ret = tbsmod_ad9789_reg_write(priv, reg[i] + 1, (freq >> 8) & 0xff);
		if (ret)
			return ret;
		ret = tbsmod_ad9789_reg_write(priv, reg[i] + 2, (freq >> 16) & 0xff);
		if (ret)
			return ret;
	}

	/* Center frequency of the bpf */
	freq = f_min + ((f_max - f_min) / 2);
	freq = div_u64(freq * 65535ull, 1536);
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CENTER_FRE_BPF, freq & 0xff);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CENTER_FRE_BPF + 1, (freq >> 8) & 0xff);
	if (ret)
		return ret;

	// Enable active channels
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CHANNEL_ENABLE, ch_enable);
	if (ret)
		return ret;

	//update
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FRE_UPDATE, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FRE_UPDATE, 0x80);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_PARAMETER_UPDATE, 0x00); 
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_PARAMETER_UPDATE, 0x80); 
	if (ret)
		return ret;

	return 0;
}

static int tbsmod_ad9789_set_gain(struct tbsmod_channel *ch, u8 gain)
{
	if (gain < 1)
		gain = 1;
	if (gain > 10)
		gain = 10;

	// 128 is gain = 1.0
	// This is a summation gain, not an RF gain
	// At 4 adjacent channels, anything over 40 causes transport errors, so we just cap
	gain *= 4;
	return tbsmod_ad9789_reg_write(ch->priv, TBSMOD_AD9789_CHANNEL_0_GAIN + ch->num, gain);
}

static int tbsmod_ad9789_init(struct tbsmod *priv)
{
	int i;
	int ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CLOCK_RECIVER_2, 0x9e);			//CLK_DIS=1;PSIGN=0;CLKP_CML=0x0F;NSIGN=0
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_Mu_CONTROL_DUTY_CYCLE, 0x80);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_Mu_DELAY_CONTROL_1, 0xce);		//SEARCH_TOL=1;SEARCH_ERR=1;TRACK_ERR=0;GUARDBAND=0x0E
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_Mu_DELAY_CONTROL_2, 0x42);		//MU_CLKDIS=0;SLOPE=1;MODE=0x00;MUSAMP=0;GAIN=0x01;MU_EN=1;
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_Mu_DELAY_CONTROL_3, 0x4e);		//MUDLY=0x00;SEARCH_DIR=0x10;MUPHZ=0x0E;
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_Mu_DELAY_CONTROL_4, 0x6c);		//MUDLY=0x9F;
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_INT_ENABLE, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_INT_STATUS, 0xfe);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_INT_ENABLE, 0x0c);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_Mu_DELAY_CONTROL_2, 0x43);				//MU_CLKDIS=0;SLOPE=1;MODE=0x00;MUSAMP=0;GAIN=0x01;MU_EN=1;
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FILTER_BYPASS, 0x01);
	if (ret)
		return ret;

	/* The AD9789 supports QAM64, but we are forcing it to QAM256 only, as all
	 * the modulation channels must be the same */
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_QAM_CONFIG, 0x01);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_RATE_CONVERT_Q, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_RATE_CONVERT_Q + 1, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_RATE_CONVERT_Q + 2, 0x80);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_RATE_CONVERT_P, 0x4c);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_RATE_CONVERT_P + 1, 0xa4);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_RATE_CONVERT_P + 2, 0x47);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_DATA_CONTROL, 0x60);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_INTERNAL_COLCK_ADJUST, 0x07);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FRE_UPDATE, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FRE_UPDATE, 0x80);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_PARAMETER_UPDATE, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_PARAMETER_UPDATE, 0x80);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_SUM_SCALAR, 0x14);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_INPUT_SCALAR, 0x20);
	if (ret)
		return ret;
	
	ret = tbsmod_ad9789_update_freq(priv);
	if (ret)
		return ret;
	
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_INTERFACE_CONFIG, 0x06);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_DCO_FRE, 0x10);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CHANNEL_0_GAIN, 0);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CHANNEL_1_GAIN, 0);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CHANNEL_2_GAIN, 0);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CHANNEL_3_GAIN, 0);
	if (ret)
		return ret;

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_SPEC_SHAPING, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FULL_SCALE_CURRENT_1, 0x00); 
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FULL_SCALE_CURRENT_2, 0x02); 
	if (ret)
		return ret;

	for (i = 0; i < 100; ++i) {
		u8 val;

		ret = tbsmod_ad9789_reg_read(priv, TBSMOD_AD9789_INT_STATUS, &val);
		if (ret)
			return ret;
		if (val == 0x08)
			break;
		msleep(10);
	}

	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FRE_UPDATE, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_FRE_UPDATE, 0x80); 
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_PARAMETER_UPDATE, 0x00);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_PARAMETER_UPDATE, 0x80);
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_CHANNEL_ENABLE, 0x00); 		// disable default four channels
	if (ret)
		return ret;
	ret = tbsmod_ad9789_reg_write(priv, TBSMOD_AD9789_INT_ENABLE, 0x0e);
	if (ret)
		return ret;

	return 0;
}

static int tsmod_dma_go(struct tbsmod_channel *ch)
{
	ch->dma_active = 1;

	writel(1000, ch->dma_reg + TBSMOD_REG_DMA_DELAY);
	writel(1, ch->dma_reg + TBSMOD_REG_DMA_EN);

	return 0;
}

static irqreturn_t tbsmod_irq(int irq, void *dev_id)
{
	struct tbsmod *priv = (struct tbsmod *)dev_id;
	u32 status;
	int i, handled = 0;

	status = readl(priv->mmio + TBSMOD_REG_INT_STATUS);		// Get which interrupts are set
	writel(status, priv->mmio + TBSMOD_REG_INT_CLEAR);		// Clear interrupts

	for (i = 0; i < 4; ++i) {
		struct tbsmod_channel *ch = &priv->channel[i];
		int ret;

		if (0 == (status & TBSMOD_REG_DMA_IF(i)))
			continue;

		spin_lock(&ch->dma_lock);

		if (kfifo_len(&ch->fifo) >= TBSMOD_BLOCKSIZE) {
			ret = kfifo_out(&ch->fifo, (void *)ch->dmavirt, TBSMOD_BLOCKSIZE);
			tsmod_dma_go(ch);
			wake_up_interruptible(&ch->wait_queue);
		}
		else	/* Sleep a little and trigger the irq again */
			writel(1000, ch->dma_reg + TBSMOD_REG_DMA_DELAYSHORT);

		spin_unlock(&ch->dma_lock);

		handled = 1;
	}

	writel(1, priv->mmio + TBSMOD_REG_INT_EN);

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static unsigned int tbsmod_poll(struct file *file, poll_table *wait)
{
	struct tbsmod_channel *ch = file->private_data;

	poll_wait(file, &ch->wait_queue, wait);

	if (kfifo_avail(&ch->fifo) > (7*188))
		return POLLOUT | POLLWRNORM;

	return 0;
}

static ssize_t tbsmod_write(struct file *file, const char __user *ptr, size_t count, loff_t *ppos)
{
	struct tbsmod_channel *ch = file->private_data;
	unsigned int copied;
	int ret;

	/* Nothing to do if no data to write */
	if (count == 0)
		return 0;

	/* If we cannot complete the write, handle non-blocking semantics */
	if (kfifo_avail(&ch->fifo) < count) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(ch->wait_queue, kfifo_avail(&ch->fifo) >= count);
		if (ret)
			return ret;
	}

	ret = kfifo_from_user(&ch->fifo, ptr, count, &copied);
	if (ret)
		return ret;

	if (ch->enabled) {
		if (kfifo_len(&ch->fifo) >= TBSMOD_BLOCKSIZE && !ch->dma_active) {
			ret = kfifo_out(&ch->fifo, (void *)ch->dmavirt, TBSMOD_BLOCKSIZE);
			tsmod_dma_go(ch);
		}
	}

	return copied;
}

static int tbsmod_open(struct inode *inode, struct file *file)
{
	struct tbsmod *priv = tbsmod_cards[iminor(inode)>>2];
	struct tbsmod_channel *ch = &priv->channel[iminor(inode)&0x3];

	if ((file->f_flags & O_ACCMODE) != O_RDONLY && ch->open)
		return -EBUSY;

	if (mutex_lock_interruptible(&ch->lock))
		return -ERESTARTSYS;

	if ((file->f_flags & O_ACCMODE) != O_RDONLY)
		ch->open = 1;

	file->private_data = ch;
	mutex_unlock(&ch->lock);
	return 0;
}

static int tbsmod_release(struct inode *inode, struct file *file)
{
	struct tbsmod_channel *ch = file->private_data;
	if (!ch)
		return -ENODEV;

	if (mutex_lock_interruptible(&ch->lock))
		return -ERESTARTSYS;

	if ((file->f_flags & O_ACCMODE) != O_RDONLY) {
		writel(0, ch->dma_reg + TBSMOD_REG_DMA_EN);

		kfifo_reset(&ch->fifo);

		ch->dma_active = 0;
		ch->open = 0;
		ch->enabled = 0;

		/* Re-adjust BPF center, disable channel */
		tbsmod_ad9789_update_freq(ch->priv);
	}

	mutex_unlock(&ch->lock);
	return 0;
}

static long tbsmod_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tbsmod_channel *ch = file->private_data;
	struct dvb_frontend_info info;
	struct dtv_properties props;
	struct dtv_property prop;
	s32 f_min, f_max, freq;
	int i, j;
	int ret;

	if ((file->f_flags & O_ACCMODE) == O_RDONLY && _IOC_DIR(cmd) != _IOC_READ )
		return -EACCES;

	switch (cmd) {
	case FE_GET_INFO:
		memset(&info, 0, sizeof(info));
		snprintf(info.name, sizeof(info.name), "TBSMod 6014");
		ret = copy_to_user((void __user *)arg, &info, sizeof(info));
		break;

	case FE_SET_PROPERTY:
		ret = copy_from_user(&props , (const char*)arg, sizeof(struct dtv_properties ));
		for (i = 0; i < props.num; ++i) {
			ret = copy_from_user(&prop , (const char*)&props.props[i], sizeof(struct dtv_property ));
			switch (prop.cmd) {
			case DTV_DELIVERY_SYSTEM:
				if (prop.u.data != SYS_DVBC_ANNEX_B)
					return -EINVAL;
				break;
			case DTV_MODULATION:
				if (prop.u.data != QAM_256)
					return -EINVAL;
				break;
			case DTV_FREQUENCY:
				/* make sure this frequency doesn't make our bpf too wide */
				f_min = 1000; f_max = 0;
				freq = prop.u.data / 1000000;
				for (j = 0; j < 4; ++j) {
					if (j == ch->num)
						continue;

					if (!ch->priv->channel[j].enabled)
						continue;

					f_min = min(f_min, (s32)ch->priv->channel[j].frequency);
					f_max = max(f_max, (s32)ch->priv->channel[j].frequency);
				}
				if (f_min < 1000 && f_max > 0) {
					if (abs(f_max - freq) > 48 || abs(f_min - freq) > 48)
						return -EINVAL;
				}
				ch->frequency = freq;
				tbsmod_ad9789_update_freq(ch->priv);
				break;
			case DTV_LNA:
				tbsmod_ad9789_set_gain(ch, prop.u.data);
				break;
			case DTV_TUNE:
				if (0 == ch->frequency)
					return -EINVAL;
				ch->enabled = 1;
				tbsmod_ad9789_update_freq(ch->priv);
				break;
			case DTV_CLEAR:
				ch->enabled = 0;
				tbsmod_ad9789_update_freq(ch->priv);
				break;
			default:
				return -EINVAL;
			}
		}
		break;
	case FE_GET_PROPERTY:
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct file_operations tbsmod_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.poll		= tbsmod_poll,
	.write		= tbsmod_write,
	.open		= tbsmod_open,
	.release	= tbsmod_release,
	.unlocked_ioctl	= tbsmod_ioctl,
};

static int tbsmod_probe(struct pci_dev *pdev,
						const struct pci_device_id *pci_id)
{
	struct tbsmod *priv;
	u8 val;
	int ret, i, card_num;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct tbsmod), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pdev = pdev;
	pci_set_drvdata(pdev, priv);

	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable device: %i\n", ret);
		goto err;
	}

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (ret < 0)
	{
		dev_err(&pdev->dev, "32-bit PCI DMA not supported\n");
		goto err;
	}

	pci_set_master(pdev);

	ret = pci_request_region(pdev, 0, "tbs6014");
	if (ret) {
		dev_err(&pdev->dev, "pci_request_region failed: %i\n", ret);
		goto err0;
	}

	priv->mmio = ioremap(pci_resource_start(pdev, 0),
						pci_resource_len(pdev, 0));
	if (!priv->mmio) {
		dev_err(&pdev->dev, "ioremap failed: %i\n", ret);
		goto err1;
	}

	writel(0, priv->mmio + TBSMOD_REG_INT_EN);
	writel(0xff, priv->mmio + TBSMOD_REG_INT_CLEAR);

	ret = pci_enable_msi(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set irq to msi: %i\n", ret);
		goto err2;
	}

	ret = request_irq(pdev->irq, tbsmod_irq, IRQF_SHARED, KBUILD_MODNAME, (void *)priv);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get irq: %i\n", ret);
		goto err2;
	}

	writel(1, priv->mmio + TBSMOD_REG_INT_EN);

	mutex_init(&priv->spi_mutex);
	mutex_init(&priv->ioctl_mutex);

	// Put AD9789 into reset
	writel(1, priv->mmio + TBSMOD_REG_AD9789_RESET);
	msleep(10);

	/* Init clock driver */
	ret = tbsmod_ad4351_init(priv);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to init ad4351: %i\n", ret);
		goto err3;
	}
	msleep(50);

	// Enable AD9789 SPI interface?
	writel(1, priv->mmio + TBSMOD_REG_SPI_ENABLE);
	writel(0, priv->mmio + TBSMOD_REG_SPI_DEVICE);

	// Take AD9789 out of reset
	writel(0, priv->mmio + TBSMOD_REG_AD9789_RESET);
	msleep(50);

	/* Init AD9789 */
	ret = tbsmod_ad9789_init(priv);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to init ad9789: %i\n", ret);
		goto err3;
	}
	msleep(50);

	// Set Core to QAM256
	writel(1, priv->mmio + TBSMOD_REG_AD9789_MODULATION);

	writel(1, priv->mmio + TBSMOD_REG_RESET_IPCORE);
	msleep(100);
	writel(0, priv->mmio + TBSMOD_REG_RESET_IPCORE);
	msleep(100);

	ret = tbsmod_ad9789_reg_read(priv, TBSMOD_AD9789_HARDWARE_VERSION, &val);
	if (ret) {
		dev_err(&pdev->dev, "failed to get ad9789 hw rev: %i\n", ret);
		goto err3;
	}
	if (val != 3) {
		dev_err(&pdev->dev, "failed to initialize ad9789\n");
		goto err3;
	}
	dev_info(&pdev->dev, "AD9789 hardware version: %x\n", val);

	// Find card number
	for (card_num = 0; card_num < 4; ++card_num) {
		if (!tbsmod_cards[card_num])
			break;
	}
	if (card_num == 4) {
		dev_err(&pdev->dev, "failed to initialize ad9789\n");
		goto err3;
	}
	priv->card_num = card_num;
	tbsmod_cards[card_num] = priv;

	// Setup channels
	for (i = 0; i < 4; ++i) {
		struct device *dev;
		int dev_num = (card_num * 4) + i;

		priv->channel[i].priv = priv;
		priv->channel[i].num = i;

		priv->channel[i].dma_reg = priv->mmio + TBSMOD_REG_DMA_BASE(i);
		priv->channel[i].dmavirt = pci_alloc_consistent(pdev, TBSMOD_DMASIZE, &priv->channel[i].dmaphy);
		if (!priv->channel[i].dmavirt) {
			dev_err(&pdev->dev, "pci_alloc_consistent() failed\n");
			continue;
		}
		writel(0, priv->channel[i].dma_reg + TBSMOD_REG_DMA_EN);
		writel(priv->channel[i].dmaphy >> 32, priv->channel[i].dma_reg + TBSMOD_REG_DMA_ADDR_HIGH);
		writel(priv->channel[i].dmaphy, priv->channel[i].dma_reg + TBSMOD_REG_DMA_ADDR_LOW);
		writel(465063, priv->channel[i].dma_reg + TBSMOD_REG_DMA_SPEED_CTRL);
		writel(465063*2, priv->channel[i].dma_reg + TBSMOD_REG_DMA_INT_MONITOR);
		writel(TBSMOD_BLOCKSIZE, priv->channel[i].dma_reg + TBSMOD_REG_DMA_SIZE);

		writel(1, priv->mmio + TBSMOD_REG_DMA_IE(i));	// Enable chanel DMA interrupt?

		mutex_init(&priv->channel[i].lock);
		init_waitqueue_head(&priv->channel[i].wait_queue);

		ret = kfifo_alloc(&priv->channel[i].fifo, TBSMOD_FIFOSIZE, GFP_KERNEL);
		if (ret) {
			dev_err(&pdev->dev, "kfifo_alloc() failed: %i\n", ret);
			pci_free_consistent(pdev, TBSMOD_DMASIZE, priv->channel[i].dmavirt, priv->channel[i].dmaphy);
			priv->channel[i].dmavirt = NULL;
			continue;
		}

		// Create device node
		dev = device_create(tbsmod_class, &priv->pdev->dev, MKDEV(tbsmod_major, dev_num), &priv->channel[i], "tbsmod%d", dev_num);
		if (IS_ERR(dev)) {
			dev_err(&pdev->dev, "device_create() failed: %li\n", PTR_ERR(dev));
			pci_free_consistent(pdev, TBSMOD_DMASIZE, priv->channel[i].dmavirt, priv->channel[i].dmaphy);
			priv->channel[i].dmavirt = NULL;
		}
	}

	dev_info(&pdev->dev, "TBS 6014 modulator initialized\n");

	return 0;

err3:
	free_irq(pdev->irq, priv);
err2:
	iounmap(priv->mmio);
err1:
	pci_disable_device(pdev);
err0:
	pci_release_region(pdev, 0);
err:
	pci_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, priv);

	return ret;
}

static void tbsmod_remove(struct pci_dev *pdev)
{
	struct tbsmod *priv = (struct tbsmod *)pci_get_drvdata(pdev);
	int i;

	writel(0, priv->mmio + TBSMOD_REG_INT_EN);
	writel(0xff, priv->mmio + TBSMOD_REG_INT_CLEAR);

	// Remove channels
	for (i = 0; i < 4; ++i) {
		int dev_num = (priv->card_num * 4) + i;

		kfifo_free(&priv->channel[i].fifo);

		device_destroy(tbsmod_class, MKDEV(tbsmod_major, dev_num));

		if (priv->channel[i].dmavirt)
			pci_free_consistent(pdev, TBSMOD_DMASIZE, priv->channel[i].dmavirt, priv->channel[i].dmaphy);
	}

	tbsmod_cards[priv->card_num] = NULL;

	free_irq(pdev->irq, priv);
	pci_disable_msi(pdev);

	iounmap(priv->mmio);

	pci_disable_device(pdev);

	pci_release_region(pdev, 0);

	pci_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, priv);
}

static const struct pci_device_id tbsmod_id_table[] = {
	{
		.vendor = 0x544d,
		.device = 0x6178,
		.subvendor = 0x6014,
		.subdevice = 0x0001,
		//.driver_data = NULL,
	},
	{ }
};
MODULE_DEVICE_TABLE(pci, tbsmod_id_table);

static struct pci_driver tbsmod_pci_driver = {
	.name = "tbsmod",
	.id_table = tbsmod_id_table,
	.probe = tbsmod_probe,
	.remove = tbsmod_remove,
};

static int __init tbsmod_module_init(void)
{
	dev_t dev;
	int ret;

	ret = alloc_chrdev_region(&dev, 0, TBSMOD_MAX_DEVICES, "tbsmod");
	if (ret < 0)
		goto err1;

	tbsmod_major = MAJOR(dev);

	tbsmod_class = class_create(THIS_MODULE, "tbsmod");
	if (IS_ERR(tbsmod_class)) {
		ret = PTR_ERR(tbsmod_class);
		goto err2;
	}

	cdev_init(&tbsmod_cdev, &tbsmod_fops);
	ret = cdev_add(&tbsmod_cdev, dev, TBSMOD_MAX_DEVICES);
	if (ret < 0)
		goto err3;

	ret = pci_register_driver(&tbsmod_pci_driver);
	if (ret < 0)
		goto err3;

	return 0;

err3:
	class_destroy(tbsmod_class);
err2:
	unregister_chrdev_region(dev, 4);
err1:
	return ret;
}

static void __exit tbsmod_module_exit(void)
{
	int i;

	for (i = 0; i < TBSMOD_MAX_DEVICES; ++i)
		device_destroy(tbsmod_class, MKDEV(tbsmod_major, i));

	class_destroy(tbsmod_class);

	cdev_del(&tbsmod_cdev);

	unregister_chrdev_region(MKDEV(tbsmod_major, 0), TBSMOD_MAX_DEVICES);

	pci_unregister_driver(&tbsmod_pci_driver);
}

module_init(tbsmod_module_init);
module_exit(tbsmod_module_exit);

MODULE_DESCRIPTION("TBS PCIe Modulator");
MODULE_AUTHOR("Nathan Ford <nford@westpond.com>");
MODULE_LICENSE("GPL");
