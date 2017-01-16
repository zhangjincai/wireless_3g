#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "lib_general.h"
#include "lib_eventloop.h"
#include "zte_mc2716.h"
#include "gpio_ctrl.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

//add ay zjc begin
#define CONFIG_NO_CARRIER_CHECK	0 //SIM卡欠费检测

#define	SIM_REBOOT_PATH		"/opt/logpath/sim_reboot_times.txt" //记录自SIM卡识别不到后系统重启次数
#define	DIAL_REBOOT_PATH	"/opt/logpath/dail_reboot_times.txt" //记录拨号失败后系统重启次数
#define	TTYUSB1_REBOOT_PATH	"/opt/logpath/ttyusb1_reboot_times.txt" //记录拨号失败后系统重启次数

pthread_mutex_t g_reboot_mutex = PTHREAD_MUTEX_INITIALIZER; //系统重启读写访问互斥锁(SIM卡识别不到)
pthread_mutex_t g_reboot_mutex1 = PTHREAD_MUTEX_INITIALIZER; //系统重启读写访问互斥锁(拨号超时)
//add by zjc end

#define WL_APP_ERR
#define WL_APP_DBG


#ifdef WL_APP_ERR
#define WL_ERR(fmt, args...)	 do{ \
	fprintf(stderr, "ERROR: %s:%d, ", __FUNCTION__,__LINE__); \
	fprintf(stderr, fmt, ##args); \
}while(0)
#else
#define WL_ERR(fmt, args...)
#endif

#ifdef WL_APP_DBG
#define WL_DBG(fmt, args...)	 fprintf(stderr, fmt, ##args)
#else
#define WL_DBG(fmt, args...)
#endif



/* 日志定义 */
#define WL_LOG_RUN
#define TERMINAL_NO_PATH			"/opt/config/terminal_no_file.txt"

#ifdef WL_LOG_RUN
#include <syslog.h>
#define SYS_LOG_EMERG(fmt, args...) 		syslog(LOG_EMERG, fmt, ##args)
#define SYS_LOG_ALERT(fmt, args...) 			syslog(LOG_ALERT, fmt, ##args)
#define SYS_LOG_CRIT(fmt, args...) 			syslog(LOG_CRIT, fmt, ##args)
#define SYS_LOG_ERR(fmt, args...) 			syslog(LOG_ERR, fmt, ##args)
#define SYS_LOG_WARNING(fmt, args...) 		syslog(LOG_WARNING, fmt, ##args) 
#define SYS_LOG_NOTICE(fmt, args...)  		syslog(LOG_NOTICE, fmt, ##args)
#define SYS_LOG_INFO(fmt, args...) 			syslog(LOG_INFO, fmt, ##args)
#define SYS_LOG_DEBUG(fmt, args...) 		syslog(LOG_DEBUG, fmt, ##args)
#else
#define SYS_LOG_EMERG(fmt, args...) 		fprintf(stderr, fmt, ##args)
#define SYS_LOG_ALERT(fmt, args...) 			fprintf(stderr, fmt, ##args)
#define SYS_LOG_CRIT(fmt, args...) 			fprintf(stderr, fmt, ##args)
#define SYS_LOG_ERR(fmt, args...) 			fprintf(stderr, fmt, ##args)
#define SYS_LOG_WARNING(fmt, args...) 		fprintf(stderr, fmt, ##args)
#define SYS_LOG_NOTICE(fmt, args...) 		fprintf(stderr, fmt, ##args)
#define SYS_LOG_INFO(fmt, args...) 			fprintf(stderr, fmt, ##args)
#define SYS_LOG_DEBUG(fmt, args...) 		fprintf(stderr, fmt, ##args)
#endif

/*
 * 函数返回码定义
 */
#define WL_NULL					(NULL) 
#define WL_EOK					(0)  //正常
#define WL_ERROR				(-1) //错误
#define WL_ETIMEOUT				(-2) //超时
#define WL_EFULL					(-3) //满
#define WL_EEMPTY				(-4) //空
#define WL_ENOMEM 				(-5) //内存不够
#define WL_EXCMEM				(-6) //内存越界
#define WL_EBUSY				(-7) //忙
#define WL_ERR_COMMAND			(-8) //不支持该指令

#define WL_ERR_NO_CARRIER			(-9) //NO CARRIER


#define WL_TRUE					(1)
#define WL_FALSE					(0)


/*
 * 拨号状态
 */
enum WL_DIAL_STAT
{
	WL_DIAL_STAT_UNDIAL = 0,
	WL_DIAL_STAT_DIALING,
	WL_DIAL_STAT_DIALOK,
	WL_DIAL_STAT_SOFT_RESET,
	WL_DIAL_STAT_HW_RESET,
	WL_DIAL_STAT_HW_RESTART,
	WL_DIAL_STAT_NOT_SIM
};

/*
 * 网络模式
 */
enum WL_NETWORK_MODEL
{
	WL_NETWORK_MODEL_WIRED = 1,
	WL_NETWORK_MODEL_WIRELESS,
};

/*
 * 操作指令
 */
#define WL_CMD_RESET_SET				0x01
#define WL_CMD_DIAL_STAT_GET			0x02
#define WL_CMD_HWVER_GET				0x03
#define WL_CMD_SYSINFO_GET			0x04
#define WL_CMD_CGMR_GET				0x05
#define WL_CMD_CGMI_GET				0x06
#define WL_CMD_CGMM_GET				0x07
#define WL_CMD_CSQ_GET				0x08
#define WL_CMD_PPPD_INFO_GET			0x09
#define WL_CMD_PPPD_ONLINE			0x10
#define WL_CMD_CSQ_SYSINFO			0x11
#define WL_CMD_CUR_FLOW_INFO			0x12
#define WL_CMD_DIAL_TIME				0x13
#define WL_CMD_FN_CB					0x14   //回调
#define WL_CMD_MODEL_SET				0x15
#define WL_CMD_3G_HW_RESET			0x18
#define WL_CMD_3G_SOFT_RESET			0x19

/*
 * 报文结构
 */
#define WL_PKG_HEAD_LEN		5
#define WL_PKG_DATA_LEN		320
#define WL_PKG_LEN				(WL_PKG_HEAD_LEN + WL_PKG_DATA_LEN)

struct wl_unix_package
{
	unsigned char id_h;		//关键字
	unsigned char id_l;
	unsigned char cmd;		//命令字
	unsigned char result;	//结果
	unsigned char length;	//长度
	unsigned char data[WL_PKG_DATA_LEN];
}__attribute__((packed));
typedef struct wl_unix_package wl_unix_package_t;

#define WL_ID_H			0x55
#define WL_ID_L			0xaa


/* 协议返回结果 */
enum WL_RESULT
{
	WL_RESULT_OK = 1,
	WL_RESULT_ERR = 2,
	WL_RESULT_PKG_ERR = 3,
	WL_RESULT_NOT_CMD = 4
};

struct wl_kill_pppd
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int wait;
};

#define WL_KILL_PPPD_SEC				(60) //30-->90 by zjc at 2016-11-24		
#define WL_DIAL_PPPD_SEC				(10)
#define WL_WIRED_SLEEP					(15)

#define WL_UNIX_DOMAIN					"/tmp/lib_wl_unix.domain"
#define WL_PERM							(0777)
#define WL_DEV3G_PATH					"/dev/MC27161" //"/dev/ttyUSB1"   
#define WL_DIAL_MAX_LEVEL1				(1)  //5-->1 by zjc at 2016-11-24
#define WL_DIAL_MAX_LEVEL2				(3) //10-->3 by zjc at 2016-11-24
#define WL_DIAL_MAX_LEVEL3				(5) //15-->5 by zjc at 2016-11-24
#define WL_CSQ_MIN						(5)
#define WL_RD_AT_CNT_MAX				(2) //6-->2 by zjc at 2016-11-23
#define WL_USING_CHL_MAX				(128)

#define WL_DEV2_PATH					"/dev/MC27162"
#define WL_DEV3_PATH					"/dev/MC27163"


