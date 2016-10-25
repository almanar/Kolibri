#include "vmtype.h"
#include "vmboard.h"
#include "vmsystem.h"
#include "vmlog.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmthread.h"

#include "spi.h"
#include "cc2500.h"

#include "string.h"

void cc2500_init3(void){

	spi_open();
	cc2500_reset();

	//# Base frequency = 2425.000000
	//# RX filter BW = 270.833333
	//# Address config = No address check
	//# Packet length mode = Fixed packet length mode. Length configured in PKTLEN register
	//# Whitening = false
	//# Channel number = 0
	//# Channel spacing = 249.938965
	//# Modulated = true
	//# Data format = Normal mode
	//# Modulation format = MSK
	//# Data rate = 49.9878
	//# Carrier frequency = 2425.000000
	//# CRC autoflush = false
	//# Sync word qualifier mode = 30/32 sync word bits detected
	//# Device address = 0
	//# Preamble count = 4
	//# CRC enable = true
	//# Manchester enable = false
	//# Phase transition time = 0
	//# Packet length = 18
	//# TX power = -55
	//
	// Rf settings for CC2500
	//
	cc2500_write_reg(CC2500_REG_PATABLE, 0x00);
	cc2500_write_reg(REG_IOCFG1,0x06);  //GDO1Output Pin Configuration
	cc2500_write_reg(REG_IOCFG0,0x06);  //GDO0Output Pin Configuration
	cc2500_write_reg(REG_PKTLEN,0x12);  //Packet Length
	cc2500_write_reg(REG_PKTCTRL0,0x04);//Packet Automation Control
	cc2500_write_reg(REG_FSCTRL1,0x0A); //Frequency Synthesizer Control
	cc2500_write_reg(REG_FREQ2,0x5D);   //Frequency Control Word, High Byte
	cc2500_write_reg(REG_FREQ1,0x44);   //Frequency Control Word, Middle Byte
	cc2500_write_reg(REG_MDMCFG4,0x6A); //Modem Configuration
	cc2500_write_reg(REG_MDMCFG3,0xF8); //Modem Configuration
	cc2500_write_reg(REG_MDMCFG2,0x73); //Modem Configuration
	cc2500_write_reg(REG_MDMCFG1,0x23); //Modem Configuration
	cc2500_write_reg(REG_MDMCFG0,0x3B); //Modem Configuration
	cc2500_write_reg(REG_DEVIATN,0x00); //Modem Deviation Setting
	cc2500_write_reg(REG_MCSM0,0x18);   //Main Radio Control State Machine Configuration
	cc2500_write_reg(REG_FOCCFG,0x3F);  //Frequency Offset Compensation Configuration
	cc2500_write_reg(REG_BSCFG,0x1C);   //Bit Synchronization Configuration
	cc2500_write_reg(REG_AGCCTRL2,0xC7);//AGC Control
	cc2500_write_reg(REG_AGCCTRL1,0x00);//AGC Control
	cc2500_write_reg(REG_AGCCTRL0,0xB0);//AGC Control
	cc2500_write_reg(REG_FREND1,0xB6);  //Front End RX Configuration
	cc2500_write_reg(REG_FSCAL1,0x00);  //Frequency Synthesizer Calibration
	cc2500_write_reg(REG_FSCAL0,0x11);  //Frequency Synthesizer Calibration
}

