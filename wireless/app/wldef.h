#ifndef __WLDEF_H__
#define __WLDEF_H__


/*
 * PPPd拨号信息
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
  * 查询模块软件版本 
  */
struct wl_cgmr
{
	char softversion[32];
}__attribute__((packed));
typedef struct wl_cgmr wl_cgmr_t;

/*
 * 厂商信息查询 
 */
struct wl_cgmi
{
	char manufacturer[32];
}__attribute__((packed));
typedef struct wl_cgmi wl_cgmi_t;

/*
 * 产品名称查询命令 
 */
 struct wl_cgmm
{
	char production_name[32];
}__attribute__((packed));
typedef struct wl_cgmm wl_cgmm_t;

/*
 * 查询硬件版本
 */
struct wl_hwver
{
	char firmversion[32];
}__attribute__((packed));
typedef struct wl_hwver wl_hwver_t;

/*
 * 信号强度查询
 */
struct wl_csq
{
	unsigned char rssi;   //信号强度
	unsigned char fer;    //信道误帧率
}__attribute__((packed));
typedef struct wl_csq wl_csq_t;

/*
 * 系统的信息查询 
 */
struct wl_sysinfo
{
	unsigned char srv_status;  	//系统服务状态
	unsigned char srv_domain;	//系统服务域
	unsigned char roam_status; //漫游状态
	unsigned char sys_mode;    //系统模式
	unsigned char sim_state;   //UIM卡状态
}__attribute__((packed));
typedef struct wl_sysinfo wl_sysinfo_t;

/*
 * 查询当前是否正在上网 
 */
struct wl_zps
{
	unsigned char state;
}__attribute__((packed));
typedef struct wl_zps wl_zps_t;


struct wl_csq_sysinfo
{
	unsigned char rssi;   //信号强度
	unsigned char fer;    //信道误帧率
	unsigned char srv_status;  	//系统服务状态
	unsigned char srv_domain;	//系统服务域
	unsigned char roam_status; //漫游状态
	unsigned char sys_mode;    //系统模式
	unsigned char sim_state;   //UIM卡状态
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