static lib_event_loop_t *g_eventloop = NULL;
static int g_unix_sockfd = -1;
static unsigned int g_network_model = WL_NETWORK_MODEL_WIRELESS;
static unsigned int g_dial_stat = WL_DIAL_STAT_UNDIAL;
static unsigned int g_dial_count = 0;
static lib_serial_t g_usb_serial;
static int g_wl_dev2_fd = -1, g_wl_dev3_fd = -1;
static long long g_rx_bytes = 0;
static long long g_tx_bytes = 0;
static lib_mutex_t *g_pppd_info_mutex = NULL;
static lib_mutex_t *g_csq_sysinfo_mutex = NULL;
static wl_pppd_info_t g_wl_pppd_info;
static wl_csq_sysinfo_t g_wl_csq_sysinfo;
static wl_dial_time_t g_wl_dial_time;
static int g_ttyusb_exist = WL_FALSE;
static int g_rd_at_cnt = 0;
static struct wl_kill_pppd g_kill_pppd = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 1};

static int g_sim_state_cnt = 0; //SIM卡状态计数

static void *wl_handle_thread(void *arg);
static void *wl_kill_pppd_thread(void *arg);
static void unix_read_proc(lib_event_loop_t *ep, int fd, void *client_data, int mask);
static void unix_accept_proc(lib_event_loop_t *ep, int fd, void *client_data, int mask);
static int wl_unix_package_explain(const int sockfd, void *ptr, const int len);
static void wl_pppd_info_destroy(void);
static void wl_csq_sysinfo_destroy(void);
static int ttyusb_init(void);
static void ttyusb_close(void);
static void wl_3g_power_reset(void);

/*
 * SIGHUP 1 A 终端挂起或者控制进程终止 
 * SIGINT 2 A 键盘中断（如break键被按下） 
 * SIGQUIT 3 C 键盘的退出键被按下 
 * SIGILL 4 C 非法指令 
 * SIGABRT 6 C 由abort(3)发出的退出指令 
 * SIGFPE 8 C 浮点异常 
 * SIGKILL 9 AEF Kill信号 
 * SIGSEGV 11 C 无效的内存引用 
 * SIGPIPE 13 A 管道破裂: 写一个没有读端口的管道 
 * SIGALRM 14 A 由alarm(2)发出的信号 
 * SIGTERM 15 A 终止信号 
 * SIGUSR1 30,10,16 A 用户自定义信号1 
 * SIGUSR2 31,12,17 A 用户自定义信号2 
 * SIGCHLD 20,17,18 B 子进程结束信号 
 * SIGCONT 19,18,25 进程继续（曾被停止的进程） 
 * SIGSTOP 17,19,23 DEF 终止进程 
 * SIGTSTP 18,20,24 D 控制终端（tty）上按下停止键 
 * SIGTTIN 21,21,26 D 后台进程企图从控制终端读 
 * SIGTTOU 22,22,27 D 后台进程企图从控制终端写 
 */
static void sigint(int sig)
{
	fprintf(stderr, "wireless signal: %d\n", sig);
}

static void signals_init(void)
{
	struct sigaction sa;

	signal(SIGPIPE, SIG_IGN); //管道关闭
	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	
	signal(SIGTTOU, SIG_IGN); //后台进程写控制终端
	signal(SIGTTIN, SIG_IGN); //后台进程读控制终端
	signal(SIGTSTP, SIG_IGN); //终端挂起

	
	sa.sa_flags = 0;
    	sigaddset(&sa.sa_mask, SIGTERM);
	sa.sa_handler = sigint;
	sigaction(SIGTERM, &sa, NULL);
}

static unsigned int __network_model_get(void)
{
	return lib_atmoic_get(&g_network_model);
}

static void __network_model_set(const unsigned int model)
{
	lib_atmoic_set(&g_network_model, model);
}

static unsigned int dial_stat_get(void)
{
	return lib_atmoic_get(&g_dial_stat);
}

static void dial_stat_set(const unsigned int stat)
{
	lib_atmoic_set(&g_dial_stat, stat);
}

static unsigned int dial_count_inc(void)
{
	return lib_atmoic_inc(&g_dial_count);
}

static int dial_count_get(void)
{
	return lib_atmoic_get(&g_dial_count);
}

static void dial_count_zero(void)
{
	lib_atmoic_set(&g_dial_count, 0);
}

static void wl_pppd_info_init(void)
{
	memset(&g_wl_pppd_info, 0, sizeof(struct wl_pppd_info));

	g_pppd_info_mutex = lib_mutex_create();
}

static void wl_pppd_info_put(wl_pppd_info_t *wl)
{
	
	lib_mutex_lock(g_pppd_info_mutex);

	memcpy(&g_wl_pppd_info, wl, sizeof(wl_pppd_info_t));

	lib_mutex_unlock(g_pppd_info_mutex);
}

static void wl_pppd_info_get(wl_pppd_info_t *wl)
{
	lib_mutex_lock(g_pppd_info_mutex);

	memcpy(wl, &g_wl_pppd_info, sizeof(wl_pppd_info_t));

	lib_mutex_unlock(g_pppd_info_mutex);
}

static void wl_pppd_info_clean(void)
{
	lib_mutex_lock(g_pppd_info_mutex);

	memset(&g_wl_pppd_info, 0, sizeof(struct wl_pppd_info));
		
	lib_mutex_unlock(g_pppd_info_mutex);
}

static void wl_pppd_info_destroy(void)
{
	memset(&g_wl_pppd_info, 0, sizeof(struct wl_pppd_info));
	
	lib_mutex_destroy(g_pppd_info_mutex);
}

static void wl_csq_sysinfo_init(void)
{
	memset(&g_wl_csq_sysinfo, 0, sizeof(struct wl_csq_sysinfo));

	g_csq_sysinfo_mutex = lib_mutex_create();
}

static void wl_csq_sysinfo_put(wl_csq_sysinfo_t *wl)
{
	lib_mutex_lock(g_csq_sysinfo_mutex);

	memcpy(&g_wl_csq_sysinfo, wl, sizeof(struct wl_csq_sysinfo));

	lib_mutex_unlock(g_csq_sysinfo_mutex);
}

static void wl_csq_sysinfo_get(wl_csq_sysinfo_t *wl)
{
	lib_mutex_lock(g_csq_sysinfo_mutex);

	memcpy(wl, &g_wl_csq_sysinfo, sizeof(struct wl_csq_sysinfo));

	lib_mutex_unlock(g_csq_sysinfo_mutex);
}

static void wl_csq_sysinfo_clean(void)
{
	lib_mutex_lock(g_csq_sysinfo_mutex);

	memset(&g_wl_csq_sysinfo, 0, sizeof(struct wl_csq_sysinfo));
		
	lib_mutex_unlock(g_csq_sysinfo_mutex);
}

static void wl_csq_sysinfo_destroy(void)
{
	memset(&g_wl_csq_sysinfo, 0, sizeof(struct wl_csq_sysinfo));
	
	lib_mutex_destroy(g_csq_sysinfo_mutex);
}

static int is_ttyusb0_exist(void)
{
	if(access("/dev/MC27160", F_OK) == 0)
		return WL_TRUE;

	return WL_FALSE;
}

static int is_ttyusb1_exist(void)
{
	if(access("/dev/MC27161", F_OK) == 0)
		return WL_TRUE;

	return WL_FALSE;
}

