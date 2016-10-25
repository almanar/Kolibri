#pragma pack(1)
typedef struct _dexcom_g4_packet
{
	//0      pkt_len;
	VMUINT8  pkt_len;
	//1:4	 dest_addr;
	VMUINT32 dest_addr;
	//5:8    src_addr;
	VMUINT32 src_addr;
	//9      port;
	VMUINT8  port;
	//10     device_info;
	VMUINT8  device_info;
	//11     transaction_id;
	VMUINT8  transaction_id;
	//12:13  raw_data;
	VMUINT16 raw_data;
	//14:15  filtered_data;
	VMUINT16 filtered_data;
	//16     battery;
	VMUINT8  battery;
	//17     fcs;
	VMUINT8  fcs;
	//18     crc;
	VMUINT8  crc;
	//19     rssi;
	VMUINT8  rssi;
	//20     lqi;
	VMUINT8  lqi;
} dexcom_g4_packet;

