#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "lib_general.h"
#include "gpio_ctrl.h"
#include "zte_mc2716.h"

#include <syslog.h>



#if 0
#define MC_API_DBG

#ifdef MC_API_DBG
#define DBG(fmt, ...)	fprintf(stderr, ""__FILE__":%d:%s(): "fmt"\n", __LINE__, __FUNCTION__, ##__VA_ARGS__);\
	//					log_write(""fmt"\n", strlen(fmt) + 1, 1);
#else
#define DBG(fmt, ...)
#endif
#endif


#define MC_DBG

#ifdef MC_DBG
#define DBG(fmt, args...)	 do{ \
	fprintf(stderr, "DEBUG: %s:%d, ", __FUNCTION__,__LINE__); \
	fprintf(stderr, fmt, ##args); \
}while(0)
#else 
#define DBG(fmt, args...)
#endif


#define WL_RECV_DELAY_MS			300

#define ZTE_CR				'\r'
#define ZTE_LF				'\n'
#define ZTE_CRLF				"\r\n"
#define ZTE_OK				"OK"

/*
 * 中兴MC2716 AT指令
 */
#define ZTE_RESET			"AT^RESET\r"   		//复位
#define ZTE_AT				"AT\r"				//AT测试
#define ZTE_ATE0				"ATE0\r"   			//关闭回显
#define ZTE_CGMR     			"AT+CGMR\r"   			//查询模块软件版本
#define ZTE_CGMI     			"AT+CGMI\r"   			//厂商信息查询
#define ZTE_CGMM     			"AT+CGMM\r"   		//产品名称查询
#define ZTE_HWVER     			"AT^HWVER\r"   		//查询硬件版本
#define ZTE_VOLT     			"AT^VOLT\r"   			//查询电压
#define ZTE_SYSINFO			"AT^SYSINFO\r"		//系统的信息查询
#define ZTE_CSQ				"AT+CSQ\r"			//信号强度查询
#define ZTE_ZPS				"AT+ZPS?\r"			//查询当前是否在上网

#define ZTE_PREFMODE_SET 		"AT^PREFMODE=2\r"   //2:CDMA, 8:强制设置优先网络模式为CDMA/HDR HYBRID 模式
#define ZTE_PREFMODE_GET 		"AT^PREFMODE?\r"



static int g_mc_dial_err_cnt = 0;


static void __dial_err_cnt_inc(void)
{
	g_mc_dial_err_cnt++;
}

static void __dial_err_cnt_zero(void)
{
	g_mc_dial_err_cnt = 0;
}

static int __dial_err_cnt_get(void)
{
	return g_mc_dial_err_cnt;
}

static int __get_useful_info(char *src, char *key_s, char *des)
{
	if((src == NULL) || (des == NULL))
		return -1;

	char *p1 = NULL;
	char *p2 = NULL;
	char *p3 = NULL;
	int nlen = 0;

	p2 = strstr(src, ZTE_OK);
	if(p2 == NULL)
		return -1;

	if(key_s == NULL)
	{
		p3 = src;
		while(p3 < (p2 - 2 * strlen(ZTE_CRLF)))
		{
			p1 = p3 + strlen(ZTE_CRLF);
			p3 = strstr(p1, ZTE_CRLF);
		}

		if(p1 == NULL)
			return -1;
	}
	else
	{
		p1 = strstr(src, key_s);
		if(p1 == NULL)
			return -1;

		p1 += strlen(key_s);		
	}

	if((nlen = p2 - p1 - 2 * strlen(ZTE_CRLF)) < 1)
		return -1;

	memcpy(des, p1, nlen);
	
	return nlen;
}


int mc_dial_err_cnt_get(void)
{
	int dial_err_cnt = 0;
	
	dial_err_cnt =  __dial_err_cnt_get();
	__dial_err_cnt_zero();
	
	return dial_err_cnt;
}


