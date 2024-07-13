//#include <config.h>	
//#include <mach/clkcore.h>
//#include <mach/ddr.h>
//#include <mach/gpio.h>
//#include <asm/io.h>
//#include <mach/serdes.h>
#include <common.h>
#include <command.h>
#include <errno.h>
#include <asm/io.h>
#include <mach/comcerto-2000.h>
#include <mach/gpio.h>


#define LED_REG_ADDR        COMCERTO_GPIO_OUTPUT_REG

#define SYSTEM_LED_RED      GPIO_7
#define SYSTEM_LED_GREEN    GPIO_5
#define SYSTEM_LED_BLUE     GPIO_6
#define SYSTEM_LED_YELLOW   (SYSTEM_LED_RED | SYSTEM_LED_GREEN)
#define SYSTEM_LED_WHITE    (SYSTEM_LED_RED | SYSTEM_LED_GREEN | SYSTEM_LED_BLUE)
#define SYSTEM_LED_ALL      SYSTEM_LED_WHITE

#define WIFI_LED_YELLOW     GPIO_12
#define WIFI_LED_BLUE       GPIO_13
#define WIFI_LED_ALL        (WIFI_LED_YELLOW | WIFI_LED_BLUE)

#define LED_OFF             0


void set_led(u32 mask, u32 value) {
	u32 regval;

	regval = readl(LED_REG_ADDR);
	regval &= ~mask;
	regval |= (value & mask); 
	writel(regval, LED_REG_ADDR);
}

static int do_setled(struct command *cmdtp, int argc, char *argv[]) {
	if (argc < 3)
		return COMMAND_ERROR_USAGE;

	printf("arg1 char: %c\n", argv[1][0]);
	if (argv[1][0] == 's') {           // system_led
		switch(argv[2][0]) {
			case 'r':
				set_led(SYSTEM_LED_ALL, SYSTEM_LED_RED);
				break;
			case 'g':
				set_led(SYSTEM_LED_ALL, SYSTEM_LED_GREEN);
				break;
			case 'b':
				set_led(SYSTEM_LED_ALL, SYSTEM_LED_BLUE);
				break;
			case 'y':
				set_led(SYSTEM_LED_ALL, SYSTEM_LED_YELLOW);
				break;
			case 'w':
				set_led(SYSTEM_LED_ALL, SYSTEM_LED_WHITE);
				break;
			case 'o':
				set_led(SYSTEM_LED_ALL, LED_OFF);
				break;
			default:
			break;
		}
		
	} else if (argv[1][0] == 'w') {    // wifi_led
		switch(argv[2][0]) {
			case 'b':
				set_led(WIFI_LED_ALL, WIFI_LED_BLUE);
				break;
			case 'y':
				set_led(WIFI_LED_ALL, WIFI_LED_YELLOW);
				break;
			case 'o':
				set_led(WIFI_LED_ALL, LED_OFF);
				break;
			default:
			break;
		}
	}


//	printf("arg0: %s\n", argv[0]);
//	printf("arg1: %s\n", argv[1]); // LED NAME
//	printf("arg2: %s\n", argv[2]); // LED COLOR

//	switch(argv[1][0]) {
//		case 'r':
//	writel( readl(COMCERTO_GPIO_OE_REG) | GPIO_27, COMCERTO_GPIO_OE_REG);


	return 0;
}

BAREBOX_CMD_HELP_START(led)
BAREBOX_CMD_HELP_USAGE("led [LED Name] [Color]\n")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(led)
	.cmd		= do_setled,
	.usage		= "changes the LED color",
	BAREBOX_CMD_HELP(cmd_led_help)
BAREBOX_CMD_END