void dex_RadioSettings()
{
	spi_open();

	cc2500_reset();

	// Transmit power: one of the highest settings, but not the highest.
	cc2500_write_reg(CC2500_REG_PATABLE, 0x00);

	// Set the center frequency of channel 0 to 2403.47 MHz.
	// Freq = 24/2^16*(0xFREQ) = 2403.47 MHz
	// FREQ[23:0] = 2^16*(fCarrier/fRef) = 2^16*(2400.156/24) = 0x6401AA
	//IOCFG2 = 0x0E; //RX_SYMBOL_TICK
	//IOCFG1 = 0x16; //RX_HARD_DATA[1]
	//IOCFG0 = 0x1D; //Preamble Quality Reached
	cc2500_write_reg(REG_IOCFG0, 0x0E);
	cc2500_write_reg(REG_IOCFG1, 0x06);
	cc2500_write_reg(REG_FREQ2, 0x65);
	cc2500_write_reg(REG_FREQ1, 0x0A);
	cc2500_write_reg(REG_FREQ0, 0xAA);
	cc2500_write_reg(REG_SYNC1, 0xD3);
	cc2500_write_reg(REG_SYNC0, 0x91);
	cc2500_write_reg(REG_ADDR, 0x00);

	// Controls the FREQ_IF used for RX.
	// This is affected by MDMCFG2.DEM_DCFILT_OFF according to p.212 of datasheet.
	cc2500_write_reg(REG_FSCTRL1, 0x0A);				// Intermediate Freq.  Fif = FRef x FREQ_IF / 2^10 = 24000000 * 10/1024 = 234375  for 0x0F = 351562.5
	cc2500_write_reg(REG_FSCTRL0, 0x00);				// base freq offset.  This is 1/214 th of the allowable range of frequency compensation
	// which depends on the FOCCFG param for fraction of channel bandwidth to swing (currently 1/8th, maybe should be 1/4).

	// Sets the data rate (symbol rate) used in TX and RX.  See Sec 13.5 of the datasheet.
	// Also sets the channel bandwidth.
	// We tried different data rates: 375 kbps was pretty good, but 400 kbps and above caused lots of packet errors.
	// NOTE: If you change this, you must change RSSI_OFFSET in radio_registers.h


	// Dexcom states channel bandwidth (not spacing) = 334.7 kHz E = 1, M = 0 (MDMCFG4 = 4B)
	// =        24000000 / 8 x (4 + 0) x 2 ^ 1
	// =        24000000 / 64 = 375000
	cc2500_write_reg(REG_MDMCFG4, 0x4B);				// 375kHz BW, DRATE_EXP = 11.
	// Rdata = (256+DRATE_M) x 2 ^ DRATE_E
	//           ------------------------ x Fref = FRef x (256 + DRATE_M) x 2 ^ (DRATE_E-28)
	//                2 ^ 28
	// in our case = 24000000 * (256+17) x 2 ^ (-17) = (24000000 / 131072) * 273 = 49987.79
	cc2500_write_reg(REG_MDMCFG3, 0x11);				// DRATE_M = 0x11 = 17.

	// MDMCFG2.DEM_DCFILT_OFF, 0, enable digital DC blocking filter before
	//   demodulator.  This affects the FREQ_IF according to p.212 of datasheet.
	// MDMCFC2.MANCHESTER_EN, 0 is required because we are using MSK (see Sec 13.9.2)
	// MDMCFG2.MOD_FORMAT, 111: MSK modulation
	// MDMCFG2.SYNC_MODE, 011: 30/32 sync bits received, no requirement on Carrier sense
	cc2500_write_reg(REG_MDMCFG2, 0x73);

	// MDMCFG1.FEC_EN = 0 : 0=Disable Forward Error Correction
	// MDMCFG1.NUM_PREAMBLE = 000 : Minimum number of preamble bytes is 2.
	// MDMCFG1.CHANSPC_E = 3 : Channel spacing exponent.
	// MDMCFG0.CHANSPC_M = 0x55 : Channel spacing mantissa.
	// Channel spacing = (256 + CHANSPC_M)*2^(CHANSPC_E) * f_ref / 2^18
	// = 24000000 x (341) x 2^(3-18) = 24000000 x 341 / 2^15
	// = 249755Hz.
	cc2500_write_reg(REG_MDMCFG1, 0x03);	// no FEC, preamble bytes = 2 (AAAA), CHANSPC_E = 3
	cc2500_write_reg(REG_MDMCFG0, 0x55);	// CHANSPC_M = 0x55 = 85


	cc2500_write_reg(REG_DEVIATN, 0x00);
	// See Sec 13.9.2.

	cc2500_write_reg(REG_FREND1, 0xB6);
	cc2500_write_reg(REG_FREND0, 0x10);

	// F0CFG and BSCFG configure details of the PID loop used to correct the
	// bit rate and frequency of the signal (RX only I believe).
	cc2500_write_reg(REG_FOCCFG, 0x0A);		// allow range of +/1 FChan/4 = 375000/4 = 93750.  No CS GATE
	cc2500_write_reg(REG_BSCFG, 0x6C);

	// AGC Control:
	// This affects many things, including:
	//    Carrier Sense Absolute Threshold (Sec 13.10.5).
	//    Carrier Sense Relative Threshold (Sec 13.10.6).
	cc2500_write_reg(REG_AGCCTRL2, 0x44);
	cc2500_write_reg(REG_AGCCTRL1, 0x00);
	cc2500_write_reg(REG_AGCCTRL0, 0xB2);

	// Frequency Synthesizer registers that are not fully documented.
	cc2500_write_reg(REG_FSCAL3, 0xA9);
	cc2500_write_reg(REG_FSCAL2, 0x0A);
	cc2500_write_reg(REG_FSCAL1, 0x20);
	cc2500_write_reg(REG_FSCAL0, 0x0D);

	// Mostly-undocumented test settings.
	// NOTE: The datasheet says TEST1 must be 0x31, but SmartRF Studio recommends 0x11.
	cc2500_write_reg(REG_TEST2, 0x81);
	cc2500_write_reg(REG_TEST1, 0x35);
	cc2500_write_reg(REG_TEST0, 0x0B);

	// Packet control settings.
	cc2500_write_reg(REG_PKTCTRL1, 0x04);
	cc2500_write_reg(REG_PKTCTRL0, 0x05);		// enable CRC flagging and variable length packets.  Probably could use fix length for our case, since all are same length.
	// but that would require changing the library code, since it sets up buffers etc etc, and I'm too lazy.

}
void cc2500_init(){
	/*
# Modulated = true
# Base frequency = 2424.999756
# Channel number = 0
# RX filter BW = 337.500000
# CRC enable = true
# Modulation format = MSK
# Packet length mode = Fixed packet length mode. Length configured in PKTLEN register
# CRC autoflush = false
# Packet length = 18
# Device address = 0
# Channel spacing = 249.664307
# Preamble count = 4
# Sync word qualifier mode = 30/32 sync word bits detected
# Data format = Normal mode
# TX power = 0
# Manchester enable = false
# Carrier frequency = 2424.999756
# Phase transition time = 0
# Address config = Address check and 0 (0x00) and 255 (0xFF) broadcast
# Data rate = 249.664
# Whitening = false
# PA table
	 */

	//
	// Rf settings for CC2500
	//
	spi_open();

	cc2500_reset();

	cc2500_write_reg(CC2500_REG_PATABLE, 0x00);

	cc2500_write_reg(REG_IOCFG2,   0x02);  //GDO2Output Pin Configuration
	cc2500_write_reg(REG_IOCFG1,   0x06);  //GDO1Output Pin Configuration
	cc2500_write_reg(REG_IOCFG0,   0x02);  //GDO0Output Pin Configuration

	cc2500_write_reg(REG_PKTLEN,   0xff);  //Packet Length
	cc2500_write_reg(REG_PKTCTRL1, 0x04);  //Packet Automation Control
	cc2500_write_reg(REG_PKTCTRL0, 0x05);  //Packet Automation Control
	cc2500_write_reg(REG_ADDR,     0x00);  //Device address

	cc2500_write_reg(REG_FSCTRL1,  0x0f);  //Frequency Synthesizer Control
	cc2500_write_reg(REG_FSCTRL0,  0x00);  //Frequency Synthesizer Control

	cc2500_write_reg(REG_FREQ2,    0x5d);  //Frequency Control Word, High Byte
	cc2500_write_reg(REG_FREQ1,    0x44);  //Frequency Control Word, Middle Byte
	cc2500_write_reg(REG_FREQ0,    0xeb);  //Frequency Control Word, Low Byte

	cc2500_write_reg(REG_MDMCFG4,  0x7a);  //Modem Configuration
	cc2500_write_reg(REG_MDMCFG3,  0xf8);  //Modem Configuration
	cc2500_write_reg(REG_MDMCFG2,  0x73);  //Modem Configuration
	cc2500_write_reg(REG_MDMCFG1,  0x23);  //Modem Configuration
	cc2500_write_reg(REG_MDMCFG0,  0x3b);  //Modem Configuration

	cc2500_write_reg(REG_DEVIATN,  0x40);  //Modem Deviation Setting

	cc2500_write_reg(REG_MCSM2,    0x07);  //Main Radio Control State Machine Configuration
	cc2500_write_reg(REG_MCSM1,    0x30);  //Main Radio Control State Machine Configuration
	cc2500_write_reg(REG_MCSM0,    0x18);  //Main Radio Control State Machine Configuration

	cc2500_write_reg(REG_FOCCFG,   0x0A);  //Frequency Offset Compensation Configuration
	cc2500_write_reg(REG_BSCFG,    0x6C);  //Bit Synchronization Configuration

	cc2500_write_reg(REG_AGCCTRL2, 0x03);  //AGC Control
	cc2500_write_reg(REG_AGCCTRL1, 0x00);  //AGC Control
	cc2500_write_reg(REG_AGCCTRL0, 0x91);  //AGC Control

	cc2500_write_reg(REG_FREND1,   0xB6);  //Front End RX Configuration
	cc2500_write_reg(REG_FREND0,   0x10);  //Front End RX Configuration

	cc2500_write_reg(REG_FSCAL3,   0xa9);  //Frequency Synthesizer Calibration
	cc2500_write_reg(REG_FSCAL2,   0x0a);  //Frequency Synthesizer Calibration
	cc2500_write_reg(REG_FSCAL1,   0x00);  //Frequency Synthesizer Calibration
	cc2500_write_reg(REG_FSCAL0,   0x11);  //Frequency Synthesizer Calibration

	cc2500_write_reg(REG_TEST2,    0x81);  //Various Test Settings
	cc2500_write_reg(REG_TEST1,    0x35);  //Various Test Settings
}

