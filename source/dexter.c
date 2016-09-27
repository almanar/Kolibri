#include "vmtype.h"
#include "vmboard.h"
#include "vmsystem.h"
#include "vmlog.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmthread.h"
#include "vmdatetime.h"
#include "ResID.h"

#include "cc2500.h"
#include "string.h"
#include "dexter.h"

VMUCHAR RX_BUFFER[4][24];
/*
0	VMUCHAR     pkt_len;
1	VMUCHAR     dest_addr1;
2	VMUCHAR     dest_addr2;
3	VMUCHAR     dest_addr3;
4	VMUCHAR     dest_addr4;
5	VMUCHAR     src_addr1;
6	VMUCHAR     src_addr2;
7	VMUCHAR     src_addr3;
8	VMUCHAR     src_addr4;
9	VMUCHAR     port;
10	VMUCHAR     count;
11	VMUCHAR     transaction_id;
12	VMUCHAR     raw_data1;
13	VMUCHAR     raw_data2;
14	VMUCHAR     filtered_data1;
15	VMUCHAR     filtered_data2;
16	VMUCHAR     battery;
17	VMUCHAR     fcs;
18	VMUCHAR     crc;
19	VMUCHAR     rssi;
20	VMUCHAR     lqi;
 */


VMUINT RX_POLL_TIME =0;
VMUINT RX_uS_TIMEOUT[4] = {0,0,0,0};

static VMUCHAR dynamic_transmitter_id[6];
volatile VMUINT32 dex_tx_id;
VMUCHAR  SrcNameTable[32] = { '0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
		'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P',
		'Q', 'R', 'S', 'T', 'U', 'W', 'X', 'Y'
};

static VMUINT8 nChannels[4] = {0,100,199,209};
VMINT8 fOffset[4] = {0xe4, 0xe3, 0xe2, 0xe2};

int microsec(){return vm_time_ust_get_count();};

void delay(int x){vm_thread_sleep(x);};


void swap_channel(VMUINT8 channel, VMUINT8 newFSCTRL0)
{
	cc2500_send_strobe(CC2500_CMD_SIDLE);
	cc2500_write_reg(REG_CHANNR, channel);
	cc2500_write_reg(REG_FSCTRL0, newFSCTRL0);

	cc2500_send_strobe(CC2500_CMD_SRX);
	while ((cc2500_read_status_reg(REG_MARCSTATE) & 0x1F) != 0x0D) {};
}

VMUINT32
getSrcValue (VMUCHAR srcVal)
{
	VMUINT8 i = 0;
	for (i = 0; i < 32; i++)
	{
		if (SrcNameTable[i] == srcVal)
			break;
	}
	return i & 0xFF;
}

VMUINT32
asciiToDexcomSrc (VMUCHAR addr[6])
{
	VMUINT32  src = 0;
	src |= (getSrcValue (addr[0]) << 20);
	src |= (getSrcValue (addr[1]) << 15);
	src |= (getSrcValue (addr[2]) << 10);
	src |= (getSrcValue (addr[3]) << 5);
	src |= getSrcValue (addr[4]);
	return src;
}


void
bit_reverse_bytes (VMUINT8 * buf, VMUINT8 nLen)
{
	VMUINT8 i = 0;
	for (; i < nLen; i++)
	{
		buf[i] = bit_reverse_byte (buf[i]);
	}
}

VMUINT32
dex_num_decoder (VMUINT16 usShortFloat)
{
	VMUINT16 usReversed = usShortFloat;
	VMUINT8 usExponent = 0;
	VMUINT32 usMantissa = 0;
	bit_reverse_bytes ((VMUINT8 *) & usReversed, 2);
	usExponent = ((usReversed & 0xE000) >> 13);
	usMantissa = (usReversed & 0x1FFF);
	return usMantissa << usExponent;
}



void
dexcom_src_to_ascii ()
{
	dynamic_transmitter_id[0] = SrcNameTable[(dex_tx_id >> 20) & 0x1F];
	dynamic_transmitter_id[1] = SrcNameTable[(dex_tx_id >> 15) & 0x1F];
	dynamic_transmitter_id[2] = SrcNameTable[(dex_tx_id >> 10) & 0x1F];
	dynamic_transmitter_id[3] = SrcNameTable[(dex_tx_id >> 5) & 0x1F];
	dynamic_transmitter_id[4] = SrcNameTable[(dex_tx_id >> 0) & 0x1F];
	dynamic_transmitter_id[5] = 0;
}

VMUINT dexcom_packet_reciever(){
	VMUINT8 channel = 0;
	VMUCHAR pkt_len = 0;
	VMUINT current_rtc=0;
	VMUINT32 us_time_stamp;

	vm_time_get_unix_time(&current_rtc);
	vm_log_info("1. RTC : %d",current_rtc);
	while(RX_POLL_TIME>current_rtc){
		vm_time_get_unix_time(&current_rtc);
	}
	RX_POLL_TIME = current_rtc + 300;

	channel = 0;
	while(channel <= 3){
		us_time_stamp = microsec();
		vm_log_info("3. MAINWHILE : %d",channel);
		swap_channel(nChannels[channel], fOffset[channel]);
		if(RX_uS_TIMEOUT[channel] == 0){
			vm_log_info("4. NOT TIMED ");
			while(get_miso()==0){}
			if (channel == 0){
				vm_time_get_unix_time(&RX_POLL_TIME);
				RX_POLL_TIME += 300;
				RX_uS_TIMEOUT[channel] = 450000;
			}else{
				RX_uS_TIMEOUT[channel] = vm_time_ust_get_duration(us_time_stamp,
						microsec());
			}
		}else{
			while(microsec() < (us_time_stamp + RX_uS_TIMEOUT[channel])){}
			vm_log_info("5. TIMED");
		}
		delay(50);
		pkt_len = cc2500_read_status_reg(0x3B);
		if(pkt_len == 0x15){
			cc2500_read_burst_reg(CC2500_REG_RXFIFO, RX_BUFFER[channel],
					pkt_len);
			if ( 0x80 <= RX_BUFFER[channel][20]) {
				fOffset[channel] += cc2500_read_status_reg(REG_FREQEST);
			}else{
				RX_BUFFER[channel][0] = '0';
			}
		}else{
			cc2500_send_strobe(CC2500_CMD_SFRX);
			RX_BUFFER[channel][0] = '\0';
		}
		channel++;
	}

	VMUCHAR status=0x00;
	if(RX_BUFFER[0][0] == 18){status = status | 0x01;}
	if(RX_BUFFER[1][0] == 18){status = status | 0x02;}
	if(RX_BUFFER[2][0] == 18){status = status | 0x04;}
	if(RX_BUFFER[3][0] == 18){status = status | 0x08;}

	return status;
}