int mc_at_set(const int fd)
{
	int ret = -1;
	unsigned char rxbuf[128] = {0};

	if(fd <= 0)
		return MC_ERROR;

	ret = lib_serial_send(fd, (unsigned char *)ZTE_AT, strlen(ZTE_AT));
	if(ret != strlen(ZTE_AT))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_AT write failed\n");
		return MC_ERROR;
	}

	ret = lib_serial_readn_select(fd, rxbuf, sizeof(rxbuf), WL_RECV_DELAY_MS);
	if(ret > 0)
	{
		if(strstr((char *)rxbuf, "OK") != NULL)
		{
			__dial_err_cnt_zero();
			return MC_EOK;
		}
	}

	__dial_err_cnt_inc();
	return MC_ERROR;
}

int mc_ate0_set(const int fd)
{
	int ret = -1;
	unsigned char rxbuf[128] = {0};

	if(fd <= 0)
		return MC_ERROR;
	
	ret = lib_serial_send(fd, (unsigned char *)ZTE_ATE0, strlen(ZTE_ATE0));
	if(ret != strlen(ZTE_ATE0))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_ATE0 write failed\n");
		return MC_ERROR;
	}

	ret = lib_serial_readn_select(fd, rxbuf, sizeof(rxbuf), WL_RECV_DELAY_MS);
	if(ret > 0)
	{
		if(strstr((char *)rxbuf, "OK") != NULL)
		{
			__dial_err_cnt_zero();
			return MC_EOK;
		}
	}

	__dial_err_cnt_inc();
	return MC_ERROR;
}

int mc_reset_set(const int fd)
{
	int ret = -1;

	if(fd <= 0)
		return MC_ERROR;
	
	ret = lib_serial_send(fd, (unsigned char *)ZTE_RESET, strlen(ZTE_RESET));
	if(ret != strlen(ZTE_RESET))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_RESET write failed\n");
		return MC_ERROR;
	}

#if 0
	ret = lib_serial_read_select(fd, rxbuf, sizeof(rxbuf), 1000);
	if(ret > 0)
	{
		if(strstr((char *)rxbuf, "OK") != NULL)
		{
			lib_close(fd);
			return MC_EOK;
		}
	}
#endif

	__dial_err_cnt_zero();
	lib_close(fd);
	return MC_EOK;
}

int mc_cgmr_get(const int fd, wl_cgmr_t *wl)
{
	if(wl == NULL)
		return MC_ERROR;
	
	int ret = -1;
	unsigned char rxbuf[512] = {0};
	char str[512] = {0};
	char *p1 = NULL;
	char *p2 = NULL;
		
	if(fd <= 0)
		return MC_ERROR;
	
	ret = lib_serial_send(fd, (unsigned char *)ZTE_CGMR, strlen(ZTE_CGMR));
	if(ret != strlen(ZTE_CGMR))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_CGMR write failed\n");
		return MC_ERROR;
	}

	ret = lib_serial_readn_select(fd, rxbuf, sizeof(rxbuf), WL_RECV_DELAY_MS);
	if(ret > 0)
	{
		ret = __get_useful_info((char *)rxbuf, "+CGMR:", str);
		if(ret > 0)
		{
			p1 = strtok_r(str, ZTE_CRLF, &p2);
			strcpy(wl->softversion, p1);
			__dial_err_cnt_zero();
			return MC_EOK;
		}

		DBG("ZTE_CGMR get useful info failed\n");
	}	

	__dial_err_cnt_inc();
	DBG("ZTE_CGMR read failed\n");
	return MC_ERROR;
}

int mc_hwver_get(const int fd, wl_hwver_t *wl)
{
	if(wl == NULL)
		return MC_ERROR;
	
	int ret = -1;
	unsigned char rxbuf[512] = {0};
	char str[512] = {0};
	char *p1 = NULL;
	char *p2 = NULL;
		
	if(fd <= 0)
		return MC_ERROR;

	ret = lib_serial_send(fd, (unsigned char *)ZTE_HWVER, strlen(ZTE_HWVER));
	if(ret != strlen(ZTE_HWVER))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_HWVER write failed\n");
		return MC_ERROR;
	}

	ret = lib_serial_readn_select(fd, rxbuf, sizeof(rxbuf), WL_RECV_DELAY_MS);
	if(ret > 0)
	{
		ret = __get_useful_info((char *)rxbuf, "^HWVER:", str);
		if(ret > 0)
		{
			p1 = strtok_r(str, ZTE_CRLF, &p2);
			strcpy(wl->firmversion, p1);
			__dial_err_cnt_zero();
			return MC_EOK;
		}

		DBG("ZTE_HWVER get useful info failed\n");
	}	

	__dial_err_cnt_inc();
	DBG("ZTE_HWVER read failed\n");
	return MC_ERROR;
}

