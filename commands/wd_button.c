#include <common.h>
#include <command.h>
#include <errno.h>
#include <clock.h>
#include <asm/io.h>
#include <mach/comcerto-2000.h>
#include <mach/gpio.h>

#define BUTTON_REG_ADDR         COMCERTO_GPIO_INPUT_REG
#define WPS_BTN                 GPIO_1    // Active-low button
#define RESET_BTN               GPIO_2    // Active-low button
#define ALL_BTN                 (WPS_BTN | RESET_BTN)
#define BTN_ENV_VAR             "btn_status"


#define BUTTON_WAIT_TIME        1


static int which_button_pressed(void) {
	u32 reg_val = 0;
	u32 cmp_val = 0;

	reg_val = readl(BUTTON_REG_ADDR);  // Read current register value
	cmp_val = (~reg_val & ALL_BTN);      // Buttons are active low

	switch (cmp_val) {
		case WPS_BTN:                // WPS button is pressed
			return 1;
			break;
		case RESET_BTN:              // Reset button is pressed
			return 2;
			break;
		case ALL_BTN:                // Both are pressed
			return 3;
			break;
		default:                     // None is pressed
			return 0;
			break;
	}
}


static int do_get_button_status(struct command *cmdtp, int argc, char *argv[])
{
	uint64_t get_button_start = 0;
	int retval = -1;
	char str[2];

	get_button_start = get_time_ns();

	while ( !is_timeout(get_button_start, BUTTON_WAIT_TIME * SECOND) ) {
		retval = which_button_pressed();

		if (ctrlc() || retval != 0) {
			break;
		}
	}

	sprintf(str, "%d", retval);
	setenv(BTN_ENV_VAR, str);
	printf("Button VAR: %s set to %d\n", BTN_ENV_VAR, retval);


	return 0;
}

BAREBOX_CMD_START(get_button_status)
	.cmd		= do_get_button_status,
	.usage		= "get_button_status",
BAREBOX_CMD_END


