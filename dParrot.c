/*
  This example code is in public domain.

  This example code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
  This example shows how to read a digital pin.

  In this example, it will read the status from the pin READ_PIN, and write the same
  status to the pin WRITE_PIN.
 */

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
#include "cc2500_REG.h"
#include "dexter.h"

void dparrot_handle_sysevent(VMINT message, VMINT param);
VMINT32 dparrot_main_thread(VM_THREAD_HANDLE thread_handle, void* user_data);
void dparrot_handle_sysevent(VMINT message, VMINT param);
//-----------------------------------------------------------------------------

extern VMUCHAR RX_BUFFER[4][24];
extern VMUINT RX_POLL_TIME;
extern VMUINT RX_uS_TIMEOUT[4];


VMINT32 dparrot_main_thread(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	vm_log_info("dparrot_main_thread - START.");

	vm_date_time_t sys_date_time;
	sys_date_time.year = 2016;
	sys_date_time.month = 1;
	sys_date_time.day = 1;
	sys_date_time.hour = 0;
	sys_date_time.minute = 0;
	sys_date_time.second = 0;
	vm_time_set_date_time(&sys_date_time);

	vm_thread_sleep(5000);
	spi_open();
	cc2500_init();
	cc2500_read_config_regs();
	vm_thread_sleep(5000);

	vm_log_info("0 TIMEOUT %d",RX_uS_TIMEOUT[0]);
	vm_log_info("0 PACKETSIZE %d",RX_BUFFER[0][0]);
	vm_log_info("%02x - %02x - %02x - %02x",
			RX_BUFFER[0][5],
			RX_BUFFER[0][6],
			RX_BUFFER[0][7],
			RX_BUFFER[0][8]);

	vm_log_info("1 TIMEOUT %d",RX_uS_TIMEOUT[1]);
	vm_log_info("1 PACKETSIZE %d",RX_BUFFER[1][0]);
	vm_log_info("%02x - %02x - %02x - %02x",
			RX_BUFFER[1][5],
			RX_BUFFER[1][6],
			RX_BUFFER[1][7],
			RX_BUFFER[1][8]);

	vm_log_info("2 TIMEOUT %d",RX_uS_TIMEOUT[2]);
	vm_log_info("3 PACKETSIZE %d",RX_BUFFER[2][0]);
	vm_log_info("%02x - %02x - %02x - %02x",
			RX_BUFFER[2][5],
			RX_BUFFER[2][6],
			RX_BUFFER[2][7],
			RX_BUFFER[2][8]);

	vm_log_info("3 TIMEOUT %d",RX_uS_TIMEOUT[3]);
	vm_log_info("3 PACKETSIZE %d",RX_BUFFER[3][0]);
	vm_log_info("%02x - %02x - %02x - %02x",
			RX_BUFFER[3][5],
			RX_BUFFER[3][6],
			RX_BUFFER[3][7],
			RX_BUFFER[3][8]);


	return 0;
}

void dparrot_handle_sysevent(VMINT message, VMINT param)
{
	switch (message)
	{
	case VM_EVENT_CREATE:
		vm_log_info("dParrot - Start.");
		/* Creates a sub-thread with the priority of 1 */
		vm_thread_create(dparrot_main_thread, NULL, 1);

		break;

	case VM_EVENT_QUIT:
		vm_log_info("dParrot - End.");
		break;
	}
}

/* Entry point */
void vm_main(void)
{
	/* Registers system event handler */
	vm_pmng_register_system_event_callback(dparrot_handle_sysevent);
}