int mc_csq_get(const int fd, wl_csq_t *wl)
{
	if(wl == NULL)
		return MC_ERROR;
	
	int ret = -1;
	unsigned char rxbuf[512] = {0};
	char *p0 = NULL;
	char *p1 = NULL;
	char *p2 = NULL;
	char *p3 = NULL;
		
	if(fd <= 0)
		return MC_ERROR;

	ret = lib_writen(fd, (unsigned char *)ZTE_CSQ, strlen(ZTE_CSQ));
	if(ret != strlen(ZTE_CSQ))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_CSQ write failed\n");
		return MC_ERROR;
	}

	ret = lib_serial_readn_select(fd, rxbuf, sizeof(rxbuf), 1000);
	if(ret > 0)
	{
		p0 = strstr((char *)rxbuf, "+CSQ:");
		if(p0 != NULL)
		{
			p1 = p0 + strlen("+CSQ:");
			if(p1 != NULL)
			{
				p2 = strtok_r(p1, ",", &p3);
				sscanf(p2, "%d", (int *)&(wl->rssi));

				p2 = strtok_r(NULL, ",", &p3);
				sscanf(p2, "%d", (int  *)&(wl->fer));
				__dial_err_cnt_zero();
				return MC_EOK;
			}
		}

		DBG("ZTE_CSQ get \" +CSQ:\" failed\n");
	}	

	__dial_err_cnt_inc();
	DBG("ZTE_CSQ read failed\n");
	return MC_ERROR;
}

int mc_sysinfo_get(const int fd, wl_sysinfo_t *wl)
{
	if(wl == NULL)
		return MC_ERROR;
	
	int ret = -1;
	unsigned char rxbuf[512] = {0};
	char *p0 = NULL;
	char *p1 = NULL;
	char *p2 = NULL;
	char *p3 = NULL;

	if(fd <= 0)
		return MC_ERROR;

	ret = lib_writen(fd, (unsigned char *)ZTE_SYSINFO, strlen(ZTE_SYSINFO));
	if(ret != strlen(ZTE_SYSINFO))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_SYSINFO write failed\n");
		return MC_ERROR;
	}

	ret = lib_serial_readn_select(fd, rxbuf, sizeof(rxbuf), 1000);
	if(ret > 0)
	{
		p0 = strstr((char *)rxbuf, "^SYSINFO:");
		if(p0 != NULL)
		{
			p1 = p0 + strlen( "^SYSINFO:");
			if(p1 != NULL)
			{
				p2 = strtok_r(p1, ",", &p3);
				sscanf(p2, "%d", (int *)&(wl->srv_status));

				p2= strtok_r(NULL, ",", &p3);
				sscanf(p2, "%d", (int *)&(wl->srv_domain));

				p2 = strtok_r(NULL, ",", &p3);
				sscanf(p2, "%d", (int *)&(wl->roam_status));

				p2 = strtok_r(NULL, ",", &p3);
				sscanf(p2, "%d", (int *)&(wl->sys_mode));

				p2 = strtok_r(NULL, ",", &p3);
				sscanf(p2, "%d", (int *)&(wl->sim_state));    //UIM卡状态, 1:有效, 255:不存在

				__dial_err_cnt_zero();
				return MC_EOK;
			}
		}

		DBG("ZTE_SYSINFO get \" ^SYSINFO:\" failed\n");
	}	

	__dial_err_cnt_inc();
	DBG("ZTE_SYSINFO read failed\n");
	return MC_ERROR;	
}

