/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $DragonFly: src/sys/emulation/ibcs2/i386/Attic/ibcs2_proto.h,v 1.6 2003/07/30 00:19:13 dillon Exp $
 * created from DragonFly: src/sys/i386/ibcs2/syscalls.master,v 1.2 2003/06/17 04:28:35 dillon Exp 
 */

#ifndef _IBCS2_SYSPROTO_H_
#define	_IBCS2_SYSPROTO_H_

#include <sys/signal.h>

#include <sys/acl.h>

#include <sys/msgport.h>

#include <sys/sysmsg.h>

#define	PAD_(t)	(sizeof(register_t) <= sizeof(t) ? \
		0 : sizeof(register_t) - sizeof(t))

struct	ibcs2_read_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	char *	buf;	char buf_[PAD_(char *)];
	u_int	nbytes;	char nbytes_[PAD_(u_int)];
};
struct	ibcs2_open_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	int	flags;	char flags_[PAD_(int)];
	int	mode;	char mode_[PAD_(int)];
};
struct	ibcs2_wait_args {
	union sysmsg sysmsg;
	int	a1;	char a1_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
};
struct	ibcs2_creat_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	int	mode;	char mode_[PAD_(int)];
};
struct	ibcs2_unlink_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
};
struct	ibcs2_execv_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	char **	argp;	char argp_[PAD_(char **)];
};
struct	ibcs2_chdir_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
};
struct	ibcs2_time_args {
	union sysmsg sysmsg;
	ibcs2_time_t *	tp;	char tp_[PAD_(ibcs2_time_t *)];
};
struct	ibcs2_mknod_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	int	mode;	char mode_[PAD_(int)];
	int	dev;	char dev_[PAD_(int)];
};
struct	ibcs2_chmod_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	int	mode;	char mode_[PAD_(int)];
};
struct	ibcs2_chown_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	int	uid;	char uid_[PAD_(int)];
	int	gid;	char gid_[PAD_(int)];
};
struct	ibcs2_stat_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	struct ibcs2_stat *	st;	char st_[PAD_(struct ibcs2_stat *)];
};
struct	ibcs2_lseek_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	long	offset;	char offset_[PAD_(long)];
	int	whence;	char whence_[PAD_(int)];
};
struct	ibcs2_mount_args {
	union sysmsg sysmsg;
	char *	special;	char special_[PAD_(char *)];
	char *	dir;	char dir_[PAD_(char *)];
	int	flags;	char flags_[PAD_(int)];
	int	fstype;	char fstype_[PAD_(int)];
	char *	data;	char data_[PAD_(char *)];
	int	len;	char len_[PAD_(int)];
};
struct	ibcs2_umount_args {
	union sysmsg sysmsg;
	char *	name;	char name_[PAD_(char *)];
};
struct	ibcs2_setuid_args {
	union sysmsg sysmsg;
	int	uid;	char uid_[PAD_(int)];
};
struct	ibcs2_stime_args {
	union sysmsg sysmsg;
	long *	timep;	char timep_[PAD_(long *)];
};
struct	ibcs2_alarm_args {
	union sysmsg sysmsg;
	unsigned	sec;	char sec_[PAD_(unsigned)];
};
struct	ibcs2_fstat_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	struct ibcs2_stat *	st;	char st_[PAD_(struct ibcs2_stat *)];
};
struct	ibcs2_pause_args {
	union sysmsg sysmsg;
	register_t dummy;
};
struct	ibcs2_utime_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	struct ibcs2_utimbuf *	buf;	char buf_[PAD_(struct ibcs2_utimbuf *)];
};
struct	ibcs2_stty_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	struct sgttyb *	buf;	char buf_[PAD_(struct sgttyb *)];
};
struct	ibcs2_gtty_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	struct sgttyb *	buf;	char buf_[PAD_(struct sgttyb *)];
};
struct	ibcs2_access_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	int	flags;	char flags_[PAD_(int)];
};
struct	ibcs2_nice_args {
	union sysmsg sysmsg;
	int	incr;	char incr_[PAD_(int)];
};
struct	ibcs2_statfs_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	struct ibcs2_statfs *	buf;	char buf_[PAD_(struct ibcs2_statfs *)];
	int	len;	char len_[PAD_(int)];
	int	fstype;	char fstype_[PAD_(int)];
};
struct	ibcs2_kill_args {
	union sysmsg sysmsg;
	int	pid;	char pid_[PAD_(int)];
	int	signo;	char signo_[PAD_(int)];
};
struct	ibcs2_fstatfs_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	struct ibcs2_statfs *	buf;	char buf_[PAD_(struct ibcs2_statfs *)];
	int	len;	char len_[PAD_(int)];
	int	fstype;	char fstype_[PAD_(int)];
};
struct	ibcs2_pgrpsys_args {
	union sysmsg sysmsg;
	int	type;	char type_[PAD_(int)];
	caddr_t	dummy;	char dummy_[PAD_(caddr_t)];
	int	pid;	char pid_[PAD_(int)];
	int	pgid;	char pgid_[PAD_(int)];
};
struct	ibcs2_xenix_args {
	union sysmsg sysmsg;
	int	a1;	char a1_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
	int	a4;	char a4_[PAD_(int)];
	int	a5;	char a5_[PAD_(int)];
};
struct	ibcs2_times_args {
	union sysmsg sysmsg;
	struct tms *	tp;	char tp_[PAD_(struct tms *)];
};
struct	ibcs2_plock_args {
	union sysmsg sysmsg;
	int	cmd;	char cmd_[PAD_(int)];
};
struct	ibcs2_setgid_args {
	union sysmsg sysmsg;
	int	gid;	char gid_[PAD_(int)];
};
struct	ibcs2_sigsys_args {
	union sysmsg sysmsg;
	int	sig;	char sig_[PAD_(int)];
	ibcs2_sig_t	fp;	char fp_[PAD_(ibcs2_sig_t)];
};
struct	ibcs2_msgsys_args {
	union sysmsg sysmsg;
	int	which;	char which_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
	int	a4;	char a4_[PAD_(int)];
	int	a5;	char a5_[PAD_(int)];
	int	a6;	char a6_[PAD_(int)];
};
struct	ibcs2_sysi86_args {
	union sysmsg sysmsg;
	int	cmd;	char cmd_[PAD_(int)];
	int *	arg;	char arg_[PAD_(int *)];
};
struct	ibcs2_shmsys_args {
	union sysmsg sysmsg;
	int	which;	char which_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
	int	a4;	char a4_[PAD_(int)];
};
struct	ibcs2_semsys_args {
	union sysmsg sysmsg;
	int	which;	char which_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
	int	a4;	char a4_[PAD_(int)];
	int	a5;	char a5_[PAD_(int)];
};
struct	ibcs2_ioctl_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	int	cmd;	char cmd_[PAD_(int)];
	caddr_t	data;	char data_[PAD_(caddr_t)];
};
struct	ibcs2_uadmin_args {
	union sysmsg sysmsg;
	int	cmd;	char cmd_[PAD_(int)];
	int	func;	char func_[PAD_(int)];
	caddr_t	data;	char data_[PAD_(caddr_t)];
};
struct	ibcs2_utssys_args {
	union sysmsg sysmsg;
	int	a1;	char a1_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	flag;	char flag_[PAD_(int)];
};
struct	ibcs2_execve_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	char **	argp;	char argp_[PAD_(char **)];
	char **	envp;	char envp_[PAD_(char **)];
};
struct	ibcs2_fcntl_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	int	cmd;	char cmd_[PAD_(int)];
	char *	arg;	char arg_[PAD_(char *)];
};
struct	ibcs2_ulimit_args {
	union sysmsg sysmsg;
	int	cmd;	char cmd_[PAD_(int)];
	int	newlimit;	char newlimit_[PAD_(int)];
};
struct	ibcs2_rmdir_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
};
struct	ibcs2_mkdir_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	int	mode;	char mode_[PAD_(int)];
};
struct	ibcs2_getdents_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	char *	buf;	char buf_[PAD_(char *)];
	int	nbytes;	char nbytes_[PAD_(int)];
};
struct	ibcs2_sysfs_args {
	union sysmsg sysmsg;
	int	cmd;	char cmd_[PAD_(int)];
	caddr_t	d1;	char d1_[PAD_(caddr_t)];
	char *	buf;	char buf_[PAD_(char *)];
};
struct	ibcs2_getmsg_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	struct ibcs2_stropts *	ctl;	char ctl_[PAD_(struct ibcs2_stropts *)];
	struct ibcs2_stropts *	dat;	char dat_[PAD_(struct ibcs2_stropts *)];
	int *	flags;	char flags_[PAD_(int *)];
};
struct	ibcs2_putmsg_args {
	union sysmsg sysmsg;
	int	fd;	char fd_[PAD_(int)];
	struct ibcs2_stropts *	ctl;	char ctl_[PAD_(struct ibcs2_stropts *)];
	struct ibcs2_stropts *	dat;	char dat_[PAD_(struct ibcs2_stropts *)];
	int	flags;	char flags_[PAD_(int)];
};
struct	ibcs2_poll_args {
	union sysmsg sysmsg;
	struct ibcs2_poll *	fds;	char fds_[PAD_(struct ibcs2_poll *)];
	long	nfds;	char nfds_[PAD_(long)];
	int	timeout;	char timeout_[PAD_(int)];
};
struct	ibcs2_secure_args {
	union sysmsg sysmsg;
	int	cmd;	char cmd_[PAD_(int)];
	int	a1;	char a1_[PAD_(int)];
	int	a2;	char a2_[PAD_(int)];
	int	a3;	char a3_[PAD_(int)];
	int	a4;	char a4_[PAD_(int)];
	int	a5;	char a5_[PAD_(int)];
};
struct	ibcs2_symlink_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	char *	link;	char link_[PAD_(char *)];
};
struct	ibcs2_lstat_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	struct ibcs2_stat *	st;	char st_[PAD_(struct ibcs2_stat *)];
};
struct	ibcs2_readlink_args {
	union sysmsg sysmsg;
	char *	path;	char path_[PAD_(char *)];
	char *	buf;	char buf_[PAD_(char *)];
	int	count;	char count_[PAD_(int)];
};
struct	ibcs2_isc_args {
	union sysmsg sysmsg;
	register_t dummy;
};