/* TTY USB 初始化 */
static int ttyusb_init(void)
{
	int err = -1;
	static int reboot_flag = 0;
	static int gpio_ctrl_fd = -1;
	FILE *logfd = NULL;
	static int sys_reboots = 0;
	
	if(g_usb_serial.sfd > 0)
	{
		lib_close(g_usb_serial.sfd);
		g_usb_serial.sfd = -1;
	}

	if(g_wl_dev2_fd > 0)
	{
		lib_close(g_wl_dev2_fd);
		g_wl_dev2_fd = -1;
	}

	if(g_wl_dev3_fd > 0)
	{
		lib_close(g_wl_dev3_fd);
		g_wl_dev3_fd = -1;
	}

	if(is_ttyusb1_exist())  //检查ttyusb1设备描述符是否存在
	{
		logfd = fopen(TTYUSB1_REBOOT_PATH, "wb"); //拨号失败导致的重启次数清零
		if(NULL != logfd)
		{
			sys_reboots = 0;
			
			fwrite(&sys_reboots, sizeof(sys_reboots), 1, logfd);
			fclose(logfd); 
		}		
			
		memset(&g_usb_serial, 0, sizeof(lib_serial_t));
		
		/* USB串口初始化*/
		strcpy(g_usb_serial.pathname, WL_DEV3G_PATH);
		g_usb_serial.flags = O_RDWR;
		g_usb_serial.speed = 115200;
		g_usb_serial.databits = 8;
		g_usb_serial.stopbits = 1;
		
		err = lib_serial_init(&g_usb_serial);
		if(err == LIB_GE_ERROR)
		{
			g_rd_at_cnt++;  //计算初始化串口失败次数
			if(g_rd_at_cnt > WL_RD_AT_CNT_MAX)
			{
				
				SYS_LOG_DEBUG("ttyusb1 exist: wireless read AT count %d\n", WL_RD_AT_CNT_MAX);

				wl_3g_power_reset();
				g_rd_at_cnt = 0;
			}
			
			WL_ERR("usb serial init failed!\n");
			SYS_LOG_ERR("usb serial init failed!\n");
			
			return WL_ERROR;
		}

		/* 3G设备符 */
		g_wl_dev2_fd = open(WL_DEV2_PATH, O_RDWR);
		if(g_wl_dev2_fd < 0)
		{
			WL_ERR("wl open %s failed!\n", WL_DEV2_PATH);
			return WL_ERROR;
		}

		g_wl_dev3_fd = open(WL_DEV3_PATH, O_RDWR);
		if(g_wl_dev3_fd < 0)
		{
			WL_ERR("wl open %s failed!\n", WL_DEV3_PATH);
			return WL_ERROR;
		}
	}
	else
	{
		g_rd_at_cnt++;
		if(g_rd_at_cnt > WL_RD_AT_CNT_MAX)
		{
			SYS_LOG_DEBUG("ttyusb1 NOT exist: wireless read AT count %d\n", g_rd_at_cnt);

			//ttyusb1不存在时先复位HUB芯片，还是不行则重启节点机 add by zjc at 2016-11-24
			if(reboot_flag == 0) //复位USB HUB芯片
			{
				gpio_ctrl_fd = open("/dev/gpio_ctrl", O_RDWR);
				if(gpio_ctrl_fd >= 0)
				{
					err = ioctl(gpio_ctrl_fd, GPIO_CTRL_SET_PWR_USB_HUB, GPIO_CTRL_SW_OFF);
					sleep(1);
					err = ioctl(gpio_ctrl_fd, GPIO_CTRL_SET_PWR_USB_HUB, GPIO_CTRL_SW_ON);
			
					if(gpio_ctrl_fd >= 0)
						lib_close(gpio_ctrl_fd);
	
					reboot_flag = 1;
					
					SYS_LOG_DEBUG("ttyusb1 NOT exist: reset USB HUB\n");
				}
			}
			else if(reboot_flag == 1) //重启节点机
			{
				//将系统重启次数记录下来, 防止ttyusb1一直不存在时节点机频繁重启
				logfd = fopen(TTYUSB1_REBOOT_PATH, "rb");
				if(NULL != logfd)
				{
					fread(&sys_reboots, sizeof(sys_reboots), 1, logfd);
					fclose(logfd);
				}
			
				logfd = fopen(TTYUSB1_REBOOT_PATH, "wb");
				if(NULL != logfd)
				{
					sys_reboots++;
					
					fwrite(&sys_reboots, sizeof(sys_reboots), 1, logfd);
					fclose(logfd); 
				}	

				if(sys_reboots < 2)
				{
					SYS_LOG_DEBUG("ttyusb1 NOT exist: reboot system %d times!!!\n", sys_reboots);
					sleep(3);
					
					system("/mnt/firmware/reboot_wdt");
				}
			}
			
			g_rd_at_cnt = 0;
		}
		
		return WL_ERROR;
	}

	g_rd_at_cnt = 0; //错误清0

	reboot_flag = 0;
	
	lib_setfd_noblock(g_usb_serial.sfd);

	fprintf(stderr, "WIRLESS APP usb serial init succes, usb serial fd  = %d\n", g_usb_serial.sfd );

	return WL_EOK;
}

/* 重新初始化串口 */
static int __ttyusb_reinit(void)
{
	return ttyusb_init();
}

static void ttyusb_close(void)
{
	if(g_usb_serial.sfd > 0)
	{
		lib_close(g_usb_serial.sfd);
		g_usb_serial.sfd = -1;
	}

	if(g_wl_dev2_fd > 0)
	{
		lib_close(g_wl_dev2_fd);
		g_wl_dev2_fd = -1;
	}

	if(g_wl_dev3_fd > 0)
	{
		lib_close(g_wl_dev3_fd);
		g_wl_dev3_fd = -1;
	}
}

/*
 * 3G断电重启
 */
static void wl_3g_power_reset(void)
{
	SYS_LOG_DEBUG("3G power reset\n");

	int gpio_ctrl_fd = -1;
	int ret = -1;
	
	ttyusb_close();

	gpio_ctrl_fd = open("/dev/gpio_ctrl", O_RDWR);
	if(gpio_ctrl_fd < 0)
		return;
	
	lib_msleep(200);
	ret = ioctl(gpio_ctrl_fd, GPIO_CTRL_SET_PWR_3G, GPIO_CTRL_SW_OFF);
	SYS_LOG_DEBUG("------------power down 3g, ret:%d\n", ret);
	lib_msleep(300);
	ret = ioctl(gpio_ctrl_fd, GPIO_CTRL_SET_PWR_3G, GPIO_CTRL_SW_ON);
	SYS_LOG_DEBUG("------------power on 3g, ret:%d\n", ret);
	lib_msleep(300);

	if(gpio_ctrl_fd > 0)
		lib_close(gpio_ctrl_fd);
}


/* 
 * 软复位
 */
static void wl_soft_reset(void)
{
	fprintf(stderr, "wireless soft reset\n");

	dial_stat_set(WL_DIAL_STAT_SOFT_RESET);
		
	lib_sleep(5);

	ttyusb_init();

	lib_sleep(5);	
}

/*
 * 硬复位
 */
static void wl_hardware_reset(void)
{
	fprintf(stderr, "wireless hardware reset\n");

	dial_stat_set(WL_DIAL_STAT_HW_RESET);
}

static void wl_hardware_restart(void)
{
	fprintf(stderr, "wireless hardware restart\n");

	dial_stat_set(WL_DIAL_STAT_HW_RESTART);	
}

static void __terminal_no_file_get(char *terminal_no)
{
	FILE *fp = NULL;
	int s_len = 0;
	char s_terminal_no[64] = {0};

	fp = fopen(TERMINAL_NO_PATH, "rt");  /* 读文本 */
	if(fp != NULL)
	{
		if(fgets(s_terminal_no, sizeof(s_terminal_no), fp) != NULL)
		{
			fprintf(stderr, "Wireless Terminal NO get: %s\n", s_terminal_no);
			
			s_len = strlen(s_terminal_no);
			strcpy(terminal_no, s_terminal_no);
			terminal_no[s_len] = '\0';
		}
		else
			strcpy(terminal_no, "65535");
		
		fclose(fp);
	}
}


