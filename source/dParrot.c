#include "vmtype.h"
#include "vmboard.h"
#include "vmsystem.h"
#include "vmthread.h"
#include "vmlog.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmdatetime.h"
#include "vmfs.h"
#include "vmtimer.h"
#include "vmpwr.h"
#include "vmhttps.h"
#include "vmgsm_gprs.h"

#include "ResID.h"

#include "cc2500.h"
#include "dexcomg4.h"
#include "config.h"
#include "extern.h"

#include <stdio.h>
#include <string.h>

/* Externs */
extern VMUCHAR RX_BUFFER[4][24];
extern VMUINT32 RX_POLL_TIME;
extern VMUINT RX_uS_TIMEOUT[4];
extern dexcom_g4_packet dexg4_packet;

/* Message ID for the thread communication. */
#define DPARROT_MSG_INIT 		 (VMUINT16)1
#define DPARROT_MSG_WAIT  		 (VMUINT16)2
#define DPARROT_MSG_NEW_DATA    (VMUINT16)3
#define DPARROT_MSG_RESET     (VMUINT16)4
#define DPARROT_MSG_START        (VMUINT16)5

/* Thread handles */
VM_THREAD_HANDLE dparrot_init_thread = 0;
VM_THREAD_HANDLE dparrot_main_thread = 0;
VM_THREAD_HANDLE dparrot_gsm_thread = 0;

/* Signal, mutex and shared variable for thread synchronization */
VMUINT 		 g_variable = 0x5555; /* The global variable for sharing among multi-threads. */
VM_SIGNAL_ID g_signal;
vm_mutex_t   g_mutex;

//void dparrot_handle_sysevent(VMINT message, VMINT param);
//VMUINT32 dparrot_main(VM_THREAD_HANDLE thread_handle, void* user_data);
//void dparrot_init(void);
//void dparrot_proxy_callback(void);
//-----------------------------------------------------------------------------


VMUINT8 URL_BUFFER[256];

/*----------------------------------------------HTTP---------------------------------------------*/

VMUINT8 g_channel_id;
VMINT g_read_seg_num;

static void https_send_request_set_channel_rsp_cb(VMUINT32 req_id, VMUINT8 channel_id, VMUINT8 result)
{
    VMINT ret = -1;

    ret = vm_https_send_request(
        0,                  				/* Request ID */
        VM_HTTPS_METHOD_GET,                /* HTTP Method Constant */
        VM_HTTPS_OPTION_NO_CACHE,           /* HTTP request options */
        VM_HTTPS_DATA_TYPE_BUFFER,          /* Reply type (wps_data_type_enum) */
        100,                     	        /* bytes of data to be sent in reply at a time. If data is more that this, multiple response would be there */
        (VMSTR)URL_BUFFER,        			/* The request URL */
        strlen(URL_BUFFER),           		/* The request URL length */
        CM_MY_USER_AGENT,                   /* The request header */
        strlen(CM_MY_USER_AGENT),           /* The request header length */
        NULL,
        0);

    if (ret != 0) {
        vm_https_unset_channel(channel_id);
    }
}