void cc2500_init2()
{
	spi_open();
	cc2500_reset();

	cc2500_write_reg(CC2500_REG_PATABLE, 0x00);
	cc2500_write_reg(REG_IOCFG1, 0x06);
	cc2500_write_reg(REG_PKTLEN, 0xFF); // 0x12
	cc2500_write_reg(REG_PKTCTRL1, 0x04); // 0x05
	cc2500_write_reg(REG_PKTCTRL0, 0x05); // 0x04
	cc2500_write_reg(REG_ADDR, 0x00); // 0xFF
	cc2500_write_reg(REG_CHANNR, 0x00);

	cc2500_write_reg(REG_FSCTRL1, 0x0f);
	cc2500_write_reg(REG_FSCTRL0, 0x00);

	cc2500_write_reg(REG_FREQ2, 0x5d);
	cc2500_write_reg(REG_FREQ1, 0x44);
	cc2500_write_reg(REG_FREQ0, 0xeb);

	cc2500_write_reg(REG_FREND1, 0xb6);
	cc2500_write_reg(REG_FREND0, 0x10);

	// Bandwidth
	//0x4a = 406 khz
	//0x5a = 325 khz
	// 300 khz is supposedly what dex uses...
	//0x6a = 271 khz
	//0x7a = 232 khz //appear to get better sensitivity
	cc2500_write_reg(REG_MDMCFG4, 0x7a);
	cc2500_write_reg(REG_MDMCFG3, 0xf8);
	cc2500_write_reg(REG_MDMCFG2, 0x73); // 0xF3
	cc2500_write_reg(REG_MDMCFG1, 0x23);
	cc2500_write_reg(REG_MDMCFG0, 0x3b);

	cc2500_write_reg(REG_DEVIATN, 0x40);

	cc2500_write_reg(REG_MCSM2, 0x07);
	cc2500_write_reg(REG_MCSM1, 0x30);
	cc2500_write_reg(REG_MCSM0, 0x18);
	cc2500_write_reg(REG_FOCCFG, 0x16);  // 0x36
	cc2500_write_reg(REG_FSCAL3, 0xa9);
	cc2500_write_reg(REG_FSCAL2, 0x0a);
	cc2500_write_reg(REG_FSCAL1, 0x00);
	cc2500_write_reg(REG_FSCAL0, 0x11);

	cc2500_write_reg(REG_AGCCTRL2, 0x03);
	cc2500_write_reg(REG_AGCCTRL1, 0x00);
	cc2500_write_reg(REG_AGCCTRL0, 0x91);
	//
	cc2500_write_reg(REG_TEST2, 0x81);
	cc2500_write_reg(REG_TEST1, 0x35);
	cc2500_write_reg(REG_TEST0, 0x0b);

	cc2500_write_reg(REG_FOCCFG, 0x0A);		// allow range of +/1 FChan/4 = 375000/4 = 93750.  No CS GATE
	cc2500_write_reg(REG_BSCFG, 0x6C);

}