int main(int argc, char *argv[])
{
	int err = -1;
	pthread_t wl_thr, wl_pppd_thr;

	fprintf(stderr, "WIRELESS Software Compiled Time: %s, %s.\r\n",__DATE__, __TIME__);

	//daemon(0, 1);

	system("ulimit -s 6144");  //设置线程栈
	
	signals_init();
	
	/* 初始化syslog日志*/
#ifdef WL_LOG_RUN
	char log_ident[64] = {0};
	char terminal_no[32] = {0};
	char macaddr[8] = {0};
	char s_macaddr[16] = {0};
	
	lib_get_macaddr("eth1", macaddr);
	lib_hex_to_str((unsigned char *)macaddr, 6, (unsigned char *)s_macaddr);
	__terminal_no_file_get(terminal_no);
	
	sprintf(log_ident, "------[WIRELESS APP]-[%s]:[%s]", s_macaddr, terminal_no);
	
	fprintf(stderr, "WIRELESS log ident: %s\n", log_ident);
	
	openlog(log_ident, LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif	

	SYS_LOG_NOTICE( "WIRELESS App running, Software Compiled Time: %s, %s.\n", __DATE__, __TIME__);

	/* 创建事件*/
	g_eventloop = lib_event_loop_create(8);
	if(g_eventloop == LIB_EP_NULL)
		goto ERR;

	/* 创建unix服务端*/
	g_unix_sockfd = lib_unix_server_new(WL_UNIX_DOMAIN, WL_PERM, 8);
	if(g_unix_sockfd == LIB_GE_ERROR)
	{
		WL_ERR("unix server new failed!\n");
		goto ERR;
	}

	/* 创建unix读事件 */
	err = lib_event_loop_add(g_eventloop, g_unix_sockfd, LIB_EP_READ, unix_accept_proc, NULL, 0);
	if(err == LIB_EP_ERR)
	{
		WL_ERR("event loop add {unix sockfd} failed!\n");
		goto ERR;
	}

	g_rd_at_cnt = 0;
	ttyusb_init();  //wireless信息通道
	
	dial_stat_set(WL_DIAL_STAT_UNDIAL);

	wl_pppd_info_init();
	wl_csq_sysinfo_init();

	mc_ate0_set(g_usb_serial.sfd);

	time(&(g_wl_dial_time.last_dial_time)); //最后一次拨号时间
	time(&(g_wl_dial_time.last_discon_time)); //最后一次断线时间

	/* 创建拨号维护线程*/
	err = lib_normal_thread_create(&wl_thr, wl_handle_thread, NULL);
	if(err != LIB_GE_EOK)
		goto ERR;

	err = lib_normal_thread_create(&wl_pppd_thr, wl_kill_pppd_thread, NULL);
	if(err != LIB_GE_EOK)
		goto ERR;

	
	fprintf(stderr, "WIRELESS APP running\n");

	lib_event_loop(g_eventloop);

ERR:
	wl_csq_sysinfo_destroy();
	wl_pppd_info_destroy();
	lib_unix_close(g_unix_sockfd);
	ttyusb_close();
	lib_event_loop_destroy(g_eventloop);

#ifdef WL_LOG_RUN
	closelog();
#endif

	fprintf(stderr, "WIRELESS APP quit!\n");
		
	return 0;
}

/*
 * 检查拨号是否成功
 */
static unsigned int is_pppd_alive(void)
{
	int ret = -1;

	ret =  lib_get_network_flow("ppp0", &g_rx_bytes, &g_tx_bytes);
	if(ret == LIB_GE_EOK)
	{
		dial_stat_set(WL_DIAL_STAT_DIALOK);
		
		dial_count_zero();   //拨号成功，清0
		return WL_DIAL_STAT_DIALOK;
	}

	dial_stat_set(WL_DIAL_STAT_UNDIAL);
	
	return WL_DIAL_STAT_UNDIAL;
}

/*
 * 拨号程序
 */
static int pppd_dial(void)
{
	FILE *dialfp = NULL;
	char rxbuf[256] = {0};
	int res = -1;
	char *p1 = NULL;
	char *p2 = NULL;
	char *p3 = NULL;
	char *p4 = NULL;
	wl_pppd_info_t pppd_info;

	memset(&pppd_info, 0, sizeof(wl_pppd_info_t));
	
	system("killall -9 pppd"); //杀死pppd进程

	dialfp = popen("/mnt/firmware/etc/pppd call evdo &", "r");
	if(dialfp != NULL)
	{
		memset(rxbuf, 0, sizeof(rxbuf));
		while(fgets(rxbuf, sizeof(rxbuf) - 1, dialfp) != NULL)
		{
			WL_DBG("%s", rxbuf);
			//SYS_LOG_DEBUG("---------------rxbuf:%s\n", rxbuf);
			
			//failed
			if(strstr(rxbuf, "failed") != NULL)	
			{
				dial_stat_set(WL_DIAL_STAT_UNDIAL);
			
				WL_DBG("pppd dial [failed]!\n");
				
				res = WL_ERROR;
				break;
			}

			//using channel
			if((p1 = strstr(rxbuf, "using channel ")) != NULL)
			{
				p2 = p1 + strlen("using channel ");
				if(p2 != NULL)
					pppd_info.using_channel = atoi(p2);
			}

			//Using interface
			if((p1 = strstr(rxbuf, "Using interface")) != NULL)
			{
				p2 = p1 + strlen("Using interface ");
				if(p2 != NULL)
				{
					p4 = strtok_r(p2, "\n", &p3);
					if(p4 != NULL)
						strcpy(pppd_info.using_interface, p4);
				}
			}			

			//timeout sending Config-Requests
			if(strstr(rxbuf, "timeout sending Config-Requests") != NULL)	
			{
				dial_stat_set(WL_DIAL_STAT_UNDIAL);

				WL_DBG("pppd dial [timeout sending Config-Requests]!\n");
				
				res = WL_ERROR;
			}

			//Connection terminated
			if(strstr(rxbuf, "Connection terminated") != NULL)	
			{
				dial_stat_set(WL_DIAL_STAT_UNDIAL);

				WL_DBG("pppd dial [Connection terminated]!\n");
				
				res = WL_ERROR;
			}

			//zte ev exit
			if(strstr(rxbuf, "zte ev exit---") != NULL)	
			{
				dial_stat_set(WL_DIAL_STAT_UNDIAL);
				
				WL_DBG("pppd dial [zte ev exit]!\n");
				
				res = WL_ERROR;
			}

			//local  IP address
			if((p1 = strstr(rxbuf, "local  IP address")) != NULL)
			{
				dial_stat_set(WL_DIAL_STAT_DIALOK);
			
				p2 = p1 + strlen("local  IP address ");
				if(p2 != NULL)
				{
					p4 = strtok_r(p2, "\n", &p3);
					if(p4 != NULL)
						strcpy(pppd_info.local_ip_address, p4);
				}
					
				WL_DBG("pppd dial [local  IP address] success\n");
				
				res = WL_EOK;	
			}
			
			//remote IP address
			if((p1 = strstr(rxbuf, "remote IP address")) != NULL)
			{
				dial_stat_set(WL_DIAL_STAT_DIALOK);
				
				p2 = p1 + strlen("remote IP address ");
				if(p2 != NULL)
				{
					p4 = strtok_r(p2, "\n", &p3);
					if(p4 != NULL)
						strcpy(pppd_info.remote_ip_address, p4);
				}
					
				WL_DBG("pppd dial [remote IP address] success\n");
				
				res = WL_EOK;				
			}

			//primary   DNS address
			if((p1 = strstr(rxbuf, "primary   DNS address")) != NULL)
			{
				dial_stat_set(WL_DIAL_STAT_DIALOK);

				p2 = p1 + strlen("primary   DNS address ");			
				if(p2 != NULL)
				{
					p4 = strtok_r(p2, "\n", &p3);
					if(p4 != NULL)
						strcpy(pppd_info.primary_dns_address, p4);
				}
						
				WL_DBG("pppd dial [primary   DNS address] success\n");
				
				res = WL_EOK;	
			}			
			
			//secondary DNS address
			if((p1 = strstr(rxbuf, "secondary DNS address")) != NULL)
			{
				dial_stat_set(WL_DIAL_STAT_DIALOK);

				p2 = p1 + strlen("secondary DNS address ");
				if(p2 != NULL)
				{
					p4 = strtok_r(p2, "\n", &p3);
					if(p4 != NULL)
						strcpy(pppd_info.secondary_dns_address, p4);
				}
							
				WL_DBG("pppd dial [secondary DNS address] success\n");
				
				res = WL_EOK;	
				break;
			}
		}
	}

	wl_pppd_info_put(&pppd_info);

	if(dialfp)
		pclose(dialfp);

	if(pppd_info.using_channel == WL_USING_CHL_MAX) //防止using channel数多大
	{
		SYS_LOG_DEBUG("WL PPPD using channel is %d, MAX CHL %d\n", pppd_info.using_channel, WL_USING_CHL_MAX);
		wl_3g_power_reset();
	}
	
	return res;
}


/* 
 * 拨号失败处理 机制
 */
static void dial_fail_handle(const unsigned int dial_cnt)
{
	FILE *logfd = NULL;
	static int sim_reboots = 0, dial_reboots = 0; //由于检测不到SIM卡和拨号失败导致的系统重启次数 
	
	switch(dial_cnt)
	{
		//复位模块
		case WL_DIAL_MAX_LEVEL1:
		{
			SYS_LOG_DEBUG("WL dial fail handle LEVEL1 %d\n", WL_DIAL_MAX_LEVEL1);
			//dial_count_zero(); //复位和重启3G模块都不一定能拨号成功，所以不应该清零拨号次数 2016-11-29
			mc_module_reset(g_usb_serial.sfd); //add by zjc at 2016-11-24
		}
		break; 

		//重启模块
		case WL_DIAL_MAX_LEVEL2:
		{
			SYS_LOG_DEBUG("WL dial fail handle LEVEL2 %d\n", WL_DIAL_MAX_LEVEL2);
			wl_3g_power_reset();    
		}
		break;

		//重启设备
		case WL_DIAL_MAX_LEVEL3:
		{
			SYS_LOG_DEBUG("WL dial fail handle LEVEL3 %d\n", WL_DIAL_MAX_LEVEL3); 
			
			//读取识别不到SIM卡导致的系统重启次数，没有重启过才重启(SIM卡识别到的情况下才重启)
			pthread_mutex_lock(&g_reboot_mutex);
			logfd = fopen(SIM_REBOOT_PATH, "rb");
			if(NULL != logfd)
			{
				fread(&sim_reboots, sizeof(sim_reboots), 1, logfd);
				fclose(logfd);
			}
			pthread_mutex_unlock(&g_reboot_mutex);
			
			if(sim_reboots < 1) //SIM卡有检测到但拨号失败
			{
				//将系统重启次数记录下来, 防止SIM卡一直识别不到时节点机频繁重启
				pthread_mutex_lock(&g_reboot_mutex1);
				logfd = fopen(DIAL_REBOOT_PATH, "rb");
				if(NULL != logfd)
				{
					fread(&dial_reboots, sizeof(dial_reboots), 1, logfd);
					fclose(logfd);
				}
			
				logfd = fopen(DIAL_REBOOT_PATH, "wb");
				if(NULL != logfd)
				{
					dial_reboots++;
					
					fwrite(&dial_reboots, sizeof(dial_reboots), 1, logfd);
					fclose(logfd); 
				}		
				pthread_mutex_unlock(&g_reboot_mutex1);

				SYS_LOG_DEBUG("WL dial fail, dial_reboots:%d\n", dial_reboots);

				if(dial_reboots < 3) // <2 --> <3 2016-12-13
				{
					SYS_LOG_DEBUG("WL dial fail, dial_reboots:%d, reboot system!!!\n", dial_reboots);
					sleep(3);
					system("/mnt/firmware/reboot_wdt"); //add by zjc at 2016-11-24
				}
			}
		}
		break;
	}
}

/*
 * 无线管理线程
 */
static void *wl_handle_thread(void *arg)
{
	int ret = -1;
	int wait = 0;
	unsigned int dial_cnt = 0;
	unsigned int dial_stat = WL_DIAL_STAT_UNDIAL;
	wl_csq_t csq;
	wl_sysinfo_t sysinfo;
	wl_csq_sysinfo_t csq_sysinfo;
	static int ttyusb_err_cnt = 0;
	int dial_err_cnt = 0;
	static int gpio_ctrl_fd = -1;
	FILE *logfd = NULL;
	static int sys_reboots = 0, dail_reboots = 0, sim_reboots = 0; //系统重启次数	
	static int signal_service_abnormals = 0; //信号或者服务异常次数
	static int prefmode_set_flag = 0; 
	
	while(1)
	{
		//printf("-----------%s------------\n", __func__);
		
		dial_err_cnt = mc_dial_err_cnt_get();
		if(dial_err_cnt > WL_DIAL_MAX_LEVEL2) //拨号出错次数
		{
			fprintf(stderr, "Dial Err Count %d On %d\n", dial_err_cnt, WL_DIAL_MAX_LEVEL2);
			SYS_LOG_NOTICE("Dial Err Count %d On %d\n", dial_err_cnt, WL_DIAL_MAX_LEVEL2);
			
			dial_fail_handle(WL_DIAL_MAX_LEVEL2);
		}
		
		if(__network_model_get() == WL_NETWORK_MODEL_WIRED) //有线模式
		{
			wl_csq_sysinfo_clean();
			
			fprintf(stderr, "%s:Network Model Wired\n", __FUNCTION__);
			
			lib_sleep(WL_WIRED_SLEEP);
			continue;
		}

		memset(&csq, 0, sizeof(wl_csq_t));
		memset(&sysinfo, 0, sizeof(wl_sysinfo_t));
		memset(&csq_sysinfo, 0, sizeof(wl_csq_sysinfo_t));

		dial_stat = dial_stat_get();
		if(dial_stat != WL_DIAL_STAT_DIALOK) //处于没有拨号成功状态
		{
			if(g_usb_serial.sfd <= 0)
			{
				if(ttyusb_init() < 0)  //设备串口没有打开成功, 进行打开操作
				{
						
					lib_sleep(WL_DIAL_PPPD_SEC);
					continue;
				}
			}
			
			if((is_ttyusb0_exist() == WL_TRUE) && (is_ttyusb1_exist() == WL_TRUE)) //设备描述符存在情况下
			{
				g_ttyusb_exist = WL_TRUE;  //设备存在
				
				if((mc_sysinfo_get(g_usb_serial.sfd, &sysinfo) == MC_EOK) && (mc_csq_get(g_usb_serial.sfd, &csq) == MC_EOK))
				{
					WL_DBG("[SYSINFO] sim_state: %d\n", sysinfo.sim_state);
					WL_DBG("[CSQ] rssi: %d\n", csq.rssi);
	
					csq_sysinfo.srv_status = sysinfo.srv_status;
					csq_sysinfo.srv_domain = sysinfo.srv_domain;
					csq_sysinfo.roam_status = sysinfo.roam_status;
					csq_sysinfo.sys_mode = sysinfo.sys_mode;
					csq_sysinfo.sim_state = sysinfo.sim_state;
					csq_sysinfo.rssi = csq.rssi;
					csq_sysinfo.fer = csq.fer;	
					wl_csq_sysinfo_put(&csq_sysinfo);

					/*
					 * <RSSI>:0- 31 信号强度等级，31代表信号最强 
					 *	<FER>  :  数据帧出错比率 
					 *	0: less than 0.01% 
					 *	1: 0.01% to less than 0.1%  
					 *	2: 0.1% to less than 0.5%  
					 *	3: 0.5% to less than 1.0% 
					 *	4: 1.0% to less than 2.0% 
					 *	5: 2.0% to less than 4.0% 
					 *	6: 4.0% to less than 8.0% 
					 *	7: greater than 8.0% 
					 *	99: FER is unknown 
					 */

					/* UIM卡存在,信号强度等级大于WL_CSQ_MIN */
					if((sysinfo.sim_state == 1) && (csq.rssi > WL_CSQ_MIN))
					{
 						#if 1       
						/* 强制优先使用3g的网络模式 add by zjc at 2016-11-08 */
						//mc_prefmode_get(g_usb_serial.sfd);
						if(prefmode_set_flag == 0)   
						{
							mc_prefmode_set(g_usb_serial.sfd);
							
							prefmode_set_flag = 1;
						}
						//mc_prefmode_get(g_usb_serial.sfd);
						#endif
						   
						ret = pppd_dial();  //拨号，阻塞模式	
						if(ret == WL_EOK) //成功
						{
							time(&(g_wl_dial_time.last_dial_time));  //最后一次拨号时间
						}
						
						g_sim_state_cnt = 0;  //SIM计数清0

						#if 1 //识别到SIM卡后系统重启次数清零
						pthread_mutex_lock(&g_reboot_mutex);
						logfd = fopen(SIM_REBOOT_PATH, "wb");
						if(NULL != logfd)
						{
							sys_reboots = 0;
							fwrite(&sys_reboots, sizeof(sys_reboots), 1, logfd);
							fclose(logfd); 
						}	
						pthread_mutex_unlock(&g_reboot_mutex);
						#endif
					}
					else
					{
						if(sysinfo.sim_state != 1)  // UIM卡状态，1:有效 255: UIM卡不存在
						{
							fprintf(stderr, "WIRELESS WARN: 3g sim card is NOT exist!!!\n");

							g_sim_state_cnt++;
							SYS_LOG_DEBUG("WARN: sim_state: %d, 3g sim card is NOT exist %d times!!!", sysinfo.sim_state, g_sim_state_cnt);

							//检测到SIM卡不存在时的处理策略 add by zjc at 2016-11-28
						#if 1
							if(g_sim_state_cnt == 2) //1.软复位3G模块
							{
								mc_module_reset(g_usb_serial.sfd); 
								
								SYS_LOG_DEBUG("3g sim card is NOT exist %d times: soft reboot 3g module\n", g_sim_state_cnt);
							}
							else if(g_sim_state_cnt == 5) //2.硬重启3G模块
							{
								wl_3g_power_reset(); 
								
								SYS_LOG_DEBUG("3g sim card is NOT exist %d times: hard reboot 3g module\n", g_sim_state_cnt);
							}  
							else if(g_sim_state_cnt == 8) //3.复位USB HUB芯片
							{
								gpio_ctrl_fd = open("/dev/gpio_ctrl", O_RDWR);
								if(gpio_ctrl_fd >= 0)
								{
									ret = ioctl(gpio_ctrl_fd, GPIO_CTRL_SET_PWR_USB_HUB, GPIO_CTRL_SW_OFF);
									sleep(1);
									ret = ioctl(gpio_ctrl_fd, GPIO_CTRL_SET_PWR_USB_HUB, GPIO_CTRL_SW_ON);
							
									if(gpio_ctrl_fd >= 0)
										lib_close(gpio_ctrl_fd);
														
									SYS_LOG_DEBUG("3g sim card is NOT exist %d times: reset USB HUB\n", g_sim_state_cnt);
								}
							}
							else if(g_sim_state_cnt == 12) //4.重启节点机
							{
								pthread_mutex_lock(&g_reboot_mutex);
								logfd = fopen(SIM_REBOOT_PATH, "rb");
								if(NULL != logfd)
								{
									fread(&sys_reboots, sizeof(sys_reboots), 1, logfd);
									fclose(logfd);
								}
							
								logfd = fopen(SIM_REBOOT_PATH, "wb");
								if(NULL != logfd)
								{
									sys_reboots++;
									
									fwrite(&sys_reboots, sizeof(sys_reboots), 1, logfd);
									fclose(logfd); 
								}		
								pthread_mutex_unlock(&g_reboot_mutex);

								SYS_LOG_DEBUG("3g sim card is NOT exist %d times, sys_reboots:%d\n", g_sim_state_cnt, sys_reboots);
								
								if(sys_reboots < 2) 
								{
									SYS_LOG_DEBUG("3g sim card is NOT exist %d times: reboot system %d times!!!\n", g_sim_state_cnt, sys_reboots);
									sleep(3);
									
									system("/mnt/firmware/reboot_wdt");
								}
							}
						#endif
						}

						if((sysinfo.sim_state == 1) && (csq.rssi <= WL_CSQ_MIN))
						{
							fprintf(stderr, "WIRELESS WARN: RSSI: %d, 3G signal strength is not enough !!!\n", csq.rssi);

							SYS_LOG_DEBUG("WARN: RSSI: %d, FER: %d, 3G signal strength is not enough !!", csq.rssi, csq.fer);

							g_sim_state_cnt = 0; //add by zjc at 2016-11-28

							#if 1 //识别到SIM卡后系统重启次数清零
							pthread_mutex_lock(&g_reboot_mutex);
							logfd = fopen(SIM_REBOOT_PATH, "wb");
							if(NULL != logfd)
							{
								sys_reboots = 0;
								fwrite(&sys_reboots, sizeof(sys_reboots), 1, logfd);
								fclose(logfd); 
							}		
							pthread_mutex_unlock(&g_reboot_mutex);
							#endif
						}
							
						lib_sleep(WL_DIAL_PPPD_SEC);
						continue;
					}
				}
			}
			else
			{
				/* 因为如果使用有线模式,会不断输出ttyUSB错误 */
				
				__ttyusb_reinit();
				g_ttyusb_exist = WL_FALSE;  //设备不存在
				
				lib_sleep(WL_DIAL_PPPD_SEC);
				continue;
			}
		}
		
		/* 条件变量 */
		pthread_mutex_lock(&g_kill_pppd.mutex);
		wait = g_kill_pppd.wait;
		pthread_mutex_unlock(&g_kill_pppd.mutex);

		if(wait == 1)
			pthread_cond_signal(&g_kill_pppd.cond);
			
		if(is_pppd_alive() != WL_DIAL_STAT_DIALOK)  //检查ppp0是否存在,是否已经拨号
		{
			time(&(g_wl_dial_time.last_discon_time));   //最后一次断开时间
			
			wl_pppd_info_clean();
			dial_stat_set(WL_DIAL_STAT_UNDIAL); //未拨号
			
			if(g_ttyusb_exist == WL_TRUE) //设备存在
			{
				dial_count_inc();  //拨号次数
			}
			
			dial_cnt = dial_count_get();
			
			fprintf(stderr, "pppd dial: %d times again\n", dial_cnt);

			SYS_LOG_DEBUG("pppd dial: %d times again", dial_cnt);
			
			dial_fail_handle(dial_cnt); //拨号失败处理:1.复位模块 2.重启模块 3.重启系统
		}
		else
		{
			dial_count_zero(); //拨号成功后拨号次数清零 add by zjc at 2016-11-29

			logfd = fopen(DIAL_REBOOT_PATH, "wb"); //拨号失败导致的重启次数清零
			if(NULL != logfd)
			{
				dail_reboots = 0;
				
				fwrite(&dail_reboots, sizeof(dail_reboots), 1, logfd);
				fclose(logfd); 
			}		
		}

		memset(&csq, 0, sizeof(wl_csq_t));
		memset(&sysinfo, 0, sizeof(wl_sysinfo_t));
		memset(&csq_sysinfo, 0, sizeof(wl_csq_sysinfo_t));

		if((mc_sysinfo_get(g_usb_serial.sfd, &sysinfo) == MC_EOK) && (mc_csq_get(g_usb_serial.sfd, &csq) == MC_EOK))
		{
			csq_sysinfo.srv_status = sysinfo.srv_status;
			csq_sysinfo.srv_domain = sysinfo.srv_domain;
			csq_sysinfo.roam_status = sysinfo.roam_status;
			csq_sysinfo.sys_mode = sysinfo.sys_mode;
			csq_sysinfo.sim_state = sysinfo.sim_state;
			csq_sysinfo.rssi = csq.rssi;
			csq_sysinfo.fer = csq.fer;	

			ttyusb_err_cnt = 0;
			wl_csq_sysinfo_put(&csq_sysinfo);
		}
		else
		{
			ttyusb_err_cnt++;
			if(ttyusb_err_cnt >= 3)   //获取3G设备信息失败次数过多，重新初始化3G设备通信
			{
				__ttyusb_reinit();
				ttyusb_err_cnt = 0;
			}
		}

		if(dial_cnt <= 10)
			lib_sleep(WL_DIAL_PPPD_SEC);
		else
			lib_sleep(WL_DIAL_PPPD_SEC + dial_cnt); //2016-12-12
	}

	return lib_thread_exit((void *)NULL);
}

/*
 * pppd 维护线程
 */
static void *wl_kill_pppd_thread(void *arg)
{
	int err = -1;
	struct timespec tspec;
	wl_csq_sysinfo_t csq_sysinfo;
	static int dail_timeout_times = 0; //拨号超时计数
	static int dail_timeout_reboots = 0; //拨号超时导致重启节点机的次数
	FILE *logfd = NULL;
	
	while(1)
	{
		//printf("-----------%s------------\n", __func__);
		
		if(__network_model_get() == WL_NETWORK_MODEL_WIRED)
		{
			fprintf(stderr, "%s:Network Model Wired\n", __FUNCTION__);
			lib_sleep(WL_WIRED_SLEEP);
			continue;
		}
		
		memset(&tspec, 0, sizeof(struct timespec));
		tspec.tv_sec = time(NULL) + WL_KILL_PPPD_SEC;
		tspec.tv_nsec = WL_KILL_PPPD_SEC * 1000 * 1000 % 1000000;
	
		pthread_mutex_lock(&g_kill_pppd.mutex);

		if(g_kill_pppd.wait == 1)
		{
			err = pthread_cond_timedwait(&g_kill_pppd.cond, &g_kill_pppd.mutex, &tspec);
			if(err == ETIMEDOUT)  //超时
			{
				dail_timeout_times++; //拨号超时计数 2016-12-12
				
				dial_stat_set(WL_DIAL_STAT_UNDIAL);  //超时，没有拨号
				
				if(g_ttyusb_exist == WL_TRUE) //TTYUSB存在，才输出错误
				{
					fprintf(stderr, "WIRELESS WARN: pppd thread timedwait ETIMEDOUT!!!\n");
					
					SYS_LOG_DEBUG("WIRELESS WARN: pppd thread timedwait ETIMEDOUT");
				}

				memset(&csq_sysinfo, 0, sizeof(wl_csq_sysinfo_t));
				wl_csq_sysinfo_get(&csq_sysinfo);
  				if(csq_sysinfo.sim_state == 1)  //SIM卡存在
				{
					if(g_ttyusb_exist == WL_TRUE) 
					{
						SYS_LOG_DEBUG("killall pppd AND chat");
					}
					
					system("killall -9 pppd");  //阻塞
					system("killall -9 chat");  //阻塞

					#if 1 //拨号超时3次以上后重启节点机 2016-12-12
					if(dail_timeout_times > 3) 
					{
						//将系统重启次数记录下来, 防止4G卡欠费时节点机频繁重启
						pthread_mutex_lock(&g_reboot_mutex1);
						logfd = fopen(DIAL_REBOOT_PATH, "rb");
						if(NULL != logfd)
						{
							fread(&dail_timeout_reboots, sizeof(dail_timeout_reboots), 1, logfd);
							fclose(logfd);
						}
					
						logfd = fopen(DIAL_REBOOT_PATH, "wb");
						if(NULL != logfd)
						{
							dail_timeout_reboots++;
							
							fwrite(&dail_timeout_reboots, sizeof(dail_timeout_reboots), 1, logfd);
							fclose(logfd); 
						}		
						pthread_mutex_unlock(&g_reboot_mutex1);
						
						if(dail_timeout_reboots < 3) // <2 --> <3 2016-12-13
						{
							SYS_LOG_DEBUG("dail timeout %d times: reboot system %d times!!!\n", dail_timeout_times, dail_timeout_reboots);
							sleep(3);
							
							system("/mnt/firmware/reboot_wdt");
						}
					}
					#endif
				}
				
				pthread_mutex_unlock(&g_kill_pppd.mutex);
				
				g_kill_pppd.wait = 1;
				continue;
			}
			else
				dail_timeout_times = 0; //拨号成功返回后清零拨号超时计数
		}

		pthread_mutex_unlock(&g_kill_pppd.mutex);
		g_kill_pppd.wait = 1;
	}

	return lib_thread_exit((void *)NULL);
}

static void unix_accept_proc(lib_event_loop_t *ep, int fd, void *client_data, int mask)
{
	int cli_sockfd = -1;
	struct sockaddr_un cli_addr;
	
	memset(&cli_addr, 0, sizeof(struct sockaddr_un));

	while((cli_sockfd = lib_unix_accept(fd, &cli_addr) ) < 0)
	{
		if((errno == EAGAIN) || (errno == EINTR))
		{
			WL_ERR("unix accept continue!\n");
			continue;
		}
		else
		{
			WL_ERR("unix accept: %s\n", strerror(errno));
			return;
		}
	}

	lib_setsock_noblock(cli_sockfd);
	lib_event_loop_add(g_eventloop, cli_sockfd, LIB_EP_READ, unix_read_proc, NULL, 0);	
}

static void unix_read_proc(lib_event_loop_t *ep, int fd, void *client_data, int mask)
{
	int n = -1;
	unsigned char rxbuf[WL_PKG_LEN] = {0};
		
	while(1)
	{
		memset(rxbuf, 0, WL_PKG_LEN);
		n = read(fd, rxbuf, WL_PKG_LEN);
		if(n > 0)
		{
			/* 指令解释执行 */
			wl_unix_package_explain(fd, rxbuf, n);
			
			break;
		}
		else if(n == 0)
		{
			WL_ERR("unix read proc Quit!\n");

			SYS_LOG_DEBUG("wireless unix read proc Quit");
			
			lib_unix_close(fd);
			lib_event_loop_del(g_eventloop, fd, LIB_EP_READ);
			return;
		}
		else if(n < 0)
		{	
			if(errno == ETIMEDOUT) 
			{
				WL_ERR("unix read proc error: ETIMEDOUT!\n");

				lib_unix_close(fd);
				lib_event_loop_del(g_eventloop, fd, LIB_EP_READ);
				return;
			}

			WL_ERR("unix read proc error!\n");

			lib_unix_close(fd);
			lib_event_loop_del(g_eventloop, fd, LIB_EP_READ);
			return;
		}
	}
}

static int wl_unix_pkg(wl_unix_package_t *pkg, const unsigned int len)
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
			length = tbuf[i + 3];  //长度字
			memset(pkg, 0, sizeof(wl_unix_package_t));
			memcpy(pkg, &tbuf[i -1], length + WL_PKG_HEAD_LEN);

			return WL_EOK;
		}
	}

	return WL_ERROR;
}

