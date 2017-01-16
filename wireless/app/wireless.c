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
#define CONFIG_NO_CARRIER_CHECK	0 //SIM��Ƿ�Ѽ��

#define	SIM_REBOOT_PATH		"/opt/logpath/sim_reboot_times.txt" //��¼��SIM��ʶ�𲻵���ϵͳ��������
#define	DIAL_REBOOT_PATH	"/opt/logpath/dail_reboot_times.txt" //��¼����ʧ�ܺ�ϵͳ��������
#define	TTYUSB1_REBOOT_PATH	"/opt/logpath/ttyusb1_reboot_times.txt" //��¼����ʧ�ܺ�ϵͳ��������

pthread_mutex_t g_reboot_mutex = PTHREAD_MUTEX_INITIALIZER; //ϵͳ������д���ʻ�����(SIM��ʶ�𲻵�)
pthread_mutex_t g_reboot_mutex1 = PTHREAD_MUTEX_INITIALIZER; //ϵͳ������д���ʻ�����(���ų�ʱ)
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



/* ��־���� */
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
 * ���������붨��
 */
#define WL_NULL					(NULL) 
#define WL_EOK					(0)  //����
#define WL_ERROR				(-1) //����
#define WL_ETIMEOUT				(-2) //��ʱ
#define WL_EFULL					(-3) //��
#define WL_EEMPTY				(-4) //��
#define WL_ENOMEM 				(-5) //�ڴ治��
#define WL_EXCMEM				(-6) //�ڴ�Խ��
#define WL_EBUSY				(-7) //æ
#define WL_ERR_COMMAND			(-8) //��֧�ָ�ָ��

#define WL_ERR_NO_CARRIER			(-9) //NO CARRIER


#define WL_TRUE					(1)
#define WL_FALSE					(0)


/*
 * ����״̬
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
 * ����ģʽ
 */
enum WL_NETWORK_MODEL
{
	WL_NETWORK_MODEL_WIRED = 1,
	WL_NETWORK_MODEL_WIRELESS,
};

/*
 * ����ָ��
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
#define WL_CMD_FN_CB					0x14   //�ص�
#define WL_CMD_MODEL_SET				0x15
#define WL_CMD_3G_HW_RESET			0x18
#define WL_CMD_3G_SOFT_RESET			0x19

/*
 * ���Ľṹ
 */
#define WL_PKG_HEAD_LEN		5
#define WL_PKG_DATA_LEN		320
#define WL_PKG_LEN				(WL_PKG_HEAD_LEN + WL_PKG_DATA_LEN)

struct wl_unix_package
{
	unsigned char id_h;		//�ؼ���
	unsigned char id_l;
	unsigned char cmd;		//������
	unsigned char result;	//���
	unsigned char length;	//����
	unsigned char data[WL_PKG_DATA_LEN];
}__attribute__((packed));
typedef struct wl_unix_package wl_unix_package_t;

#define WL_ID_H			0x55
#define WL_ID_L			0xaa


/* Э�鷵�ؽ�� */
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

static int g_sim_state_cnt = 0; //SIM��״̬����

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
 * SIGHUP 1 A �ն˹�����߿��ƽ�����ֹ 
 * SIGINT 2 A �����жϣ���break�������£� 
 * SIGQUIT 3 C ���̵��˳��������� 
 * SIGILL 4 C �Ƿ�ָ�� 
 * SIGABRT 6 C ��abort(3)�������˳�ָ�� 
 * SIGFPE 8 C �����쳣 
 * SIGKILL 9 AEF Kill�ź� 
 * SIGSEGV 11 C ��Ч���ڴ����� 
 * SIGPIPE 13 A �ܵ�����: дһ��û�ж��˿ڵĹܵ� 
 * SIGALRM 14 A ��alarm(2)�������ź� 
 * SIGTERM 15 A ��ֹ�ź� 
 * SIGUSR1 30,10,16 A �û��Զ����ź�1 
 * SIGUSR2 31,12,17 A �û��Զ����ź�2 
 * SIGCHLD 20,17,18 B �ӽ��̽����ź� 
 * SIGCONT 19,18,25 ���̼���������ֹͣ�Ľ��̣� 
 * SIGSTOP 17,19,23 DEF ��ֹ���� 
 * SIGTSTP 18,20,24 D �����նˣ�tty���ϰ���ֹͣ�� 
 * SIGTTIN 21,21,26 D ��̨������ͼ�ӿ����ն˶� 
 * SIGTTOU 22,22,27 D ��̨������ͼ�ӿ����ն�д 
 */
