#include "vmtype.h"
#include "vmboard.h"
#include "vmsystem.h"
#include "vmlog.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmthread.h"
#include "vmdatetime.h"
#include "vmfs.h"
#include "vmtimer.h"
#include "vmpwr.h"

#include "ResID.h"

#include "cc2500.h"
#include "dexcomg4.h"
#include "cellular.h"
#include "config.h"
#include "extern.h"

#include <stdio.h>
#include <string.h>

extern dexcom_g4_packet dexg4_packet;


/* Message ID for the thread communication. */
#define DPARROT_MSG_INIT 		 (VMUINT16)0
#define DPARROT_MSG_SLEEP  		 (VMUINT16)1
#define DPARROT_MSG_SEND_HTTP    (VMUINT16)2
#define DPARROT_MSG_SEND_BLE     (VMUINT16)3
#define DPARROT_MSG_START        (VMUINT16)4

VM_THREAD_HANDLE g_thread = 0; /* The sub-thread handle */

/* Signal and mutex for thread synchronization */
VM_SIGNAL_ID g_signal;
vm_mutex_t g_mutex;

VMUINT g_variable = 0x5555; /* The global variable for sharing among multi-threads. */
vm_thread_message_t g_message; /* The message structure */


void dparrot_handle_sysevent(VMINT message, VMINT param);
VMUINT32 dparrot_main(VM_THREAD_HANDLE thread_handle, void* user_data);
void dparrot_init(void);
void dparrot_proxy_callback(void);
//-----------------------------------------------------------------------------

extern VMUINT8 VMHTTPS_BUFFER[256];
extern VMUCHAR RX_BUFFER[4][24];
extern VMUINT RX_POLL_TIME;
extern VMUINT RX_uS_TIMEOUT[4];

/* a non precise timer */
VM_TIMER_ID_NON_PRECISE g_init_non_precise_id = 0;
VM_TIMER_ID_NON_PRECISE g_main_non_precise_id = 0;

VMUINT8 STR_BUFFER[256];

static unsigned short days[4][12] =
{
    {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
    { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
    { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
    {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};

void epoch_to_date_time(vm_date_time_t* date_time, VMUINT epoch)
{
    date_time->second = epoch%60; epoch /= 60;
    date_time->minute = epoch%60; epoch /= 60;
    date_time->hour   = epoch%24; epoch /= 24;

    VMUINT years = epoch/(365*4+1)*4; epoch %= 365*4+1;

    VMUINT year;
    for (year=3; year>0; year--)
    {
        if (epoch >= days[year][0])
            break;
    }

    VMUINT month;
    for (month=11; month>0; month--)
    {
        if (epoch >= days[year][month])
            break;
    }

    date_time->year  = years+year;
    date_time->month = month+1;
    date_time->day   = epoch-days[year][month]+1;
}

void dparrot_init(void)
{
	vm_log_info("dparrot_init");

	vm_date_time_t sys_date_time;
	sys_date_time.year = 2016;
	sys_date_time.month = 1;
	sys_date_time.day = 1;
	sys_date_time.hour = 0;
	sys_date_time.minute = 0;
	sys_date_time.second = 0;
	vm_time_set_date_time(&sys_date_time);

	vm_thread_sleep(1000);
	vm_log_info("START INIT");
	vm_log_info("pwr %d", vm_pwr_get_battery_level());
	vm_thread_sleep(1000);
}

void dparrot_proxy_callback(void){
	g_main_non_precise_id = vm_timer_create_non_precise(300000,
			(vm_timer_non_precise_callback)dparrot_main,
			NULL);
	vm_timer_delete_non_precise(g_init_non_precise_id);
//	dparrot_main();
}

VMUINT32 dparrot_main(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	vm_date_time_t get_time;
	vm_thread_sleep(10000);
	vm_log_info("dparrot main");
	vm_thread_message_t msg;
	vm_log_info("dparrot main - msg %d", msg.message_id);
	//vm_thread_get_message(&msg);

	int next;
	while(1){
		switch(msg.message_id)
		{
		case DPARROT_MSG_INIT:
			vm_log_info("dparrot main - init");
			dparrot_init();
			dexg4_init();
			next = DPARROT_MSG_START;
			break;

		case DPARROT_MSG_START:
			vm_log_info("dparrot main - start");
			dexg4_recieve();
			next = DPARROT_MSG_SEND_HTTP;
			break;

		case DPARROT_MSG_SLEEP:
//			vm_time_get_date_time(&get_time);
//			get_time.minute += 4;
//			vm_pwr_scheduled_startup(&get_time, 4);
//			vm_pwr_shutdown(0xFF);

			vm_thread_sleep(RX_POLL_TIME-milisec());
			next=DPARROT_MSG_START;

		case DPARROT_MSG_SEND_HTTP:
			sprintf(VMHTTPS_BUFFER, "%s?rr=%lu&zi=%lu&pc=%s&lv=%lu&lf=%lu&db=%hhu&ts=%lu&bp=%d",
					CM_MY_WEBSERVICE_URL,
					milisec(),
					dexg4_packet.src_addr,
					CM_MY_CONTROL_NUMBER,
					dexg4_packet.raw_data,
					dexg4_packet.filtered_data,
					dexg4_packet.battery,
					milisec(),
					vm_pwr_get_battery_level());
				vm_log_info("HTTP: %s", VMHTTPS_BUFFER);
				https_send_request(NULL,NULL,NULL);

				next=DPARROT_MSG_SLEEP;
			break;

		case DPARROT_MSG_SEND_BLE:
			break;

		default:

			vm_log_info("dparrot main - default: msg %d", msg.message_id);
			return 1;
		}
		msg.message_id = next;
		next = -1;
	}

}

void dparrot_handle_sysevent(VMINT message, VMINT param)
{
	switch (message)
	{
	case VM_EVENT_CREATE:
		/* Creates a sub-thread with the priority of 126 */
		g_thread = vm_thread_create((vm_thread_callback)dparrot_main,
									NULL, 129);

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