int mc_zps_get(const int fd, wl_zps_t *wl)
{
	if(wl == NULL)
		return MC_ERROR;
	
	int ret = -1;
	unsigned char rxbuf[512] = {0};
	char str[512] = {0};
	char *p1 = NULL;
	char *p2 = NULL;
		
	if(fd <= 0)
		return MC_ERROR;
	
	ret = lib_serial_send(fd, (unsigned char *)ZTE_ZPS, strlen(ZTE_ZPS));
	if(ret != strlen(ZTE_ZPS))
	{
		__dial_err_cnt_inc();
		DBG("ZTE_ZPS write failed\n");
		return MC_ERROR;
	}

	ret = lib_serial_readn_select(fd, rxbuf, sizeof(rxbuf), WL_RECV_DELAY_MS);
	if(ret > 0)
	{
		ret = __get_useful_info((char *)rxbuf, "+ZPS:", str);
		if(ret > 0)
		{
			p1 = strtok_r(str, ",", &p2);
			sscanf(p1, "%d", (int *)&(wl->state));

			__dial_err_cnt_zero();
			return MC_EOK;
		}

		DBG("ZTE_CSQ get \" +ZPS:\" failed\n");
	}	

	__dial_err_cnt_inc();
	DBG("ZTE_ZPS read failed\n");
	return MC_ERROR;	
}


/* 强制优先使用3g的网络模式 add by zjc at 2016-11-08 */
#define SYS_LOG_DEBUGG(fmt, args...) 		syslog(LOG_DEBUG, fmt, ##args)

int mc_prefmode_set(const int fd)
{
	int ret = -1;
	unsigned char rxbuf[32] = {0};
	
	if(fd <= 0)
		return MC_ERROR;
	
	ret = lib_serial_send(fd, (unsigned char *)ZTE_PREFMODE_SET, strlen(ZTE_PREFMODE_SET));
	if(ret != strlen(ZTE_PREFMODE_SET))
	{
		DBG("ZTE_PREFMODE_SET write failed\n");
		return MC_ERROR;
	}
	SYS_LOG_DEBUGG("------haha, mc_prefmode_set write ok\n");

#if 1
	ret = lib_serial_read_select(fd, rxbuf, sizeof(rxbuf), 2000);
	if(ret > 0)
	{
		SYS_LOG_DEBUGG("------haha, mc_prefmode_set, receive:%s\n", rxbuf);
	}
#endif

	//__dial_err_cnt_zero();
	//lib_close(fd);
	return MC_EOK;
}


int mc_prefmode_get(const int fd)
{
	int ret = -1;
	unsigned char rxbuf[32] = {0};
	
	if(fd <= 0)
		return MC_ERROR;
	
	ret = lib_serial_send(fd, (unsigned char *)ZTE_PREFMODE_GET, strlen(ZTE_PREFMODE_GET));
	if(ret != strlen(ZTE_PREFMODE_GET))
	{
		DBG("ZTE_PREFMODE_GET write failed\n");
		return MC_ERROR;
	}
	SYS_LOG_DEBUGG("------haha, mc_prefmode_get write ok\n");

#if 1
	ret = lib_serial_read_select(fd, rxbuf, sizeof(rxbuf), 2000);
	if(ret > 0)
	{
		SYS_LOG_DEBUGG("------haha, mc_prefmode_get, receive:%s\n", rxbuf);
	}
#endif

	//__dial_err_cnt_zero();
	//lib_close(fd);
	return MC_EOK;
}


/* 模块软件重启 add by zjc at 2016-11-09 */
int mc_module_reset(const int fd)
{
	int ret = -1;
	unsigned char rxbuf[32] = {0};
	
	if(fd <= 0)
		return MC_ERROR;
		
	ret = lib_serial_send(fd, (unsigned char *)ZTE_RESET, strlen(ZTE_RESET));
	if(ret != strlen(ZTE_RESET))
	{
		DBG("ZTE_RESET write failed\n");
		SYS_LOG_DEBUGG("------haha, mc_module_reset write error!\n");
		
		return MC_ERROR;
	}
	SYS_LOG_DEBUGG("------haha, mc_module_reset write ok\n");

#if 1
	ret = lib_serial_read_select(fd, rxbuf, sizeof(rxbuf), 2000);
	if(ret > 0)
	{
		SYS_LOG_DEBUGG("------haha, mc_module_reset, receive:%s\n", rxbuf);
	}
#endif

	//__dial_err_cnt_zero();
	//lib_close(fd);
	return MC_EOK;
}



