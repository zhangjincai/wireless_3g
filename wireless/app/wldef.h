#ifndef __WLDEF_H__
#define __WLDEF_H__


/*
 * PPPd������Ϣ
 */
struct wl_pppd_info
{
	unsigned int using_channel;
	char using_interface[16];
	char local_ip_address[32];
	char remote_ip_address[32];
	char primary_dns_address[32];
	char secondary_dns_address[32];
}__attribute__((packed));
typedef struct wl_pppd_info wl_pppd_info_t;

/*
  * ��ѯģ������汾 
  */
struct wl_cgmr
{
	char softversion[32];
}__attribute__((packed));
typedef struct wl_cgmr wl_cgmr_t;

/*
 * ������Ϣ��ѯ 
 */
struct wl_cgmi
{
	char manufacturer[32];
}__attribute__((packed));
typedef struct wl_cgmi wl_cgmi_t;

/*
 * ��Ʒ���Ʋ�ѯ���� 
 */
 struct wl_cgmm
{
	char production_name[32];
}__attribute__((packed));
typedef struct wl_cgmm wl_cgmm_t;

/*
 * ��ѯӲ���汾
 */
struct wl_hwver
{
	char firmversion[32];
}__attribute__((packed));
typedef struct wl_hwver wl_hwver_t;

/*
 * �ź�ǿ�Ȳ�ѯ
 */
struct wl_csq
{
	unsigned char rssi;   //�ź�ǿ��
	unsigned char fer;    //�ŵ���֡��
}__attribute__((packed));
typedef struct wl_csq wl_csq_t;

/*
 * ϵͳ����Ϣ��ѯ 
 */
struct wl_sysinfo
{
	unsigned char srv_status;  	//ϵͳ����״̬
	unsigned char srv_domain;	//ϵͳ������
	unsigned char roam_status; //����״̬
	unsigned char sys_mode;    //ϵͳģʽ
	unsigned char sim_state;   //UIM��״̬
}__attribute__((packed));
typedef struct wl_sysinfo wl_sysinfo_t;

/*
 * ��ѯ��ǰ�Ƿ��������� 
 */
struct wl_zps
{
	unsigned char state;
}__attribute__((packed));
typedef struct wl_zps wl_zps_t;


struct wl_csq_sysinfo
{
	unsigned char rssi;   //�ź�ǿ��
	unsigned char fer;    //�ŵ���֡��
	unsigned char srv_status;  	//ϵͳ����״̬
	unsigned char srv_domain;	//ϵͳ������
	unsigned char roam_status; //����״̬
	unsigned char sys_mode;    //ϵͳģʽ
	unsigned char sim_state;   //UIM��״̬
}__attribute__((packed));
typedef struct wl_csq_sysinfo wl_csq_sysinfo_t;

struct wl_flow_info
{
	long long rx_bytes;
	long long tx_bytes;
}__attribute__((packed));
typedef struct wl_flow_info wl_flow_info_t;

struct wl_dial_time
{
	long last_discon_time;
	long last_dial_time;
}__attribute__((packed));
typedef struct wl_dial_time wl_dial_time_t;



#endif



