#include <string.h>
#include "vmtype.h"
#include "vmboard.h"
#include "vmsystem.h"
#include "vmlog.h"
#include "vmcmd.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "vmthread.h"

#include "ResID.h"

#include "vmhttps.h"
#include "vmtimer.h"
#include "vmgsm_gprs.h"
#include "vmstdlib.h"

#include "config.h"


VMUINT8 g_channel_id;
VMINT g_read_seg_num;

VMUINT8 VMHTTPS_BUFFER[256];

void https_send_request_set_channel_rsp_cb(VMUINT32 req_id, VMUINT8 channel_id, VMUINT8 result)
{
    VMINT ret = -1;

    ret = vm_https_send_request(
        0,                  			/* Request ID */
        VM_HTTPS_METHOD_GET,            /* HTTP Method Constant */
        VM_HTTPS_OPTION_NO_CACHE,       /* HTTP request options */
        VM_HTTPS_DATA_TYPE_BUFFER,		/* Reply type (wps_data_type_enum) */
        256,                     	    /* bytes of data to be sent in reply at a time. If data is more that this, multiple response would be there */
        (VMUINT8 *)VMHTTPS_BUFFER,      /* The request URL */
        strlen(VMHTTPS_BUFFER),         /* The request URL length */
        NULL,                           /* The request header */
        0,                              /* The request header length */
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
        ret = vm_https_read_content(request_id, ++g_read_seg_num, 256);
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
            256);                                          /* The suggested segment data length of replied data in the peer buffer of
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
    strcpy(apn_info.apn, CM_CUST_APN);
    strcpy(apn_info.proxy_address, CM_PROXY_ADDRESS);
    apn_info.proxy_port = CM_PROXY_PORT;
    apn_info.using_proxy = CM_USING_PROXY;
    vm_gsm_gprs_set_customized_apn_info(&apn_info);
}

void https_send_request(void)
{
     /*----------------------------------------------------------------*/
     /* Local Variables                                                */
     /*----------------------------------------------------------------*/
     VMINT ret = -1;
     VMINT apn = VM_BEARER_DATA_ACCOUNT_TYPE_GPRS_CUSTOMIZED_APN;
     vm_https_callbacks_t callbacks = {
         (vm_https_set_channel_response_callback)https_send_request_set_channel_rsp_cb,
         (vm_https_unset_channel_response_callback)https_unset_channel_rsp_cb,
         (vm_https_release_all_request_response_callback)https_send_release_all_req_rsp_cb,
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