static void sigint(int sig)
{
	fprintf(stderr, "wireless signal: %d\n", sig);
}

static void signals_init(void)
{
	struct sigaction sa;

	signal(SIGPIPE, SIG_IGN); //�ܵ��ر�
	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	
	signal(SIGTTOU, SIG_IGN); //��̨����д�����ն�
	signal(SIGTTIN, SIG_IGN); //��̨���̶������ն�
	signal(SIGTSTP, SIG_IGN); //�ն˹���

	
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

/* TTY USB ��ʼ�� */
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

	if(is_ttyusb1_exist())  //���ttyusb1�豸�������Ƿ����
	{
		logfd = fopen(TTYUSB1_REBOOT_PATH, "wb"); //����ʧ�ܵ��µ�������������
		if(NULL != logfd)
		{
			sys_reboots = 0;
			
			fwrite(&sys_reboots, sizeof(sys_reboots), 1, logfd);
			fclose(logfd); 
		}		
			
		memset(&g_usb_serial, 0, sizeof(lib_serial_t));
		
		/* USB���ڳ�ʼ��*/
		strcpy(g_usb_serial.pathname, WL_DEV3G_PATH);
		g_usb_serial.flags = O_RDWR;
		g_usb_serial.speed = 115200;
		g_usb_serial.databits = 8;
		g_usb_serial.stopbits = 1;
		
		err = lib_serial_init(&g_usb_serial);
		if(err == LIB_GE_ERROR)
		{
			g_rd_at_cnt++;  //�����ʼ������ʧ�ܴ���
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

		/* 3G�豸�� */
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

			//ttyusb1������ʱ�ȸ�λHUBоƬ�����ǲ����������ڵ�� add by zjc at 2016-11-24
			if(reboot_flag == 0) //��λUSB HUBоƬ
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
			else if(reboot_flag == 1) //�����ڵ��
			{
				//��ϵͳ����������¼����, ��ֹttyusb1һֱ������ʱ�ڵ��Ƶ������
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

	g_rd_at_cnt = 0; //������0

	reboot_flag = 0;
	
	lib_setfd_noblock(g_usb_serial.sfd);

	fprintf(stderr, "WIRLESS APP usb serial init succes, usb serial fd  = %d\n", g_usb_serial.sfd );

	return WL_EOK;
}

/* ���³�ʼ������ */
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
 * 3G�ϵ�����
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
 * ��λ
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
 * Ӳ��λ
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

	fp = fopen(TERMINAL_NO_PATH, "rt");  /* ���ı� */
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

	system("ulimit -s 6144");  //�����߳�ջ
	
	signals_init();
	
	/* ��ʼ��syslog��־*/
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

	/* �����¼�*/
	g_eventloop = lib_event_loop_create(8);
	if(g_eventloop == LIB_EP_NULL)
		goto ERR;

	/* ����unix�����*/
	g_unix_sockfd = lib_unix_server_new(WL_UNIX_DOMAIN, WL_PERM, 8);
	if(g_unix_sockfd == LIB_GE_ERROR)
	{
		WL_ERR("unix server new failed!\n");
		goto ERR;
	}

	/* ����unix���¼� */
	err = lib_event_loop_add(g_eventloop, g_unix_sockfd, LIB_EP_READ, unix_accept_proc, NULL, 0);
	if(err == LIB_EP_ERR)
	{
		WL_ERR("event loop add {unix sockfd} failed!\n");
		goto ERR;
	}

	g_rd_at_cnt = 0;
	ttyusb_init();  //wireless��Ϣͨ��
	
	dial_stat_set(WL_DIAL_STAT_UNDIAL);

	wl_pppd_info_init();
	wl_csq_sysinfo_init();

	mc_ate0_set(g_usb_serial.sfd);

	time(&(g_wl_dial_time.last_dial_time)); //���һ�β���ʱ��
	time(&(g_wl_dial_time.last_discon_time)); //���һ�ζ���ʱ��

	/* ��������ά���߳�*/
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
 * ��鲦���Ƿ�ɹ�
 */
static unsigned int is_pppd_alive(void)
{
	int ret = -1;

	ret =  lib_get_network_flow("ppp0", &g_rx_bytes, &g_tx_bytes);
	if(ret == LIB_GE_EOK)
	{
		dial_stat_set(WL_DIAL_STAT_DIALOK);
		
		dial_count_zero();   //���ųɹ�����0
		return WL_DIAL_STAT_DIALOK;
	}

	dial_stat_set(WL_DIAL_STAT_UNDIAL);
	
	return WL_DIAL_STAT_UNDIAL;
}

/*
 * ���ų���
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
	
	system("killall -9 pppd"); //ɱ��pppd����

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

	if(pppd_info.using_channel == WL_USING_CHL_MAX) //��ֹusing channel�����
	{
		SYS_LOG_DEBUG("WL PPPD using channel is %d, MAX CHL %d\n", pppd_info.using_channel, WL_USING_CHL_MAX);
		wl_3g_power_reset();
	}
	
	return res;
}


/* 
 * ����ʧ�ܴ��� ����
 */
static void dial_fail_handle(const unsigned int dial_cnt)
{
	FILE *logfd = NULL;
	static int sim_reboots = 0, dial_reboots = 0; //���ڼ�ⲻ��SIM���Ͳ���ʧ�ܵ��µ�ϵͳ�������� 
	
	switch(dial_cnt)
	{
		//��λģ��
		case WL_DIAL_MAX_LEVEL1:
		{
			SYS_LOG_DEBUG("WL dial fail handle LEVEL1 %d\n", WL_DIAL_MAX_LEVEL1);
			//dial_count_zero(); //��λ������3Gģ�鶼��һ���ܲ��ųɹ������Բ�Ӧ�����㲦�Ŵ��� 2016-11-29
			mc_module_reset(g_usb_serial.sfd); //add by zjc at 2016-11-24
		}
		break; 

		//����ģ��
		case WL_DIAL_MAX_LEVEL2:
		{
			SYS_LOG_DEBUG("WL dial fail handle LEVEL2 %d\n", WL_DIAL_MAX_LEVEL2);
			wl_3g_power_reset();    
		}
		break;

		//�����豸
		case WL_DIAL_MAX_LEVEL3:
		{
			SYS_LOG_DEBUG("WL dial fail handle LEVEL3 %d\n", WL_DIAL_MAX_LEVEL3); 
			
			//��ȡʶ�𲻵�SIM�����µ�ϵͳ����������û��������������(SIM��ʶ�𵽵�����²�����)
			pthread_mutex_lock(&g_reboot_mutex);
			logfd = fopen(SIM_REBOOT_PATH, "rb");
			if(NULL != logfd)
			{
				fread(&sim_reboots, sizeof(sim_reboots), 1, logfd);
				fclose(logfd);
			}
			pthread_mutex_unlock(&g_reboot_mutex);
			
			if(sim_reboots < 1) //SIM���м�⵽������ʧ��
			{
				//��ϵͳ����������¼����, ��ֹSIM��һֱʶ�𲻵�ʱ�ڵ��Ƶ������
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
 * ���߹����߳�
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
	static int sys_reboots = 0, dail_reboots = 0, sim_reboots = 0; //ϵͳ��������	
	static int signal_service_abnormals = 0; //�źŻ��߷����쳣����
	static int prefmode_set_flag = 0; 
	
	while(1)
	{
		//printf("-----------%s------------\n", __func__);
		
		dial_err_cnt = mc_dial_err_cnt_get();
		if(dial_err_cnt > WL_DIAL_MAX_LEVEL2) //���ų������
		{
			fprintf(stderr, "Dial Err Count %d On %d\n", dial_err_cnt, WL_DIAL_MAX_LEVEL2);
			SYS_LOG_NOTICE("Dial Err Count %d On %d\n", dial_err_cnt, WL_DIAL_MAX_LEVEL2);
			
			dial_fail_handle(WL_DIAL_MAX_LEVEL2);
		}
		
		if(__network_model_get() == WL_NETWORK_MODEL_WIRED) //����ģʽ
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
		if(dial_stat != WL_DIAL_STAT_DIALOK) //����û�в��ųɹ�״̬
		{
			if(g_usb_serial.sfd <= 0)
			{
				if(ttyusb_init() < 0)  //�豸����û�д򿪳ɹ�, ���д򿪲���
				{
						
					lib_sleep(WL_DIAL_PPPD_SEC);
					continue;
				}
			}
			
			if((is_ttyusb0_exist() == WL_TRUE) && (is_ttyusb1_exist() == WL_TRUE)) //�豸���������������
			{
				g_ttyusb_exist = WL_TRUE;  //�豸����
				
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
					 * <RSSI>:0- 31 �ź�ǿ�ȵȼ���31�����ź���ǿ 
					 *	<FER>  :  ����֡������� 
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

					/* UIM������,�ź�ǿ�ȵȼ�����WL_CSQ_MIN */
					if((sysinfo.sim_state == 1) && (csq.rssi > WL_CSQ_MIN))
					{
 						#if 1       
						/* ǿ������ʹ��3g������ģʽ add by zjc at 2016-11-08 */
						//mc_prefmode_get(g_usb_serial.sfd);
						if(prefmode_set_flag == 0)   
						{
							mc_prefmode_set(g_usb_serial.sfd);
							
							prefmode_set_flag = 1;
						}
						//mc_prefmode_get(g_usb_serial.sfd);
						#endif
						   
						ret = pppd_dial();  //���ţ�����ģʽ	
						if(ret == WL_EOK) //�ɹ�
						{
							time(&(g_wl_dial_time.last_dial_time));  //���һ�β���ʱ��
						}
						
						g_sim_state_cnt = 0;  //SIM������0

						#if 1 //ʶ��SIM����ϵͳ������������
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
						if(sysinfo.sim_state != 1)  // UIM��״̬��1:��Ч 255: UIM��������
						{
							fprintf(stderr, "WIRELESS WARN: 3g sim card is NOT exist!!!\n");

							g_sim_state_cnt++;
							SYS_LOG_DEBUG("WARN: sim_state: %d, 3g sim card is NOT exist %d times!!!", sysinfo.sim_state, g_sim_state_cnt);

							//��⵽SIM��������ʱ�Ĵ������ add by zjc at 2016-11-28
						#if 1
							if(g_sim_state_cnt == 2) //1.��λ3Gģ��
							{
								mc_module_reset(g_usb_serial.sfd); 
								
								SYS_LOG_DEBUG("3g sim card is NOT exist %d times: soft reboot 3g module\n", g_sim_state_cnt);
							}
							else if(g_sim_state_cnt == 5) //2.Ӳ����3Gģ��
							{
								wl_3g_power_reset(); 
								
								SYS_LOG_DEBUG("3g sim card is NOT exist %d times: hard reboot 3g module\n", g_sim_state_cnt);
							}  
							else if(g_sim_state_cnt == 8) //3.��λUSB HUBоƬ
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
							else if(g_sim_state_cnt == 12) //4.�����ڵ��
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

							#if 1 //ʶ��SIM����ϵͳ������������
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
				/* ��Ϊ���ʹ������ģʽ,�᲻�����ttyUSB���� */
				
				__ttyusb_reinit();
				g_ttyusb_exist = WL_FALSE;  //�豸������
				
				lib_sleep(WL_DIAL_PPPD_SEC);
				continue;
			}
		}
		
		/* �������� */
		pthread_mutex_lock(&g_kill_pppd.mutex);
		wait = g_kill_pppd.wait;
		pthread_mutex_unlock(&g_kill_pppd.mutex);

		if(wait == 1)
			pthread_cond_signal(&g_kill_pppd.cond);
			
		if(is_pppd_alive() != WL_DIAL_STAT_DIALOK)  //���ppp0�Ƿ����,�Ƿ��Ѿ�����
		{
			time(&(g_wl_dial_time.last_discon_time));   //���һ�ζϿ�ʱ��
			
			wl_pppd_info_clean();
			dial_stat_set(WL_DIAL_STAT_UNDIAL); //δ����
			
			if(g_ttyusb_exist == WL_TRUE) //�豸����
			{
				dial_count_inc();  //���Ŵ���
			}
			
			dial_cnt = dial_count_get();
			
			fprintf(stderr, "pppd dial: %d times again\n", dial_cnt);

			SYS_LOG_DEBUG("pppd dial: %d times again", dial_cnt);
			
			dial_fail_handle(dial_cnt); //����ʧ�ܴ���:1.��λģ�� 2.����ģ�� 3.����ϵͳ
		}
		else
		{
			dial_count_zero(); //���ųɹ��󲦺Ŵ������� add by zjc at 2016-11-29

			logfd = fopen(DIAL_REBOOT_PATH, "wb"); //����ʧ�ܵ��µ�������������
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
			if(ttyusb_err_cnt >= 3)   //��ȡ3G�豸��Ϣʧ�ܴ������࣬���³�ʼ��3G�豸ͨ��
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
 * pppd ά���߳�
 */
static void *wl_kill_pppd_thread(void *arg)
{
	int err = -1;
	struct timespec tspec;
	wl_csq_sysinfo_t csq_sysinfo;
	static int dail_timeout_times = 0; //���ų�ʱ����
	static int dail_timeout_reboots = 0; //���ų�ʱ���������ڵ���Ĵ���
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
			if(err == ETIMEDOUT)  //��ʱ
			{
				dail_timeout_times++; //���ų�ʱ���� 2016-12-12
				
				dial_stat_set(WL_DIAL_STAT_UNDIAL);  //��ʱ��û�в���
				
				if(g_ttyusb_exist == WL_TRUE) //TTYUSB���ڣ����������
				{
					fprintf(stderr, "WIRELESS WARN: pppd thread timedwait ETIMEDOUT!!!\n");
					
					SYS_LOG_DEBUG("WIRELESS WARN: pppd thread timedwait ETIMEDOUT");
				}

				memset(&csq_sysinfo, 0, sizeof(wl_csq_sysinfo_t));
				wl_csq_sysinfo_get(&csq_sysinfo);
  				if(csq_sysinfo.sim_state == 1)  //SIM������
				{
					if(g_ttyusb_exist == WL_TRUE) 
					{
						SYS_LOG_DEBUG("killall pppd AND chat");
					}
					
					system("killall -9 pppd");  //����
					system("killall -9 chat");  //����

					#if 1 //���ų�ʱ3�����Ϻ������ڵ�� 2016-12-12
					if(dail_timeout_times > 3) 
					{
						//��ϵͳ����������¼����, ��ֹ4G��Ƿ��ʱ�ڵ��Ƶ������
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
				dail_timeout_times = 0; //���ųɹ����غ����㲦�ų�ʱ����
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
			/* ָ�����ִ�� */
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
			length = tbuf[i + 3];  //������
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
		/* ģ�鸴λ */
		case WL_CMD_RESET_SET:
		{
			WL_DBG("WL_CMD_RESET_SET\n");

			wl_unix_package_create(&pkg, WL_CMD_RESET_SET, WL_RESULT_OK);

			wl_soft_reset(); //���³�ʼ�����ڣ����ջ�������Ӳ��
		}
		break;
		
		/* pppd����״̬ */
		case WL_CMD_DIAL_STAT_GET:
		{
			WL_DBG("WL_CMD_DIAL_STAT_GET\n");
			
			unsigned int dial_stat = WL_DIAL_STAT_UNDIAL;
			
			send_data_len = sizeof(unsigned int); //����
			wl_unix_package_create(&pkg, WL_CMD_DIAL_STAT_GET, WL_RESULT_OK);

			dial_stat = is_pppd_alive();
			
			dial_stat_set(dial_stat);

			pkg.length = send_data_len;
			memcpy(&(pkg.data), &dial_stat, sizeof(unsigned int));
		}
		break;

		/* ��ѯģ������汾 */
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

		/* ��ѯӲ���汾 */
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

		/* ϵͳ����Ϣ��ѯ */
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

		/* �ź�ǿ�Ȳ�ѯ */
		case WL_CMD_CSQ_GET:
		{
			WL_DBG("WL_CMD_CSQ_GET\n");
			
			wl_csq_t csq;
			memset(&csq, 0, sizeof(wl_csq_t));
			
			send_data_len = sizeof(wl_csq_t);
			wl_unix_package_create(&pkg, WL_CMD_CSQ_GET, WL_RESULT_OK);
			if(__network_model_get() == WL_NETWORK_MODEL_WIRED)  //���������ģʽ
			{
				csq.fer = 0;
				csq.rssi = 0;
				pkg.length = send_data_len;
				pkg.result = WL_RESULT_OK;
				memcpy(pkg.data, &csq, sizeof(wl_csq_t));
			}
			else  //����ģʽ
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

		/* PPPd������Ϣ */
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

		case WL_CMD_CUR_FLOW_INFO: //��ȡ������Ϣ
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

		case WL_CMD_DIAL_TIME: //����ʱ��
		{
			WL_DBG("WL_CMD_DIAL_TIME\n");

			send_data_len = sizeof(wl_dial_time_t);
			wl_unix_package_create(&pkg, WL_CMD_DIAL_TIME, WL_RESULT_OK);

			pkg.length = send_data_len;
			pkg.result = WL_RESULT_OK;
			memcpy(pkg.data, &g_wl_dial_time, sizeof(wl_dial_time_t));				
		}
		break;

		case WL_CMD_MODEL_SET: //����ģʽ
		{
			WL_DBG("WL_CMD_MODEL_SET\n");

			unsigned int model = 0;

			memcpy(&model, pkg.data, 4);

			fprintf(stderr, "Network Model: %d\n", model);

			send_data_len = 0;
			__network_model_set(model);  //��������ģʽ

			wl_unix_package_create(&pkg, WL_CMD_MODEL_SET, WL_RESULT_OK);
			pkg.length = send_data_len;
			pkg.result = WL_RESULT_OK;
		}
		break;

		/* 3GӲ����λ */
		case WL_CMD_3G_HW_RESET: //3  
		{
			WL_DBG("WL_CMD_3G_HW_RESET\n");

			pkg.length = 0;

			dial_fail_handle(WL_DIAL_MAX_LEVEL2);
			
			wl_unix_package_create(&pkg, WL_CMD_3G_HW_RESET, WL_RESULT_OK);		
		}
		break;

		/* 3G�����λ add by zjc at 2016-11-09 */
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
	return lib_tcp_writen(sockfd, &pkg, send_data_len + WL_PKG_HEAD_LEN);  //��������
}