static  int is_wl_unix_package(wl_unix_package_t *pkg)
{
	if((pkg->id_h == WL_ID_H) && (pkg->id_l == WL_ID_L))
		return WL_EOK;
	
	return WL_ERROR;
}

static  void wl_unix_package_create(wl_unix_package_t *pkg, const unsigned char cmd, const unsigned char result)
{
	memset(pkg, 0, sizeof(wl_unix_package_t));
	
	pkg->id_h = WL_ID_H;
	pkg->id_l = WL_ID_L;
	pkg->cmd = cmd;
	pkg->result = result;
}

static int wl_unix_package_explain(const int sockfd, void *ptr, const int len)
{
	int cpy_len = 0;
	wl_unix_package_t pkg;
	int pkg_sz = sizeof(struct wl_unix_package);
	unsigned char send_data_len = 0;
	
	if(len > pkg_sz)
		cpy_len = pkg_sz;
	else
		cpy_len = len;

	memcpy(&pkg, ptr, cpy_len);
	if(wl_unix_pkg(&pkg, cpy_len) != WL_EOK)
	{
		fprintf(stderr, "WL UNIX PKG error!\n");

		SYS_LOG_ERR("WL UNIX PKG errror!\n");

		pkg.length = send_data_len;
		pkg.result = WL_RESULT_PKG_ERR;
		goto Done;	
	}

	if(is_wl_unix_package(&pkg) != WL_EOK)
	{
		fprintf(stderr, "WL UNIX package error!\n");

		SYS_LOG_ERR("WL UNIX package error");
		
		pkg.length = send_data_len;
		pkg.result = WL_RESULT_PKG_ERR;
		goto Done;	
	}

	unsigned char cmd = pkg.cmd;
	switch(cmd)
	{
		/* 模块复位 */
		case WL_CMD_RESET_SET:
		{
			WL_DBG("WL_CMD_RESET_SET\n");

			wl_unix_package_create(&pkg, WL_CMD_RESET_SET, WL_RESULT_OK);

			wl_soft_reset(); //重新初始化串口，最终还是重启硬件
		}
		break;
		
		/* pppd拨号状态 */
		case WL_CMD_DIAL_STAT_GET:
		{
			WL_DBG("WL_CMD_DIAL_STAT_GET\n");
			
			unsigned int dial_stat = WL_DIAL_STAT_UNDIAL;
			
			send_data_len = sizeof(unsigned int); //长度
			wl_unix_package_create(&pkg, WL_CMD_DIAL_STAT_GET, WL_RESULT_OK);

			dial_stat = is_pppd_alive();
			
			dial_stat_set(dial_stat);

			pkg.length = send_data_len;
			memcpy(&(pkg.data), &dial_stat, sizeof(unsigned int));
		}
		break;

		/* 查询模块软件版本 */
		case WL_CMD_CGMR_GET:
		{
			WL_DBG("WL_CMD_CGMR_GET\n");

			wl_cgmr_t cgmr;
			memset(&cgmr, 0, sizeof(wl_cgmr_t));

			send_data_len = sizeof(wl_cgmr_t);
			wl_unix_package_create(&pkg, WL_CMD_CGMR_GET, WL_RESULT_OK);
			if(mc_cgmr_get(g_usb_serial.sfd, &cgmr) == MC_EOK)
			{
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_OK;
				memcpy(pkg.data, &cgmr, sizeof(wl_cgmr_t));
			}
			else
			{
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_ERR;
			}			
		}
		break;

		/* 查询硬件版本 */
		case WL_CMD_HWVER_GET:
		{
			WL_DBG("WL_CMD_HWVER_GET\n");

			wl_hwver_t hwver;
			memset(&hwver, 0, sizeof(wl_hwver_t));

			send_data_len = sizeof(wl_hwver_t);
			wl_unix_package_create(&pkg, WL_CMD_HWVER_GET, WL_RESULT_OK);
			if(mc_hwver_get(g_usb_serial.sfd, &hwver) == MC_EOK)
			{
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_OK;
				memcpy(pkg.data, &hwver, sizeof(wl_hwver_t));
			}
			else
			{
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_ERR;
			}			
		}
		break;

		/* 系统的信息查询 */
		case WL_CMD_SYSINFO_GET:
		{
			WL_DBG("WL_CMD_SYSINFO_GET\n");

			wl_sysinfo_t sysinfo;
			memset(&sysinfo, 0, sizeof(wl_sysinfo_t));

			send_data_len = sizeof(wl_sysinfo_t);
			wl_unix_package_create(&pkg, WL_CMD_SYSINFO_GET, WL_RESULT_OK);
			if(mc_sysinfo_get(g_usb_serial.sfd, &sysinfo) == MC_EOK)
			{
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_OK;
				memcpy(pkg.data, &sysinfo, sizeof(wl_sysinfo_t));
			}
			else
			{
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_ERR;
			}	
		}
		break;

		/* 信号强度查询 */
		case WL_CMD_CSQ_GET:
		{
			WL_DBG("WL_CMD_CSQ_GET\n");
			
			wl_csq_t csq;
			memset(&csq, 0, sizeof(wl_csq_t));
			
			send_data_len = sizeof(wl_csq_t);
			wl_unix_package_create(&pkg, WL_CMD_CSQ_GET, WL_RESULT_OK);
			if(__network_model_get() == WL_NETWORK_MODEL_WIRED)  //如果是有线模式
			{
				csq.fer = 0;
				csq.rssi = 0;
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_OK;
				memcpy(pkg.data, &csq, sizeof(wl_csq_t));
			}
			else  //无线模式
			{
				if(mc_csq_get(g_usb_serial.sfd, &csq) == MC_EOK)
				{
					pkg.length = send_data_len;
					pkg.result = WL_RESULT_OK;
					memcpy(pkg.data, &csq, sizeof(wl_csq_t));
				}
				else
				{
					pkg.length = send_data_len;
					pkg.result = WL_RESULT_ERR;
				}

				WL_DBG("CSQ FER:%d\n", csq.fer);
				WL_DBG("CSQ RSSI:%d\n", csq.rssi);
			}
		}
		break;

		/* PPPd拨号信息 */
		case WL_CMD_PPPD_INFO_GET:
		{
			WL_DBG("WL_CMD_PPPD_INFO_GET\n");
			
			wl_pppd_info_t pppd;
			memset(&pppd, 0, sizeof(wl_pppd_info_t));

			send_data_len = sizeof(wl_pppd_info_t);
			wl_unix_package_create(&pkg, WL_CMD_PPPD_INFO_GET, WL_RESULT_OK);
			wl_pppd_info_get(&pppd);

			pkg.length = send_data_len;
			pkg.result = WL_RESULT_OK;
			memcpy(pkg.data, &pppd, sizeof(wl_pppd_info_t));
		}
		break;

		case WL_CMD_PPPD_ONLINE:
		{
			WL_DBG("WL_CMD_PPPD_ONLINE\n");

			send_data_len = sizeof(unsigned int);
			pkg.length = send_data_len;
			wl_unix_package_create(&pkg, WL_CMD_PPPD_ONLINE, WL_RESULT_OK);
		}
		break;

		case WL_CMD_CSQ_SYSINFO:  //CSQ+SYSINFO
		{
			WL_DBG("WL_CMD_CSQ_SYSINFO\n");
			
			wl_csq_sysinfo_t csq_sysinfo;
			memset(&csq_sysinfo, 0, sizeof(wl_csq_sysinfo_t));
			
			send_data_len = sizeof(wl_csq_sysinfo_t);
			wl_unix_package_create(&pkg, WL_CMD_CSQ_SYSINFO, WL_RESULT_OK);
			wl_csq_sysinfo_get(&csq_sysinfo);
			
			pkg.length = send_data_len;
			pkg.result = WL_RESULT_OK;
			memcpy(pkg.data, &csq_sysinfo, sizeof(wl_csq_sysinfo_t));
		}
		break;

		case WL_CMD_CUR_FLOW_INFO: //获取流量信息
		{
			WL_DBG("WL_CMD_FLOW_INFO\n");
			
			wl_flow_info_t flow_info;
			memset(&flow_info, 0, sizeof(wl_flow_info_t));
			
			send_data_len = sizeof(wl_flow_info_t);
			wl_unix_package_create(&pkg, WL_CMD_CUR_FLOW_INFO, WL_RESULT_OK);
			
			flow_info.rx_bytes = g_rx_bytes;
			flow_info.tx_bytes = g_tx_bytes;
			
			pkg.length = send_data_len;
			pkg.result = WL_RESULT_OK;
			memcpy(pkg.data, &flow_info, sizeof(wl_flow_info_t));		
		}
		break;

		case WL_CMD_DIAL_TIME: //拨号时间
		{
			WL_DBG("WL_CMD_DIAL_TIME\n");

			send_data_len = sizeof(wl_dial_time_t);
			wl_unix_package_create(&pkg, WL_CMD_DIAL_TIME, WL_RESULT_OK);

			pkg.length = send_data_len;
			pkg.result = WL_RESULT_OK;
			memcpy(pkg.data, &g_wl_dial_time, sizeof(wl_dial_time_t));				
		}
		break;

		case WL_CMD_MODEL_SET: //网络模式
		{
			WL_DBG("WL_CMD_MODEL_SET\n");

			unsigned int model = 0;

			memcpy(&model, pkg.data, 4);

			fprintf(stderr, "Network Model: %d\n", model);

			send_data_len = 0;
			__network_model_set(model);  //设置网络模式

			wl_unix_package_create(&pkg, WL_CMD_MODEL_SET, WL_RESULT_OK);
			pkg.length = send_data_len;
			pkg.result = WL_RESULT_OK;
		}
		break;

		/* 3G硬件复位 */
		case WL_CMD_3G_HW_RESET: //3  
		{
			WL_DBG("WL_CMD_3G_HW_RESET\n");

			pkg.length = 0;

			dial_fail_handle(WL_DIAL_MAX_LEVEL2);
			
			wl_unix_package_create(&pkg, WL_CMD_3G_HW_RESET, WL_RESULT_OK);		
		}
		break;

		/* 3G软件复位 add by zjc at 2016-11-09 */
		case WL_CMD_3G_SOFT_RESET: //3  
		{
			WL_DBG("WL_CMD_3G_SOFT_RESET\n");

			pkg.length = 0;

			mc_module_reset(g_usb_serial.sfd);
			
			wl_unix_package_create(&pkg, WL_CMD_3G_SOFT_RESET, WL_RESULT_OK);		
		}
		break;
		
		default:
		{
			WL_DBG("NOT SUPPORT COMMAND!!\n");
			
			wl_unix_package_create(&pkg, cmd, WL_RESULT_NOT_CMD);
		}
	}

Done:
	return lib_tcp_writen(sockfd, &pkg, send_data_len + WL_PKG_HEAD_LEN);  //发送数据
}











