#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "lib_general.h"
#include "lib_wireless.h"

#define WL_API_ERR

#ifdef WL_API_ERR
#define ERR(fmt, args...)	 do{ \
	fprintf(stderr, "ERROR: %s:%d, ", __FUNCTION__,__LINE__); \
	fprintf(stderr, fmt, ##args); \
}while(0)
#else
#define ERR(fmt, args...)
#endif

#define WL_PKG_HEAD_LEN		5
#define WL_PKG_DATA_LEN		320
#define WL_PKG_LEN				(WL_PKG_HEAD_LEN + WL_PKG_DATA_LEN)

struct wl_unix_package
{
	unsigned char id_h;		//关键字
	unsigned char id_l;
	unsigned char cmd;		//命令字
	unsigned char result;		//结果
	unsigned char length;		//长度
	unsigned char data[WL_PKG_DATA_LEN];
}__attribute__((packed));
typedef struct wl_unix_package wl_unix_package_t;

struct wl
{
	int sockfd;
}__attribute__((packed));


#define WL_ID_H			0x55
#define WL_ID_L			0xaa

#define WL_VER					"WL VERSION 1.0.4"
#define WL_UNIX_DOMAIN			"/tmp/lib_wl_unix.domain"


/* 协议返回结果 */
enum WL_RESULT
{
	WL_RESULT_OK = 1,
	WL_RESULT_ERR = 2,
	WL_RESULT_PKG_ERR = 3,
	WL_RESULT_NOT_CMD = 4
};

static int __wl_reconnect(lib_wl_t *wl);

static int __wl_unix_pkg(wl_unix_package_t *pkg, const unsigned int len)
{
	unsigned char tbuf[WL_PKG_LEN] = {0};
	unsigned char length = 0;
	int i, s_pos = 1;

	memcpy(&tbuf, pkg, len);

	for(i = 0; i < len; i++)
	{
		if((tbuf[i] == WL_ID_H) && (s_pos == 1))
		{
			s_pos = 2;
			continue;
		}

		if((tbuf[i] == WL_ID_L) && (s_pos == 2) && (tbuf[i - 1] == WL_ID_H))
		{
			length = tbuf[i + 3];  	//长度字
			memset(pkg, 0, sizeof(wl_unix_package_t));
			memcpy(pkg, &tbuf[i -1], length + WL_PKG_HEAD_LEN);

			return LIB_WL_EOK;
		}
	}

	return LIB_WL_ERROR;
}

static int __is_wl_unix_package(wl_unix_package_t *pkg, const unsigned char cmd)
{
	if((pkg->id_h == WL_ID_H) && (pkg->id_l == WL_ID_L) && (pkg->cmd == cmd))
		return LIB_WL_TRUE;
	
	return LIB_WL_FALSE;
}

static  void __wl_unix_package_create(wl_unix_package_t *pkg, const unsigned char cmd)
{
	memset(pkg, 0, sizeof(wl_unix_package_t));

	pkg->id_h = WL_ID_H;
	pkg->id_l = WL_ID_L;
	pkg->cmd = cmd;
}

static int __wl_reconnect(lib_wl_t *wl)
{
	if(wl == NULL)
		return LIB_WL_ERROR;

	if(wl->sockfd > 0)
		lib_unix_close(wl->sockfd);
		
	wl->sockfd = lib_unix_connect(WL_UNIX_DOMAIN);
	if(wl->sockfd == LIB_GE_ERROR)
			return LIB_WL_ERROR;
	
	lib_setfd_noblock(wl->sockfd);
	
	return LIB_WL_EOK;
}

char *lib_wl_version(void)
{
	return WL_VER;
}

lib_wl_t *lib_wl_new(void)
{
	lib_wl_t *wl = (lib_wl_t *)malloc(sizeof(lib_wl_t));
	if(wl == NULL)
		return LIB_WL_NULL;

	memset(wl, 0, sizeof(lib_wl_t));
	wl->sockfd = lib_unix_connect(WL_UNIX_DOMAIN);
	if(wl->sockfd == LIB_GE_ERROR)
		return LIB_WL_NULL;

	lib_setfd_noblock(wl->sockfd);
	
	return wl;
}

void lib_wl_destroy(lib_wl_t *wl)
{
	if(wl == NULL)
		return;
		
	if(wl != NULL)
	{
		if(wl->sockfd > 0)
			lib_unix_close(wl->sockfd);

		free(wl);
		wl = NULL;
	}
}

int lib_wl_reconnect(lib_wl_t *wl)
{
	if(wl == NULL)
		return LIB_WL_ERROR;
		
	return __wl_reconnect(wl);
}

/*
 * 获取网络状态
 */
enum WL_DIAL_STAT lib_wl_dial_stat_get(lib_wl_t *wl, const unsigned int msec)
{
	if(wl == NULL)
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;
	enum WL_DIAL_STAT dial_stat = WL_DIAL_STAT_UNDIAL;

	unsigned int __dial_stat = 0;
	
