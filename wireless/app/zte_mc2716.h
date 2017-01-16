#ifndef __ZTE_MC2716_H__
#define __ZTE_MC2716_H__

#include "wldef.h"


#define MC_ERROR		-1
#define MC_EOK			0





int mc_dial_err_cnt_get(void);
int mc_at_set(const int fd);
int mc_ate0_set(const int fd);
int mc_reset_set(const int fd);
int mc_cgmr_get(const int fd, wl_cgmr_t *wl);
int mc_hwver_get(const int fd, wl_hwver_t *wl);

int mc_csq_get(const int fd, wl_csq_t *wl);
int mc_sysinfo_get(const int fd, wl_sysinfo_t *wl);
int mc_zps_get(const int fd, wl_zps_t *wl);

/* add by zjc */
int mc_prefmode_get(const int fd);
int mc_prefmode_set(const int fd);
int mc_module_reset(const int fd);





#endif