void cc2500_reset()
{
	cc2500_send_strobe(CC2500_CMD_SRES);
}

VMUINT8 cc2500_write_reg(VMUINT8 addr, VMUINT8 value)
{
	set_cs(LOW);

	while (get_miso() == HIGH) {
	};

	SPI_transfer_byte(addr);

	VMUINT8 y = SPI_transfer_byte(value);
	set_cs(HIGH);
	return y;
}

VMUINT8 cc2500_read_reg(VMUINT8 addr)
{
	addr = addr + 0x80;
	set_cs(LOW);
	while (get_miso() == HIGH) {
	};
	VMUINT8 x = SPI_transfer_byte(addr);

	VMUINT8 y = SPI_transfer_byte(0);
	set_cs(HIGH);
	return y;
}

VMUINT8 cc2500_read_burst_reg(VMUINT8 addr, VMUINT8 *dest[], VMUINT8 count)
{
	VMUINT8 buffer[count];

	addr = addr | 0xC0;
	set_cs(LOW);
	while (get_miso() == HIGH) {};

	VMUINT8 x = SPI_transfer_byte(addr);

	int i;
	for (i = 0; i < count; i++)
	{
		buffer[i] = SPI_transfer_byte(0);
	}
	set_cs(HIGH);

	memcpy(dest, buffer, count);

	return x;
}


