#include "vmtype.h"
#include "vmboard.h"
#include "vmsystem.h"
#include "vmlog.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmthread.h"
#include "vmdatetime.h"
#include "ResID.h"

#include "string.h"

#include "cc2500.h"
#include "dexcomg4.h"
#include "extern.h"
#include "config.h"

dexcom_g4_packet dexg4_packet={0};
VMUINT32 RX_POLL_TIME =0;

static VMUINT32  RAW_SRC_NAME;
static VMUINT8  ASCII_SRC_NAME[6];

static VMUINT8  CHAN_HOPP[4]     = {0,100,199,209};
static VMINT8   CHAN_OFFSET[4]   = {0,0,0,0};//{0xE4, 0xE3, 0xE2, 0xE2};
static VMUINT32 RX_CH_TIMEOUT[4] = { 500 , 1000 , 1500 , 2100 };


VMUINT8  SRC_NAME_TABLE[32] = { '0', '1', '2', '3', '4', '5', '6', '7',
								'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
								'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P',
								'Q', 'R', 'S', 'T', 'U', 'W', 'X', 'Y'
							  };


void delay(int x){vm_thread_sleep(x);};
VMUINT32 microsec(){return vm_time_ust_get_count();};
VMUINT32 milisec(){delay(1); return vm_time_ust_get_count()/1000;};


VMUINT8 get_packet_passed_checksum (VMUINT8 lqi)
{
	return ((lqi & 0x80) == 0x80) ? 1 : 0;
}

VMUINT8 bit_reverse_byte (VMUINT8 in)
{
	VMUINT8  bRet = 0;
	if (in & 0x01)
		bRet |= 0x80;
	if (in & 0x02)
		bRet |= 0x40;
	if (in & 0x04)
		bRet |= 0x20;
	if (in & 0x08)
		bRet |= 0x10;
	if (in & 0x10)
		bRet |= 0x08;
	if (in & 0x20)
		bRet |= 0x04;
	if (in & 0x40)
		bRet |= 0x02;
	if (in & 0x80)
		bRet |= 0x01;
	return bRet;
}

VMUINT32 get_src_value (VMUINT8 srcVal)
{
	VMUINT8 i = 0;
	for (i = 0; i < 32; i++)
	{
		if (SRC_NAME_TABLE[i] == srcVal)
			break;
	}
	return i & 0xFF;
}

VMUINT32 ascii_to_dexcom_src (VMUINT8 addr[6])
{
	VMUINT32  src = 0;
	src |= (get_src_value (addr[0]) << 20);
	src |= (get_src_value (addr[1]) << 15);
	src |= (get_src_value (addr[2]) << 10);
	src |= (get_src_value (addr[3]) << 5);
	src |=  get_src_value (addr[4]);
	return src;
}

void bit_reverse_bytes (VMUINT8 * buf, VMUINT8 nLen)
{
	VMUINT8 i = 0;
	for (; i < nLen; i++)
	{
		buf[i] = bit_reverse_byte (buf[i]);
	}
}

VMUINT32 dex_num_decoder (VMUINT16 usShortFloat)
{
	VMUINT16 usReversed = usShortFloat;
	VMUINT8 usExponent = 0;
	VMUINT32 usMantissa = 0;
	bit_reverse_bytes ((VMUINT8 *) & usReversed, 2);
	usExponent = ((usReversed & 0xE000) >> 13);
	usMantissa = (usReversed & 0x1FFF);
	return usMantissa << usExponent;
}

void dexcom_src_to_ascii (VMUINT32 psrc_addr)
{
	ASCII_SRC_NAME[0] = SRC_NAME_TABLE[(psrc_addr >> 20) & 0x1F];
	ASCII_SRC_NAME[1] = SRC_NAME_TABLE[(psrc_addr >> 15) & 0x1F];
	ASCII_SRC_NAME[2] = SRC_NAME_TABLE[(psrc_addr >> 10) & 0x1F];
	ASCII_SRC_NAME[3] = SRC_NAME_TABLE[(psrc_addr >> 5) & 0x1F];
	ASCII_SRC_NAME[4] = SRC_NAME_TABLE[(psrc_addr >> 0) & 0x1F];
	ASCII_SRC_NAME[5] = 0;
}

void dex4_change_channel(VMUINT8 index)
{
	cc2500_send_strobe(CC2500_CMD_SIDLE);
	cc2500_write_reg(REG_CHANNR, CHAN_HOPP[index]);
	cc2500_write_reg(REG_FSCTRL0, CHAN_OFFSET[index]);

	cc2500_send_strobe(CC2500_CMD_SFRX);
	cc2500_send_strobe(CC2500_CMD_SRX);

	while ((cc2500_read_status_reg(REG_MARCSTATE) & 0x1F) != 0x0D) {};
}