	int pkg_len = sizeof(struct wl_unix_package);

	memset(&pkg, 0, pkg_len);
	__wl_unix_package_create(&pkg, LIB_WL_CMD_DIAL_STAT_GET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);  //只发协议头部
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return WL_DIAL_STAT_UNDIAL;

		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_DIAL_STAT_GET) != LIB_WL_TRUE)
			return WL_DIAL_STAT_UNDIAL;

		if(pkg.result == WL_RESULT_OK)
		{
			memcpy(&__dial_stat, pkg.data, sizeof(unsigned int));
			
			dial_stat = __dial_stat;
			return dial_stat;
		}
		else
			return WL_DIAL_STAT_UNDIAL;
	}
	
	return WL_DIAL_STAT_UNDIAL;
}

/*
 * 获取拨号pppd状态
 */
int lib_wl_pppd_is_online(lib_wl_t *wl, const unsigned int msec)
{
	if(wl == NULL)
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR; 
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_PPPD_ONLINE);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
			
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_PPPD_ONLINE) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		if(pkg.result == WL_RESULT_OK)  //返回真
			return LIB_WL_TRUE;
		else
			return LIB_WL_FALSE;
	}
	
	return ret;	
}

/*
 * 获取sysinfo信息
 */
int lib_wl_sysinfo_get(lib_wl_t *wl, lib_wl_sysinfo_t *info, const unsigned int msec)
{
	if((wl == NULL) || (info == NULL))
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	
	memset(&pkg, 0, pkg_len);
	__wl_unix_package_create(&pkg, LIB_WL_CMD_SYSINFO_GET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
				
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_SYSINFO_GET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(info, pkg.data,  sizeof(lib_wl_sysinfo_t));

		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;	
}

/*
 * 获取sysinfo和csq信息
 */
int lib_wl_csq_sysinfo_get(lib_wl_t *wl, lib_wl_csq_sysinfo_t *info, const unsigned int msec)
{
	if((wl == NULL) || (info == NULL))
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_CSQ_SYSINFO);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
				
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_CSQ_SYSINFO) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(info, pkg.data, sizeof(lib_wl_csq_sysinfo_t));

		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;	
}

/*
 * 获取cgmr信息
 */
int lib_wl_cgmr_get(lib_wl_t *wl, lib_wl_cgmr_t *cgmr, const unsigned int msec)
{
	if((wl == NULL) || (cgmr == NULL))
		return LIB_WL_ERROR;
		
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_CGMR_GET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
				
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_CGMR_GET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(cgmr, pkg.data, sizeof(lib_wl_cgmr_t));

		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;	
}

/*
 * 获取hwver信息
 */
int lib_wl_hwver_get(lib_wl_t *wl, lib_wl_hwver_t *hwver, const unsigned int msec)
{
	if((wl == NULL) || (hwver == NULL))
		return LIB_WL_ERROR;
		
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_HWVER_GET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
				
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_HWVER_GET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(hwver, pkg.data, sizeof(lib_wl_hwver_t));

		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;	
}

/*
 * 获取pppd信息
 */
int lib_wl_pppd_info_get(lib_wl_t *wl, lib_wl_pppd_info_t *pppd, const unsigned int msec)
{
	if((wl == NULL) || (pppd == NULL))
		return LIB_WL_ERROR;
		
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_PPPD_INFO_GET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
		
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_PPPD_INFO_GET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(pppd, pkg.data, sizeof(lib_wl_pppd_info_t));

		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;		
}

/*
 * 获取当前流量
 */
int lib_wl_current_flow_info_get(lib_wl_t *wl, lib_wl_flow_info_t *flow_info, const unsigned int msec)
{
	if((wl == NULL) || (flow_info == NULL))
		return LIB_WL_ERROR;
		
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_CUR_FLOW_INFO);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
		
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_CUR_FLOW_INFO) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(flow_info, pkg.data, sizeof(lib_wl_flow_info_t));
		
		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;	
}

/*
 * 获取拨号时间
 */
int lib_wl_dial_time_get(lib_wl_t *wl, lib_wl_dial_time_t *dial_time, const unsigned int msec)
{
	if((wl == NULL) || (dial_time == NULL))
		return LIB_WL_ERROR;
		
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_DIAL_TIME);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
			
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_DIAL_TIME) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(dial_time, pkg.data, sizeof(lib_wl_dial_time_t));
		
		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;		
}

/*
 * 获取CSQ信息
 */
int lib_wl_csq_get(lib_wl_t *wl, lib_wl_csq_t *csq, const unsigned int msec)
{
	if((wl == NULL) || (csq == NULL))
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_CSQ_GET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
		
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_CSQ_GET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(csq, pkg.data, sizeof(lib_wl_csq_t));
		
		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;		
}

/*
 * 获取信号等级
 */
 
