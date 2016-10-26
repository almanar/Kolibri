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
#include "vmgsm_sms.h"
#include "vmgsm_sim.h"
#include "vmgsm_cell.h"

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

/* Message ID for the kolibri main thread communication. */
#define KOLIBRI_MSG_INIT 		 (VMUINT16)1
#define KOLIBRI_MSG_WAIT  		 (VMUINT16)2
#define KOLIBRI_MSG_NEW_DATA    (VMUINT16)3
#define KOLIBRI_MSG_RESET     (VMUINT16)4
#define KOLIBRI_MSG_START        (VMUINT16)5

/* Thread handles */
VM_THREAD_HANDLE kolibri_init_thread = 0;
VM_THREAD_HANDLE kolibri_main_thread = 0;
VM_THREAD_HANDLE kolibri_gsm_thread = 0;

/* Signal, mutex and shared variable for thread synchronization */
VMUINT 		 g_variable = 0x5555; /* The global variable for sharing among multi-threads. */
VM_SIGNAL_ID g_signal;
vm_mutex_t   g_mutex;

//void kolibri_handle_sysevent(VMINT message, VMINT param);
//VMUINT32 kolibri_main(VM_THREAD_HANDLE thread_handle, void* user_data);
//void kolibri_init(void);
//void kolibri_proxy_callback(void);
//-----------------------------------------------------------------------------


/*----------------------------------------------HTTP---------------------------------------------*/

VMUINT8 URL_BUFFER[256];
VMUINT8 g_channel_id;
VMINT g_read_seg_num;