VMBOOL dexg4_set_src(void){
	if (strlen(CONF_TRANSMITTER_ID)){
		RAW_SRC_NAME = ascii_to_dexcom_src((char *)CONF_TRANSMITTER_ID);
		vm_log_info("dexg4_set_src - RAW_SRC_NAME: %lu", RAW_SRC_NAME);
		return 1;
	}else{ return 0; }
}

VMUINT dexg4_init(void){
	cc2500_init2();
	cc2500_read_config_regs();
}

VMUINT8 dexg4_wake(void){
	return cc2500_send_strobe(CC2500_CMD_SNOP);
}

VMUINT8 dexg4_sleep(void){
	return cc2500_send_strobe(CC2500_CMD_SPWD);
}

VMUINT8 dexg4_receive(void){
	vm_log_info("dexg4_recieve");
	VMUINT8  rx_packet_status = 0;
	VMUINT8  channel = 0;
	VMUINT32 post_ch_timmer[4] = {0};

	delay(10);

	do {
		vm_log_info("channel: %d", channel);
		dex4_change_channel(channel);

		while( get_miso() == 0 ){
			if (RX_POLL_TIME == 0){ delay(1); continue; }
			else if ( milisec() > ( RX_POLL_TIME +
									RX_CH_TIMEOUT[channel] ) ){
				break;
			}}
		post_ch_timmer[channel] = milisec();

		delay(50);
		//vm_log_info("slen: %d", cc2500_read_status_reg(CC2500_REG_RXBYTES));
		cc2500_read_burst_reg(CC2500_REG_RXFIFO, &dexg4_packet, 0x15);
		vm_log_info("len: %d", dexg4_packet.pkt_len);
		if ( dexg4_packet.pkt_len == 0x12 ){
			if (get_packet_passed_checksum(dexg4_packet.lqi)){
				if ( dexg4_packet.src_addr == RAW_SRC_NAME ){
					if (RX_POLL_TIME == 0 && channel == 0){
						// save timer time - 450 ms
						RX_POLL_TIME = post_ch_timmer[0] - 450;
					}

					// save offsets
					CHAN_OFFSET[channel] += cc2500_read_status_reg(0x32);
					rx_packet_status = 1;

				// Filter out unwanted behavior
				}else{
					vm_log_info("NOT MY DEX ID: %lu", dexg4_packet.src_addr);
					memset(&dexg4_packet, 0, sizeof(dexg4_packet));
					continue;
				}
			}else{ memset(&dexg4_packet, 0, sizeof(dexg4_packet)); }
		}else{ memset(&dexg4_packet, 0, sizeof(dexg4_packet)); }

		// Stay on channel 0 first time
		if(RX_POLL_TIME != 0){ channel++; }
		// Exit when all channels are swept
		if(channel==4){break;}

	} while (!rx_packet_status);

	RX_POLL_TIME = RX_POLL_TIME + 300000;

	vm_log_info("DEX ID: %lu", dexg4_packet.src_addr);

	return rx_packet_status;
}


/*-------------------------------Dummy----------------------------------*/
VMUINT8 DUMMY_RX_BUFFER[21] = { 0x12,
								0xFF,0xFF,0xFF,0xFF,
								0x4D,0x6E,0x64,0x00,
								0x3F,
								0x03,
								0x93,
								0x93,0xCD,
								0x1D,0xC9,
								0xD2,
								0x00,
								0xAD,
								0xB0,
								0x40};

VMBOOL dexg4_receive_dummy(void){
	int channel=0;

	if (RX_POLL_TIME == 0) { delay(5000);}
	memcpy(&dexg4_packet, &DUMMY_RX_BUFFER, sizeof(DUMMY_RX_BUFFER));

	RX_POLL_TIME = milisec() - 475;

	if ( dexg4_packet.pkt_len == 0x12 ){
		if (get_packet_passed_checksum(dexg4_packet.lqi)){
			dexcom_src_to_ascii(dexg4_packet.src_addr);
			if ( dexg4_packet.src_addr == RAW_SRC_NAME ){
				if (RX_POLL_TIME == 0 && channel == 0){
					RX_POLL_TIME = milisec() - 475;
				}
			}
		}
	}

	RX_POLL_TIME = RX_POLL_TIME + 60000;

	vm_log_info("DEX ID: %s", dexg4_packet.src_addr);
	vm_log_info("Dex id %08x", dexg4_packet.src_addr);
}
/*-------------------------------Dummy----------------------------------*/