#define LV1_l        	 (1)
#define LV1_h        	 (6)
#define LV2_l      	 	(7)
#define LV2_h      	 (12)
#define LV3_l      	 	(13)
#define LV3_h      	 (19)
#define LV4_l      		 (20)
#define LV4_h      	 (26)
#define LV5_l      	 	(27)
#define LV5_h      	 (32)

enum WL_SIGNAL_LEVEL lib_wl_signal_level_get(lib_wl_t *wl, const unsigned int msec)
{
	if(wl == NULL)
		return LIB_WL_ERROR;
		
	int ret = LIB_WL_ERROR;
	enum WL_SIGNAL_LEVEL level = WL_SIGNAL_LEVEL0;
	unsigned char lv = 0;
	lib_wl_csq_sysinfo_t csq_sysinfo;
	
	memset(&csq_sysinfo, 0, sizeof(lib_wl_csq_sysinfo_t));
	
	ret = lib_wl_csq_sysinfo_get(wl, &csq_sysinfo, msec);
	if(ret == LIB_WL_EOK)
	{	
		lv = csq_sysinfo.rssi;
		if(lv == 0)
			level = WL_SIGNAL_LEVEL0;
		else if((lv >= LV1_l) && (lv <= LV1_h))
			level = WL_SIGNAL_LEVEL1;
		else if((lv >= LV2_l) && (lv <= LV2_h))
			level = WL_SIGNAL_LEVEL2;
		else if((lv >= LV3_l) && (lv <= LV3_h))
			level = WL_SIGNAL_LEVEL3;
		else if((lv >= LV4_l) && (lv <= LV4_h))
			level = WL_SIGNAL_LEVEL4;
		else if((lv >= LV5_l) && (lv <= LV5_h))
			level = WL_SIGNAL_LEVEL5;
	}
	
	return level;
}

/*
 * 设置网络模式
 */
int lib_wl_set_model(lib_wl_t *wl, enum WL_NETWORK_MODEL model, const unsigned int msec)
{
	if(wl == NULL)
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;
	unsigned int mdl = model;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_MODEL_SET);

	pkg.length = 4;  //数据长度
	memcpy(pkg.data, &mdl, 4); //网络模式
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN + 4);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
			
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_MODEL_SET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
			
		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;	
}

int lib_wl_get_gps_info(lib_wl_t *wl, wl_gps_info_t *info, const unsigned int msec)
{
	if((wl == NULL) || (info == NULL))
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_GPS_INFO);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
		
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_GPS_INFO) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		memcpy(info, pkg.data, sizeof(wl_gps_info_t));
		
		if(pkg.result == WL_RESULT_OK)  
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;	
}

int lib_wl_get_gps_zgpsr(lib_wl_t *wl, wl_zgpsr_t *zgpsr, const unsigned int msec)
{
	if((wl == NULL) || (zgpsr == NULL))
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR;
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_GPS_ZGPSR);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
		
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_GPS_ZGPSR) != LIB_WL_TRUE)
			return LIB_WL_ERROR;

		if(pkg.result == WL_RESULT_OK)  
		{
			memcpy(zgpsr, pkg.data, sizeof(wl_zgpsr_t));
			return LIB_WL_EOK;
		}
		else
			return LIB_WL_ERROR;
	}
	
	return ret;		
}

int lib_wl_dial_stat_change_cb(lib_wl_t *wl, fn_wl *fn)
{


	return LIB_WL_EOK;
}

int lib_wl_3g_hw_reset(lib_wl_t *wl, const unsigned int msec)
{
	if(wl == NULL)
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR; 
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_3G_HW_RESET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
			
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_3G_HW_RESET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		if(pkg.result == WL_RESULT_OK)  //返回真
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;		
}


/* 模块重启--AT指令重启 add by zjc at 2016-11-09 */
int lib_wl_3g_soft_reset(lib_wl_t *wl, const unsigned int msec)
{
	if(wl == NULL)
		return LIB_WL_ERROR;
	
	int ret = LIB_WL_ERROR; 
	wl_unix_package_t pkg;

	int pkg_len = sizeof(struct wl_unix_package);
	memset(&pkg, 0, pkg_len);

	__wl_unix_package_create(&pkg, LIB_WL_CMD_3G_SOFT_RESET);
	ret = lib_tcp_writen(wl->sockfd, &pkg, WL_PKG_HEAD_LEN);
	if(ret < 0)
	{
		__wl_reconnect(wl);
	}
	
	memset(&pkg, 0, pkg_len);
	ret = lib_tcp_read_select(wl->sockfd, &pkg, WL_PKG_LEN, msec);
	if(ret > 0)
	{
		if(__wl_unix_pkg(&pkg, ret) != LIB_WL_EOK)
			return LIB_WL_ERROR;
			
		if(__is_wl_unix_package(&pkg, LIB_WL_CMD_3G_SOFT_RESET) != LIB_WL_TRUE)
			return LIB_WL_ERROR;
		
		if(pkg.result == WL_RESULT_OK)  //返回真
			return LIB_WL_EOK;
		else
			return LIB_WL_ERROR;
	}
	
	return ret;		
}