static void https_send_request_set_channel_rsp_cb(VMUINT32 req_id, VMUINT8 channel_id, VMUINT8 result)
{
    VMINT ret = -1;

    vm_log_info("HTTP REQ: %s", URL_BUFFER);

    ret = vm_https_send_request(
        0,                  				/* Request ID */
        VM_HTTPS_METHOD_GET,                /* HTTP Method Constant */
        VM_HTTPS_OPTION_NO_CACHE,           /* HTTP request options */
        VM_HTTPS_DATA_TYPE_BUFFER,          /* Reply type (wps_data_type_enum) */
        100,                     	        /* bytes of data to be sent in reply at a time. If data is more that this, multiple response would be there */
        (VMSTR)URL_BUFFER,        			/* The request URL */
        strlen(URL_BUFFER),           		/* The request URL length */
        (VMSTR)CONF_MY_USER_AGENT,            /* The request header */
        strlen(CONF_MY_USER_AGENT),           /* The request header length */
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
    vm_log_info("reply_content:%s", reply_segment);
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
    strcpy(apn_info.apn, 			CONF_CUST_APN);
    strcpy(apn_info.proxy_address, 	CONF_PROXY_ADDRESS);
    apn_info.proxy_port = 			CONF_PROXY_PORT;
    apn_info.using_proxy = 			CONF_USING_PROXY;
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



/* The callback of sending SMS, for checking if an SMS is sent successfully. */
void sms_send_callback(vm_gsm_sms_callback_t* callback_data){
    if(callback_data->action == VM_GSM_SMS_ACTION_SEND){
        vm_log_info("send sms callback, result = %d", callback_data->result);
    }
}


/* Sends SMS */
void sms_send_sms_test(void){
    VMWCHAR content[100];
    VMWCHAR num[100];
    VMINT res = 0;
    VMINT number_count = 0;
    VMBOOL result;
    number_count = vm_gsm_sim_get_card_count();
    vm_log_info("sms read card count %d", number_count);
    result = vm_gsm_sim_has_card();
    vm_log_info("sms read sim card result %d", result);
    /* Sets SMS content */
    vm_chset_ascii_to_ucs2(content, sizeof(content), URL_BUFFER);
    /* Sets recipient's mobile number */
    vm_chset_ascii_to_ucs2(num, sizeof(num), CONF_MASTER_PHONENUMBER);
    res = vm_gsm_sms_send(num, content, sms_send_callback, NULL);
    if(res != 0){
        vm_log_info("sms send fail!");
    }
    vm_log_info("sms send: %d", res);
}


void kolibri_init(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	static vm_thread_message_t export_message={0}; /* The message structure */
	vm_thread_sleep(10000);
	vm_log_info("kolibri_init");

	// Set date and time
	vm_date_time_t sys_date_time;
	sys_date_time.year = 2016;
	sys_date_time.month = 1;
	sys_date_time.day = 1;
	sys_date_time.hour = 0;
	sys_date_time.minute = 0;
	sys_date_time.second = 0;
	vm_time_set_date_time(&sys_date_time);

	// Send message to start kolibri_main thread
	export_message.message_id = 2000; // SEND HTTP
	export_message.user_data = (char *)"";
	vm_thread_send_message(vm_thread_get_main_handle(), &export_message);

	DPRINT("BATTERY LEVEL: %d", vm_pwr_get_battery_level());
	vm_log_info("init end");

}

VMUINT32 kolibri_main(VM_THREAD_HANDLE thread_handle, void* user_data)
{
	static vm_thread_message_t export_message={0}; /* The message structure */
	static vm_thread_message_t main_message={0}; /* The message structure */
	static VMUINT8 parram[256] = {0};
	static VMUINT32 dtime= 0;
	static VMINT8 url_len=0;
	static VMINT8 missed=0;

	vm_log_info("kolibri main");
	vm_log_info("kolibri main - main_message: %d", main_message.message_id);

	int next=1;
	while(1){
		//vm_thread_get_message(&main_message);
		switch(main_message.message_id)
		{
		case KOLIBRI_MSG_INIT:
			vm_log_info("kolibri main - init");
			DPRINT("TEST %d", 1);
			dexg4_set_src();
			dexg4_init();
			//---
			next = KOLIBRI_MSG_START;
			break;

		case KOLIBRI_MSG_START:
			vm_log_info("kolibri main - start");
			dexg4_wake();
			if (dexg4_receive()){
				missed = 0;
				next = KOLIBRI_MSG_NEW_DATA;
			}else{
				// packet missed
				memset(&URL_BUFFER, 0, sizeof(URL_BUFFER));
				sms_send_sms_test();
				if (++missed>2){next = KOLIBRI_MSG_INIT;}
				else{next = KOLIBRI_MSG_WAIT;}
			}
			//---
			break;

		case KOLIBRI_MSG_NEW_DATA:
			vm_log_info("kolibri main - new data");
			memset(&URL_BUFFER, 0, sizeof(URL_BUFFER));
			memset(&parram, 0, sizeof(parram));
			url_len = sprintf(parram, "?rr=%lu&zi=%lu&pc=%s&lv=%lu&lf=%lu&db=%hhu&ts=%lu&ti=%lu&bp=%d",
					milisec(),
					dexg4_packet.src_addr,
					CONF_MY_CONTROL_NUMBER,
					dexg4_packet.raw_data,
					dexg4_packet.filtered_data,
					dexg4_packet.battery,
					(milisec() - (RX_POLL_TIME - 300000)),
					dexg4_packet.transaction_id,
					70);//(VMINT)vm_pwr_get_battery_level());
			vm_log_info("PARRAM: %d: %s", url_len, parram);

			url_len = sprintf(URL_BUFFER, "%s%s",
					CONF_MY_WEBSERVICE_URL, parram);

			/* Sends MESSAGE to the main thread. */
			export_message.message_id = 2001; // SEND HTTP
			export_message.user_data = &URL_BUFFER;
			vm_thread_sleep(50);
			vm_thread_send_message(vm_thread_get_main_handle(), &export_message);

			//---
			next=KOLIBRI_MSG_WAIT;
			break;

		case KOLIBRI_MSG_WAIT:
			vm_log_info("kolibri main - sleep");

			dexg4_sleep();

			dtime = RX_POLL_TIME - milisec();
			vm_log_info("kolibri main - sleep - poll %d", dtime);
			// freeze this thread for x ms
			vm_signal_timed_wait(g_signal, (VMUINT32)(dtime));
			//---
			next = KOLIBRI_MSG_START;
			break;

		case KOLIBRI_MSG_RESET:
			//---
			RX_POLL_TIME = 0;
			next = KOLIBRI_MSG_INIT;
			break;

		default:
			vm_log_info("kolibri main - default: MSG %d", main_message.message_id);
		}
		vm_thread_sleep(50);
		main_message.message_id = next;
		main_message.user_data = &g_variable;
		next = 0; // 0 Reserved for default
	}

}

void kolibri_handle_sysevent(VMINT message, VMINT param)
{
	vm_thread_message_t sys_message; /* The message structure */

	switch (message)
	{
	case VM_EVENT_CREATE:
		vm_log_info("message for init functionality");
		kolibri_init_thread = vm_thread_create((vm_thread_callback)kolibri_init,
				NULL,
				130);
		break;

	case 2000:  /* Receives the message from the sub-thread. */
		vm_log_info("message for main functionality");
		/* Creates a sub-thread with the priority of 126 */
		g_signal = vm_signal_create();
		kolibri_main_thread = vm_thread_create((vm_thread_callback)kolibri_main,
				NULL, 131);
		if(kolibri_main_thread == 0)
		{
			vm_log_info("create thread failed");
			return ;
		}
		else
		{
			vm_log_info("create thread successful");
			sys_message.message_id = KOLIBRI_MSG_INIT;
			sys_message.user_data = &g_variable;
			vm_thread_send_message(kolibri_main_thread, &sys_message);
		}
		break;

	case 2001:  /* Receives the message from the sub-thread. */
		vm_log_info("message for gsm functionality");
        vm_timer_create_non_precise(1000, https_send_request, (void *)param);
		break;

	case VM_EVENT_QUIT:
		vm_log_info("kolibri - End.");
		break;

	}
}

/* Entry point */
void vm_main(void)
{
	/* Registers system event handler */
	vm_pmng_register_system_event_callback(kolibri_handle_sysevent);
}