#ifdef _KERNEL

int	ibcs2_read __P((struct ibcs2_read_args *));
int	ibcs2_open __P((struct ibcs2_open_args *));
int	ibcs2_wait __P((struct ibcs2_wait_args *));
int	ibcs2_creat __P((struct ibcs2_creat_args *));
int	ibcs2_unlink __P((struct ibcs2_unlink_args *));
int	ibcs2_execv __P((struct ibcs2_execv_args *));
int	ibcs2_chdir __P((struct ibcs2_chdir_args *));
int	ibcs2_time __P((struct ibcs2_time_args *));
int	ibcs2_mknod __P((struct ibcs2_mknod_args *));
int	ibcs2_chmod __P((struct ibcs2_chmod_args *));
int	ibcs2_chown __P((struct ibcs2_chown_args *));
int	ibcs2_stat __P((struct ibcs2_stat_args *));
int	ibcs2_lseek __P((struct ibcs2_lseek_args *));
int	ibcs2_mount __P((struct ibcs2_mount_args *));
int	ibcs2_umount __P((struct ibcs2_umount_args *));
int	ibcs2_setuid __P((struct ibcs2_setuid_args *));
int	ibcs2_stime __P((struct ibcs2_stime_args *));
int	ibcs2_alarm __P((struct ibcs2_alarm_args *));
int	ibcs2_fstat __P((struct ibcs2_fstat_args *));
int	ibcs2_pause __P((struct ibcs2_pause_args *));
int	ibcs2_utime __P((struct ibcs2_utime_args *));
int	ibcs2_stty __P((struct ibcs2_stty_args *));
int	ibcs2_gtty __P((struct ibcs2_gtty_args *));
int	ibcs2_access __P((struct ibcs2_access_args *));
int	ibcs2_nice __P((struct ibcs2_nice_args *));
int	ibcs2_statfs __P((struct ibcs2_statfs_args *));
int	ibcs2_kill __P((struct ibcs2_kill_args *));
int	ibcs2_fstatfs __P((struct ibcs2_fstatfs_args *));
int	ibcs2_pgrpsys __P((struct ibcs2_pgrpsys_args *));
int	ibcs2_xenix __P((struct ibcs2_xenix_args *));
int	ibcs2_times __P((struct ibcs2_times_args *));
int	ibcs2_plock __P((struct ibcs2_plock_args *));
int	ibcs2_setgid __P((struct ibcs2_setgid_args *));
int	ibcs2_sigsys __P((struct ibcs2_sigsys_args *));
int	ibcs2_msgsys __P((struct ibcs2_msgsys_args *));
int	ibcs2_sysi86 __P((struct ibcs2_sysi86_args *));
int	ibcs2_shmsys __P((struct ibcs2_shmsys_args *));
int	ibcs2_semsys __P((struct ibcs2_semsys_args *));
int	ibcs2_ioctl __P((struct ibcs2_ioctl_args *));
int	ibcs2_uadmin __P((struct ibcs2_uadmin_args *));
int	ibcs2_utssys __P((struct ibcs2_utssys_args *));
int	ibcs2_execve __P((struct ibcs2_execve_args *));
int	ibcs2_fcntl __P((struct ibcs2_fcntl_args *));
int	ibcs2_ulimit __P((struct ibcs2_ulimit_args *));
int	ibcs2_rmdir __P((struct ibcs2_rmdir_args *));
int	ibcs2_mkdir __P((struct ibcs2_mkdir_args *));
int	ibcs2_getdents __P((struct ibcs2_getdents_args *));
int	ibcs2_sysfs __P((struct ibcs2_sysfs_args *));
int	ibcs2_getmsg __P((struct ibcs2_getmsg_args *));
int	ibcs2_putmsg __P((struct ibcs2_putmsg_args *));
int	ibcs2_poll __P((struct ibcs2_poll_args *));
int	ibcs2_secure __P((struct ibcs2_secure_args *));
int	ibcs2_symlink __P((struct ibcs2_symlink_args *));
int	ibcs2_lstat __P((struct ibcs2_lstat_args *));
int	ibcs2_readlink __P((struct ibcs2_readlink_args *));
int	ibcs2_isc __P((struct ibcs2_isc_args *));

#endif /* _KERNEL */

#ifdef COMPAT_43


#ifdef _KERNEL


#endif /* _KERNEL */

#endif /* COMPAT_43 */

#undef PAD_

#endif /* !_IBCS2_SYSPROTO_H_ */
