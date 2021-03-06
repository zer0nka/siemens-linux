#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/bitops.h>
#include <mach/pmb8876-gpio.h>

#define PCL_DEF_CONFIG		PMB8876_GPIO(NO_ALT,	NO_ALT,		MANUAL,		IN,		LOW,	PUSHPULL,	NONE,		NO_ENAQ)

struct pmb8876_pcl {
	uint8_t num;
	uint32_t flags;
};

// Стандартная конфигурация пинов GPIO
static struct pmb8876_pcl pcl_pins[] = {
	{	GPIO_MMC_VCC_EN,		PMB8876_GPIO(NO_ALT,	NO_ALT,	MANUAL,	OUT,	LOW,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_MMC_CD,			PMB8876_GPIO(NO_ALT,	NO_ALT,	MANUAL,	OUT,	LOW,	PUSHPULL,	NONE,		ENAQ)	},
	{	GPIO_MMC_CLK,			PMB8876_GPIO(ALT0,		ALT0,	ALT,	IN,		LOW,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_MMC_DAT,			PMB8876_GPIO(ALT0,		ALT0,	ALT,	IN,		HIGH,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_MMC_CMD,			PMB8876_GPIO(ALT0,		ALT0,	ALT,	IN,		HIGH,	PUSHPULL,	NONE,		NO_ENAQ)	},
	
	// USART0
	{	GPIO_USART0_RXD,		PMB8876_GPIO(ALT0,		NO_ALT,	ALT,	IN,		HIGH,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_USART0_TXD,		PMB8876_GPIO(NO_ALT,	ALT0,	ALT,	IN,		HIGH,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_USART0_RTS,		PMB8876_GPIO(NO_ALT,	ALT0,	ALT,	IN,		LOW,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_USART0_CTS,		PMB8876_GPIO(ALT0,		NO_ALT,	ALT,	IN,		HIGH,	PUSHPULL,	NONE,		NO_ENAQ)	},
	
	// USART1
	{	GPIO_USART1_RXD,		PMB8876_GPIO(ALT3,		NO_ALT,	ALT,	IN,		HIGH,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_USART1_TXD,		PMB8876_GPIO(NO_ALT,	NO_ALT,	MANUAL,	OUT,	HIGH,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_USART1_RTS,		PMB8876_GPIO(NO_ALT,	ALT0,	ALT,	IN,		LOW,	PUSHPULL,	NONE,		NO_ENAQ)	},
	{	GPIO_USART1_CTS,		PMB8876_GPIO(ALT0,		NO_ALT,	ALT,	IN,		LOW,	PUSHPULL,	NONE,		NO_ENAQ)	},
	
	// software i2c
	{	GPIO_I2C_SCL,			PMB8876_GPIO(NO_ALT,	NO_ALT,	MANUAL,	OUT,	LOW,	OPENDRAIN,	PULLUP,		NO_ENAQ)	},
	{	GPIO_I2C_SDA,			PMB8876_GPIO(NO_ALT,	NO_ALT,	MANUAL,	OUT,	LOW,	OPENDRAIN,	PULLUP,		NO_ENAQ)	},
};

static int pmb8876_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	uint32_t i, flags;
	
	flags = PCL_DEF_CONFIG;
	for (i = 0; i < sizeof(pcl_pins) / sizeof(pcl_pins[0]); ++i) {
		struct pmb8876_pcl *pcl = &pcl_pins[i];
		if (pcl->num == offset) {
			flags = pcl->flags;
			break;
		}
	}
	
	// Устанавливаем сначала ENAQ, что бы при переключении IS/OS мусор не попал в пины
	if ((pmb8876_gpio_reg_get_is(flags) || pmb8876_gpio_reg_get_os(flags)) && !pmb8876_gpio_reg_get_enaq(flags) && readl((void *) PMB8876_GPIO_PIN(offset)) != flags)
		writel(flags | (1 << PMB8876_GPIO_ENAQ), (void *) PMB8876_GPIO_PIN(offset));
	
	writel(flags, (void *) PMB8876_GPIO_PIN(offset));
	
	return 0;
}

static int pmb8876_gpio_set_single_ended(struct gpio_chip *chop, unsigned int offset, enum single_ended_mode mode)
{
	uint32_t val;
	switch (mode) {
		case LINE_MODE_OPEN_DRAIN:
			val = readl((void *) PMB8876_GPIO_PIN(offset));
			pmb8876_gpio_reg_set_pdpu(val, PMB8876_GPIO_PPEN_OPENDRAIN);
			writel(val, (void *) PMB8876_GPIO_PIN(offset));
			return 0;
		
		case LINE_MODE_PUSH_PULL:
			val = readl((void *) PMB8876_GPIO_PIN(offset));
			pmb8876_gpio_reg_set_pdpu(val, PMB8876_GPIO_PPEN_PUSHPULL);
			writel(val, (void *) PMB8876_GPIO_PIN(offset));
			return 0;
		default:
			return -ENOTSUPP;
	}
}

static int pmb8876_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	return pmb8876_gpio_reg_get_dir(readl((void *) PMB8876_GPIO_PIN(offset))) == PMB8876_GPIO_DIR_OUT ? 0 : 1;
}

static int pmb8876_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	uint32_t val = readl((void *) PMB8876_GPIO_PIN(offset));
	pmb8876_gpio_reg_set_dir(val, PMB8876_GPIO_DIR_IN);
	writel(val, (void *) PMB8876_GPIO_PIN(offset));
	return 0;
}

static int pmb8876_gpio_direction_output(struct gpio_chip *chip, unsigned offset, int value)
{
	uint32_t val = readl((void *) PMB8876_GPIO_PIN(offset));
	pmb8876_gpio_reg_set_dir(val, PMB8876_GPIO_DIR_OUT);
	pmb8876_gpio_reg_set_data(val, value ? 1 : 0);
	writel(val, (void *) PMB8876_GPIO_PIN(offset));
	return 0;
}

static int pmb8876_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	return pmb8876_gpio_reg_get_data(readl((void *) PMB8876_GPIO_PIN(offset)));
}

static void pmb8876_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	uint32_t val = readl((void *) PMB8876_GPIO_PIN(offset));
	pmb8876_gpio_reg_set_data(val, value ? 1 : 0);
	writel(val, (void *) PMB8876_GPIO_PIN(offset));
}

static struct gpio_chip ext_chip = {
	.label				= "gpio",
	.request			= pmb8876_gpio_request,
	.get_direction		= pmb8876_gpio_get_direction,
	.direction_input	= pmb8876_gpio_direction_input,
	.direction_output	= pmb8876_gpio_direction_output,
	.get				= pmb8876_gpio_get,
	.set				= pmb8876_gpio_set,
	.set_single_ended	= pmb8876_gpio_set_single_ended,
	.can_sleep			= false,
	.ngpio				= 0x71, // FIXME: Устал искать, куда засунуть ARCH_NR_GPIOS
	.base				= 0
};

static int __init pmb8876_init_gpio(void)
{
	int ret = gpiochip_add_data(&ext_chip, NULL);
	if (ret) {
		pr_info("pmb8876_init_gpio fail = %d\n", ret);
		return ret;
	}
	
	return 0;
}
arch_initcall(pmb8876_init_gpio);