static void https_unset_channel_rsp_cb(VMUINT8 channel_id, VMUINT8 result)
{
    vm_log_debug("https_unset_channel_rsp_cb()");
}
static void https_send_release_all_req_rsp_cb(VMUINT8 result)
{
    vm_log_debug("https_send_release_all_req_rsp_cb()");
}
static void https_send_termination_ind_cb(void)
{
    vm_log_debug("https_send_termination_ind_cb()");
}
static void https_send_read_request_rsp_cb(VMUINT16 request_id, VMUINT8 result,
                                         VMUINT16 status, VMINT32 cause, VMUINT8 protocol,
                                         VMUINT32 content_length,VMBOOL more,
                                         VMUINT8 *content_type, VMUINT8 content_type_len,
                                          VMUINT8 *new_url, VMUINT32 new_url_len,
                                         VMUINT8 *reply_header, VMUINT32 reply_header_len,
                                         VMUINT8 *reply_segment, VMUINT32 reply_segment_len)
{
    VMINT ret = -1;
    vm_log_debug("https_send_request_rsp_cb()");
    if (result != 0) {
        vm_https_cancel(request_id);
        vm_https_unset_channel(g_channel_id);
    }
    else {
        vm_log_debug("reply_content:%s", reply_segment);
        ret = vm_https_read_content(request_id, ++g_read_seg_num, 100);
        if (ret != 0) {
            vm_https_cancel(request_id);
            vm_https_unset_channel(g_channel_id);
        }
    }
}
static void https_send_read_read_content_rsp_cb(VMUINT16 request_id, VMUINT8 seq_num,
                                                 VMUINT8 result, VMBOOL more,
                                                 VMUINT8 *reply_segment, VMUINT32 reply_segment_len)
{
    VMINT ret = -1;
    vm_log_debug("reply_content:%s", reply_segment);
    if (more > 0) {
        ret = vm_https_read_content(
            request_id,                                    /* Request ID */
            ++g_read_seg_num,                 /* Sequence number (for debug purpose) */
            100);                                          /* The suggested segment data length of replied data in the peer buffer of
                                                              response. 0 means use reply_segment_len in MSG_ID_WPS_HTTP_REQ or
                                                              read_segment_length in previous request. */
        if (ret != 0) {
            vm_https_cancel(request_id);
            vm_https_unset_channel(g_channel_id);
        }
    }
    else {
        /* don't want to send more requests, so unset channel */
        vm_https_cancel(request_id);
        vm_https_unset_channel(g_channel_id);
        g_channel_id = 0;
        g_read_seg_num = 0;

    }
}
static void https_send_cancel_rsp_cb(VMUINT16 request_id, VMUINT8 result)
{
    vm_log_debug("https_send_cancel_rsp_cb()");
}
static void https_send_status_query_rsp_cb(VMUINT8 status)
{
    vm_log_debug("https_send_status_query_rsp_cb()");
}

void set_custom_apn(void)
{
    vm_gsm_gprs_apn_info_t apn_info;

    memset(&apn_info, 0, sizeof(apn_info));
    strcpy(apn_info.apn, 			CM_CUST_APN);
    strcpy(apn_info.proxy_address, 	CM_PROXY_ADDRESS);
    apn_info.proxy_port = 			CM_PROXY_PORT;
    apn_info.using_proxy = 			CM_USING_PROXY;
    vm_gsm_gprs_set_customized_apn_info(&apn_info);
}

static void https_send_request(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
     /*----------------------------------------------------------------*/
     /* Local Variables                                                */
     /*----------------------------------------------------------------*/
     VMINT ret = -1;
     VMINT apn = VM_BEARER_DATA_ACCOUNT_TYPE_GPRS_CUSTOMIZED_APN;
     vm_https_callbacks_t callbacks = {
         https_send_request_set_channel_rsp_cb,
         https_unset_channel_rsp_cb,
         https_send_release_all_req_rsp_cb,
         https_send_termination_ind_cb,
         https_send_read_request_rsp_cb,
         https_send_read_read_content_rsp_cb,
         https_send_cancel_rsp_cb,
         https_send_status_query_rsp_cb
     };
    /*----------------------------------------------------------------*/
     /* Code Body                                                      */
     /*----------------------------------------------------------------*/

    do {
        set_custom_apn();

        vm_timer_delete_non_precise(timer_id);
        ret = vm_https_register_context_and_callback(
            apn, &callbacks);

        if (ret != 0) {
            break;
        }

        /* set network profile information */
        ret = vm_https_set_channel(
            0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0,
            0, 0
        );
    } while (0);

}
/*----------------------------------------------HTTP---------------------------------------------*/


void dparrot_init(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	static vm_thread_message_t export_message={0}; /* The message structure */
	vm_thread_sleep(10000);
	vm_log_info("dparrot_init");

	// Set date and time
	vm_date_time_t sys_date_time;
	sys_date_time.year = 2016;
	sys_date_time.month = 1;
	sys_date_time.day = 1;
	sys_date_time.hour = 0;
	sys_date_time.minute = 0;
	sys_date_time.second = 0;
	vm_time_set_date_time(&sys_date_time);

	// Send message to start dparrot_main thread
	export_message.message_id = 2000; // SEND HTTP
	export_message.user_data = (char *)"";
	vm_thread_send_message(vm_thread_get_main_handle(), &export_message);

	vm_log_info("init end");

}

