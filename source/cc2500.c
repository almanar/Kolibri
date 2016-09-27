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

VMUCHAR BUFFER[64] = {0};

void cc2500_init()
{
	cc2500_reset();

	cc2500_write_reg(CC2500_REG_PATABLE, 0x00);
	cc2500_write_reg(REG_IOCFG1, 0x06);
	cc2500_write_reg(REG_PKTLEN, 0xff);
	cc2500_write_reg(REG_PKTCTRL1, 0x04);
	cc2500_write_reg(REG_PKTCTRL0, 0x05);
	cc2500_write_reg(REG_ADDR, 0x00);
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
	//0x7a = 232 khz
	cc2500_write_reg(REG_MDMCFG4, 0x7a); //appear to get better sensitivity
	cc2500_write_reg(REG_MDMCFG3, 0xf8);
	cc2500_write_reg(REG_MDMCFG2, 0x73);
	cc2500_write_reg(REG_MDMCFG1, 0x23);
	cc2500_write_reg(REG_MDMCFG0, 0x3b);

	cc2500_write_reg(REG_DEVIATN, 0x40);

	cc2500_write_reg(REG_MCSM2, 0x07);
	cc2500_write_reg(REG_MCSM1, 0x30);
	cc2500_write_reg(REG_MCSM0, 0x18);
	cc2500_write_reg(REG_FOCCFG, 0x16); //36
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

VMUCHAR cc2500_write_reg(VMUCHAR addr, VMUCHAR value)
{
	set_cs(LOW);

	while (get_miso() == HIGH) {
	};

	SPI_transfer_byte(addr);

	VMUCHAR y = SPI_transfer_byte(value);
	set_cs(HIGH);
	return y;
}

VMUCHAR cc2500_read_reg(VMUCHAR addr)
{
	addr = addr + 0x80;
	set_cs(LOW);
	while (get_miso() == HIGH) {
	};
	VMUCHAR x = SPI_transfer_byte(addr);

	VMUCHAR y = SPI_transfer_byte(0);
	set_cs(HIGH);
	return y;
}

VMUCHAR cc2500_read_burst_reg(VMUCHAR addr, VMUCHAR buffer[], VMUCHAR count)
{
	addr = addr | 0xC0;
	set_cs(LOW);
	while (get_miso() == HIGH) {};

	VMUCHAR x = SPI_transfer_byte(addr);

	int i;
	for (i = 0; i < count; i++)
	{
		buffer[i] = SPI_transfer_byte(0);
	}
	buffer[i+1] = '\0';
	set_cs(HIGH);
	return x;
}


// For status/strobe addresses, the BURST bit selects between status registers
// and command strobes.
VMUCHAR cc2500_read_status_reg(VMUCHAR addr)
{
	addr = addr | 0xC0;
	set_cs(LOW);
	while (get_miso() == HIGH) {
	};
	VMUCHAR x = SPI_transfer_byte(addr);

	VMUCHAR y = SPI_transfer_byte(0);
	set_cs(HIGH);
	return y;
}




VMUCHAR cc2500_send_strobe(VMUCHAR strobe)
{
	set_cs(LOW);

	while (get_miso() == HIGH) {
	};

	VMUCHAR result =  SPI_transfer_byte(strobe);
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
