#include <common.h>
#include <command.h>
#include <clock.h>
#include <net.h>
#include <errno.h>
#include <linux/err.h>

#define RXICMP_ENV_VAR          "rxicmp_startup_env"
#define RXICMP_STATE_INIT       0
#define RXICMP_STATE_SUCCESS    1
#define RXICMP_WAIT_TIME        5

static int rxicmp_state;

static struct net_connection *rxicmp_con;



static void rxicmp_handler(void *ctx, char *pkt, unsigned len)
{
	struct icmphdr *icmp = net_eth_to_icmphdr(pkt);

	if (icmp->type == ICMP_ECHO_REQUEST) {
		if ( strncmp( WD_ICMP_ENCODE_MSG, 
			      net_eth_to_icmp_payload(pkt),
		              sizeof(WD_ICMP_ENCODE_MSG)) ) {
			return;
		}
	}
	rxicmp_state = RXICMP_STATE_SUCCESS;
	printf("[INFO]  WD Magical Packet Received!\n");
}

static int do_rxicmp(struct command *cmdtp, int argc, char *argv[])
{
	uint64_t rxicmp_start = 0;
	//net_set_ip(0xac1966d4);

	rxicmp_con = net_icmp_new(0xffffffff, rxicmp_handler, NULL);
	if (IS_ERR(rxicmp_con)) {
		printf("[ERROR] Problem initiating internet connection!\n");
		goto out;
	}

	rxicmp_start = get_time_ns();
	rxicmp_state = RXICMP_STATE_INIT;

	printf("[INFO]  Waiting for WD Magic Packet (%d sec): CTRL+C to skip...\n", RXICMP_WAIT_TIME);
	while ( !is_timeout(rxicmp_start, RXICMP_WAIT_TIME * SECOND) && rxicmp_state == RXICMP_STATE_INIT ) {
		if (ctrlc()) {
			break;
		}
		net_poll();
	}

	net_unregister(rxicmp_con);
	/* XXX: getenv(RXICMP_ENV_VAR) can be replaced by the series of commands
	        contained in rxicmp_startup_script. In that case there is
	        no need for having '/env/bin/rxicmp_startup script' script and 
	        'rxicmp_startup_env' environment variable within '/env/config'.*/
	if ( rxicmp_state == RXICMP_STATE_SUCCESS ) {
		run_command(getenv(RXICMP_ENV_VAR), 0);  
	}

out:
	return rxicmp_state == RXICMP_STATE_SUCCESS ? 0 : 1;
}

BAREBOX_CMD_START(rxicmp)
	.cmd		= do_rxicmp,
	.usage		= "rxicmp",
BAREBOX_CMD_END