VMUINT32 dparrot_main(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	static vm_thread_message_t export_message={0}; /* The message structure */
	static vm_thread_message_t main_message={0}; /* The message structure */
	static VMUINT8 parram[256] = {0};
	static VMUINT32 dtime= 0;
	static VMINT8 url_len=0;
	static VMINT8 missed=0;

	vm_log_info("dparrot main");
	vm_log_info("dparrot main - main_message: %d", main_message.message_id);

	int next=1;
	while(1){
		//vm_thread_get_message(&main_message);
		switch(main_message.message_id)
		{
		case DPARROT_MSG_INIT:
			vm_log_info("dparrot main - init");
			//dparrot_init();
			dexg4_init();
			//---
			next = DPARROT_MSG_START;
			break;

		case DPARROT_MSG_START:
			vm_log_info("dparrot main - start");
			dexg4_wake();
			if (dexg4_receive()){
				missed = 0;
				next = DPARROT_MSG_NEW_DATA;
			}else{
				// packet missed
				if (++missed>2){next = DPARROT_MSG_INIT;}
				else{next = DPARROT_MSG_WAIT;}
			}
			//---
			break;

		case DPARROT_MSG_NEW_DATA:
			vm_log_info("dparrot main - new data");
			memset(&parram, 0, sizeof(parram));
			url_len = sprintf(parram, "?rr=%lu&zi=%lu&pc=%s&lv=%lu&lf=%lu&db=%hhu&ts=%lu&bp=%d",
					milisec(),
					dexg4_packet.src_addr,
					CM_MY_CONTROL_NUMBER,
					dexg4_packet.raw_data,
					dexg4_packet.filtered_data,
					dexg4_packet.battery,
					milisec(),
					(VMINT)vm_pwr_get_battery_level());
			vm_log_info("PARRAM: %d: %s", url_len, parram);

			url_len = sprintf(URL_BUFFER, "%s%s",
					CM_MY_WEBSERVICE_URL, parram);

			/* Sends MESSAGE to the main thread. */
			export_message.message_id = 2001; // SEND HTTP
			export_message.user_data = &URL_BUFFER;
			vm_thread_sleep(10);
			vm_thread_send_message(vm_thread_get_main_handle(), &export_message);


			//---
			next=DPARROT_MSG_WAIT;
			break;

		case DPARROT_MSG_WAIT:
			vm_log_info("dparrot main - sleep");

			dexg4_sleep();

			dtime = RX_POLL_TIME - milisec();
			vm_log_info("dparrot main - sleep - poll %d", dtime);
			// freeze this thread for x ms
			vm_signal_timed_wait(g_signal, (VMUINT32)(dtime));
			//---
			next = DPARROT_MSG_START;
			break;

		case DPARROT_MSG_RESET:
			//---
			next = 0;
			break;

		default:
			vm_log_info("dparrot main - default: MSG %d", main_message.message_id);
		}
		vm_thread_sleep(50);
		main_message.message_id = next;
		main_message.user_data = &g_variable;
		next = 0; // 0 Reserved for default
	}

}

void dparrot_handle_sysevent(VMINT message, VMINT param)
{
	vm_thread_message_t sys_message; /* The message structure */

	switch (message)
	{
	case VM_EVENT_CREATE:
		vm_log_info("message for init functionality");
		dparrot_init_thread = vm_thread_create((vm_thread_callback)dparrot_init,
				NULL,
				130);
		break;

	case 2000:  /* Receives the message from the sub-thread. */
		vm_log_info("message for main functionality");
		/* Creates a sub-thread with the priority of 126 */
		g_signal = vm_signal_create();
		dparrot_main_thread = vm_thread_create((vm_thread_callback)dparrot_main,
				NULL, 131);
		if(dparrot_main_thread == 0)
		{
			vm_log_info("create thread failed");
			return ;
		}
		else
		{
			vm_log_info("create thread successful");
			sys_message.message_id = DPARROT_MSG_INIT;
			sys_message.user_data = &g_variable;
			vm_thread_send_message(dparrot_main_thread, &sys_message);
		}
		break;

	case 2001:  /* Receives the message from the sub-thread. */
		vm_log_info("message for gsm functionality");
        vm_timer_create_non_precise(1000, https_send_request, (void *)param);
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