// For status/strobe addresses, the BURST bit selects between status registers
// and command strobes.
VMUINT8 cc2500_read_status_reg(VMUINT8 addr)
{
	addr = addr | 0xC0;
	set_cs(LOW);
	while (get_miso() == HIGH) {
	};
	VMUINT8 x = SPI_transfer_byte(addr);

	VMUINT8 y = SPI_transfer_byte(0);
	set_cs(HIGH);
	return y;
}




VMUINT8 cc2500_send_strobe(VMUINT8 strobe)
{
	set_cs(LOW);

	while (get_miso() == HIGH) {
	};

	VMUINT8 result =  SPI_transfer_byte(strobe);
	set_cs(HIGH);

	return result;
}

void cc2500_read_config_regs(void)
{
	vm_log_info("Register Configuration\n");
	vm_log_info("REG_IOCFG0:%02X\n", cc2500_read_reg(REG_IOCFG0));
	vm_log_info("REG_IOCFG1:%02X\n", cc2500_read_reg(REG_IOCFG1));
	vm_log_info("REG_IOCFG2:%02X\n", cc2500_read_reg(REG_IOCFG2));
	vm_log_info("REG_PKTLEN:%02X\n", cc2500_read_reg(REG_PKTLEN));
	vm_log_info("REG_PKTCTRL1:%02X\n", cc2500_read_reg(REG_PKTCTRL1));
	vm_log_info("REG_PKTCTRL0:%02X\n", cc2500_read_reg(REG_PKTCTRL0));
	vm_log_info("REG_ADDR:%02X\n", cc2500_read_reg(REG_ADDR));
	vm_log_info("REG_CHANNR:%02X\n", cc2500_read_reg(REG_CHANNR));
	vm_log_info("REG_FSCTRL1:%02X\n", cc2500_read_reg(REG_FSCTRL1));
	vm_log_info("REG_FREQ2:%02X\n", cc2500_read_reg(REG_FREQ2));
	vm_log_info("REG_FREQ1:%02X\n", cc2500_read_reg(REG_FREQ1));
	vm_log_info("REG_FREQ0:%02X\n", cc2500_read_reg(REG_FREQ0));
	vm_log_info("REG_MDMCFG4:%02X\n", cc2500_read_reg(REG_MDMCFG4));
	vm_log_info("REG_MDMCFG3:%02X\n", cc2500_read_reg(REG_MDMCFG3));
	vm_log_info("REG_MDMCFG2:%02X\n", cc2500_read_reg(REG_MDMCFG2));
	vm_log_info("REG_MDMCFG1:%02X\n", cc2500_read_reg(REG_MDMCFG1));
	vm_log_info("REG_MDMCFG0:%02X\n", cc2500_read_reg(REG_MDMCFG0));
	vm_log_info("REG_DEVIATN:%02X\n", cc2500_read_reg(REG_DEVIATN));
	vm_log_info("REG_MCSM2:%02X\n", cc2500_read_reg(REG_MCSM2));
	vm_log_info("REG_MCSM1:%02X\n", cc2500_read_reg(REG_MCSM1));
	vm_log_info("REG_MCSM0:%02X\n", cc2500_read_reg(REG_MCSM0));
	vm_log_info("REG_FOCCFG:%02X\n", cc2500_read_reg(REG_FOCCFG));
	vm_log_info("REG_FSCAL3:%02X\n", cc2500_read_reg(REG_FSCAL3));
	vm_log_info("REG_FSCAL2:%02X\n", cc2500_read_reg(REG_FSCAL2));
	vm_log_info("REG_FSCAL1:%02X\n", cc2500_read_reg(REG_FSCAL1));
	vm_log_info("REG_FSCAL0:%02X\n", cc2500_read_reg(REG_FSCAL0));
	vm_log_info("REG_TEST2:%02X\n", cc2500_read_reg(REG_TEST2));
	vm_log_info("REG_TEST1:%02X\n", cc2500_read_reg(REG_TEST1));
}
