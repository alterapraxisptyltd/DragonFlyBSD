/*-
 * Copyright (c) 1994-1995 S�ren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/compat/linux/linux_file.c,v 1.41.2.6 2003/01/06 09:19:43 fjoe Exp $
 * $DragonFly: src/sys/emulation/linux/linux_file.c,v 1.7 2003/07/30 00:19:13 dillon Exp $
 */

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>

#include <sys/file2.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_util.h>

#ifndef __alpha__
int
linux_creat(struct linux_creat_args *args)
{
    struct open_args bsd_open_args;
    caddr_t sg;
    int error;

    sg = stackgap_init();
    CHECKALTCREAT(&sg, args->path);

#ifdef DEBUG
	if (ldebug(creat))
		printf(ARGS(creat, "%s, %d"), args->path, args->mode);
#endif
    bsd_open_args.sysmsg_result = 0;
    bsd_open_args.path = args->path;
    bsd_open_args.mode = args->mode;
    bsd_open_args.flags = O_WRONLY | O_CREAT | O_TRUNC;
    error = open(&bsd_open_args);
    args->sysmsg_result = bsd_open_args.sysmsg_result;
    return(error);
}
#endif /*!__alpha__*/

int
linux_open(struct linux_open_args *args)
{
    struct open_args bsd_open_args;
    int error;
    caddr_t sg;
    struct thread *td = curthread;
    struct proc *p = td->td_proc;

    KKASSERT(p);

    sg = stackgap_init();
    
    if (args->flags & LINUX_O_CREAT)
	CHECKALTCREAT(&sg, args->path);
    else
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(open))
		printf(ARGS(open, "%s, 0x%x, 0x%x"),
		    args->path, args->flags, args->mode);
#endif
    bsd_open_args.flags = 0;
    if (args->flags & LINUX_O_RDONLY)
	bsd_open_args.flags |= O_RDONLY;
    if (args->flags & LINUX_O_WRONLY) 
	bsd_open_args.flags |= O_WRONLY;
    if (args->flags & LINUX_O_RDWR)
	bsd_open_args.flags |= O_RDWR;
    if (args->flags & LINUX_O_NDELAY)
	bsd_open_args.flags |= O_NONBLOCK;
    if (args->flags & LINUX_O_APPEND)
	bsd_open_args.flags |= O_APPEND;
    if (args->flags & LINUX_O_SYNC)
	bsd_open_args.flags |= O_FSYNC;
    if (args->flags & LINUX_O_NONBLOCK)
	bsd_open_args.flags |= O_NONBLOCK;
    if (args->flags & LINUX_FASYNC)
	bsd_open_args.flags |= O_ASYNC;
    if (args->flags & LINUX_O_CREAT)
	bsd_open_args.flags |= O_CREAT;
    if (args->flags & LINUX_O_TRUNC)
	bsd_open_args.flags |= O_TRUNC;
    if (args->flags & LINUX_O_EXCL)
	bsd_open_args.flags |= O_EXCL;
    if (args->flags & LINUX_O_NOCTTY)
	bsd_open_args.flags |= O_NOCTTY;
    bsd_open_args.path = args->path;
    bsd_open_args.mode = args->mode;
    bsd_open_args.sysmsg_result = 0;
    error = open(&bsd_open_args);
    args->sysmsg_result = bsd_open_args.sysmsg_result;

    if (!error && !(bsd_open_args.flags & O_NOCTTY) && 
	SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
	struct filedesc *fdp = p->p_fd;
	struct file *fp = fdp->fd_ofiles[bsd_open_args.sysmsg_result];

	if (fp->f_type == DTYPE_VNODE)
	    fo_ioctl(fp, TIOCSCTTY, (caddr_t) 0, td);
    }
#ifdef DEBUG
	if (ldebug(open))
		printf(LMSG("open returns error %d"), error);
#endif
    return error;
}

int
linux_lseek(struct linux_lseek_args *args)
{
    struct lseek_args tmp_args;
    int error;

#ifdef DEBUG
	if (ldebug(lseek))
		printf(ARGS(lseek, "%d, %ld, %d"),
		    args->fdes, (long)args->off, args->whence);
#endif
    tmp_args.fd = args->fdes;
    tmp_args.offset = (off_t)args->off;
    tmp_args.whence = args->whence;
    tmp_args.sysmsg_result = 0;
    error = lseek(&tmp_args);
    args->sysmsg_result = tmp_args.sysmsg_result;
    return error;
}

#ifndef __alpha__
int
linux_llseek(struct linux_llseek_args *args)
{
	struct lseek_args bsd_args;
	int error;
	off_t off;

#ifdef DEBUG
	if (ldebug(llseek))
		printf(ARGS(llseek, "%d, %d:%d, %d"),
		    args->fd, args->ohigh, args->olow, args->whence);
#endif
	off = (args->olow) | (((off_t) args->ohigh) << 32);

	bsd_args.fd = args->fd;
	bsd_args.offset = off;
	bsd_args.whence = args->whence;
	bsd_args.sysmsg_result = 0;

	if ((error = lseek(&bsd_args)))
		return error;

	if ((error = copyout(&bsd_args.sysmsg_offset, (caddr_t)args->res, sizeof (off_t)))) {
		return error;
	}
	args->sysmsg_result = 0;
	return 0;
}
#endif /*!__alpha__*/

#ifndef __alpha__
int
linux_readdir(struct linux_readdir_args *args)
{
	struct linux_getdents_args lda;
	int error;

	lda.fd = args->fd;
	lda.dent = args->dent;
	lda.count = 1;
	lda.sysmsg_result = 0;
	error = linux_getdents(&lda);
	args->sysmsg_result = lda.sysmsg_result;
	return(error);
}
#endif /*!__alpha__*/

/*
 * Note that linux_getdents(2) and linux_getdents64(2) have the same
 * arguments. They only differ in the definition of struct dirent they
 * operate on. We use this to common the code, with the exception of
 * accessing struct dirent. Note that linux_readdir(2) is implemented
 * by means of linux_getdents(2). In this case we never operate on
 * struct dirent64 and thus don't need to handle it...
 */

struct l_dirent {
	l_long		d_ino;
	l_off_t		d_off;
	l_ushort	d_reclen;
	char		d_name[LINUX_NAME_MAX + 1];
};

struct l_dirent64 {
	uint64_t	d_ino;
	int64_t		d_off;
	l_ushort	d_reclen;
	u_char		d_type;
	char		d_name[LINUX_NAME_MAX + 1];
};

#define LINUX_RECLEN(de,namlen) \
    ALIGN((((char *)&(de)->d_name - (char *)de) + (namlen) + 1))

#define	LINUX_DIRBLKSIZ		512

static int
getdents_common(struct linux_getdents64_args *args, int is64bit)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct dirent *bdp;
	struct vnode *vp;
	caddr_t inp, buf;		/* BSD-format */
	int len, reclen;		/* BSD-format */
	caddr_t outp;			/* Linux-format */
	int resid, linuxreclen=0;	/* Linux-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct vattr va;
	off_t off;
	struct l_dirent linux_dirent;
	struct l_dirent64 linux_dirent64;
	int buflen, error, eofflag, nbytes, justone;
	u_long *cookies = NULL, *cookiep;
	int ncookies;

	KKASSERT(p);

	if ((error = getvnode(p->p_fd, args->fd, &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);

	vp = (struct vnode *) fp->f_data;
	if (vp->v_type != VDIR)
		return (EINVAL);

	if ((error = VOP_GETATTR(vp, &va, td)))
		return (error);

	nbytes = args->count;
	if (nbytes == 1) {
		/* readdir(2) case. Always struct dirent. */
		if (is64bit)
			return (EINVAL);
		nbytes = sizeof(linux_dirent);
		justone = 1;
	} else
		justone = 0;

	off = fp->f_offset;

	buflen = max(LINUX_DIRBLKSIZ, nbytes);
	buflen = min(buflen, MAXBSIZE);
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_td = td;
	auio.uio_resid = buflen;
	auio.uio_offset = off;

	if (cookies) {
		free(cookies, M_TEMP);
		cookies = NULL;
	}

	if ((error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &ncookies,
		 &cookies)))
		goto out;

	inp = buf;
	outp = (caddr_t)args->dirent;
	resid = nbytes;
	if ((len = buflen - auio.uio_resid) <= 0)
		goto eof;

	cookiep = cookies;

	if (cookies) {
		/*
		 * When using cookies, the vfs has the option of reading from
		 * a different offset than that supplied (UFS truncates the
		 * offset to a block boundary to make sure that it never reads
		 * partway through a directory entry, even if the directory
		 * has been compacted).
		 */
		while (len > 0 && ncookies > 0 && *cookiep <= off) {
			bdp = (struct dirent *) inp;
			len -= bdp->d_reclen;
			inp += bdp->d_reclen;
			cookiep++;
			ncookies--;
		}
	}

	while (len > 0) {
		if (cookiep && ncookies == 0)
			break;
		bdp = (struct dirent *) inp;
		reclen = bdp->d_reclen;
		if (reclen & 3) {
			error = EFAULT;
			goto out;
		}

		if (bdp->d_fileno == 0) {
			inp += reclen;
			if (cookiep) {
				off = *cookiep++;
				ncookies--;
			} else
				off += reclen;

			len -= reclen;
			continue;
		}

		linuxreclen = (is64bit)
		    ? LINUX_RECLEN(&linux_dirent64, bdp->d_namlen)
		    : LINUX_RECLEN(&linux_dirent, bdp->d_namlen);

		if (reclen > len || resid < linuxreclen) {
			outp++;
			break;
		}

		if (justone) {
			/* readdir(2) case. */
			linux_dirent.d_ino = (l_long)bdp->d_fileno;
			linux_dirent.d_off = (l_off_t)linuxreclen;
			linux_dirent.d_reclen = (l_ushort)bdp->d_namlen;
			strcpy(linux_dirent.d_name, bdp->d_name);
			error = copyout(&linux_dirent, outp, linuxreclen);
		} else {
			if (is64bit) {
				linux_dirent64.d_ino = bdp->d_fileno;
				linux_dirent64.d_off = (cookiep)
				    ? (l_off_t)*cookiep
				    : (l_off_t)(off + reclen);
				linux_dirent64.d_reclen =
				    (l_ushort)linuxreclen;
				linux_dirent64.d_type = bdp->d_type;
				strcpy(linux_dirent64.d_name, bdp->d_name);
				error = copyout(&linux_dirent64, outp,
				    linuxreclen);
			} else {
				linux_dirent.d_ino = bdp->d_fileno;
				linux_dirent.d_off = (cookiep)
				    ? (l_off_t)*cookiep
				    : (l_off_t)(off + reclen);
				linux_dirent.d_reclen = (l_ushort)linuxreclen;
				strcpy(linux_dirent.d_name, bdp->d_name);
				error = copyout(&linux_dirent, outp,
				    linuxreclen);
			}
		}
		if (error)
			goto out;

		inp += reclen;
		if (cookiep) {
			off = *cookiep++;
			ncookies--;
		} else
			off += reclen;

		outp += linuxreclen;
		resid -= linuxreclen;
		len -= reclen;
		if (justone)
			break;
	}

	if (outp == (caddr_t)args->dirent)
		goto again;

	fp->f_offset = off;
	if (justone)
		nbytes = resid + linuxreclen;

eof:
	args->sysmsg_result = nbytes - resid;

out:
	if (cookies)
		free(cookies, M_TEMP);

	VOP_UNLOCK(vp, 0, td);
	free(buf, M_TEMP);
	return (error);
}

int
linux_getdents(struct linux_getdents_args *args)
{
#ifdef DEBUG
	if (ldebug(getdents))
		printf(ARGS(getdents, "%d, *, %d"), args->fd, args->count);
#endif
	return (getdents_common((struct linux_getdents64_args*)args, 0));
}

int
linux_getdents64(struct linux_getdents64_args *args)
{
#ifdef DEBUG
	if (ldebug(getdents64))
		printf(ARGS(getdents64, "%d, *, %d"), args->fd, args->count);
#endif
	return (getdents_common(args, 1));
}

/*
 * These exist mainly for hooks for doing /compat/linux translation.
 */

int
linux_access(struct linux_access_args *args)
{
	struct access_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(access))
		printf(ARGS(access, "%s, %d"), args->path, args->flags);
#endif
	bsd.path = args->path;
	bsd.flags = args->flags;
	bsd.sysmsg_result = 0;

	error = access(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_unlink(struct linux_unlink_args *args)
{
	struct unlink_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(unlink))
		printf(ARGS(unlink, "%s"), args->path);
#endif
	bsd.path = args->path;
	bsd.sysmsg_result = 0;

	error = unlink(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_chdir(struct linux_chdir_args *args)
{
	struct chdir_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(chdir))
		printf(ARGS(chdir, "%s"), args->path);
#endif
	bsd.path = args->path;
	bsd.sysmsg_result = 0;

	error = chdir(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_chmod(struct linux_chmod_args *args)
{
	struct chmod_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(chmod))
		printf(ARGS(chmod, "%s, %d"), args->path, args->mode);
#endif
	bsd.path = args->path;
	bsd.mode = args->mode;
	bsd.sysmsg_result = 0;

	error = chmod(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_mkdir(struct linux_mkdir_args *args)
{
	struct mkdir_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTCREAT(&sg, args->path);

#ifdef DEBUG
	if (ldebug(mkdir))
		printf(ARGS(mkdir, "%s, %d"), args->path, args->mode);
#endif
	bsd.path = args->path;
	bsd.mode = args->mode;
	bsd.sysmsg_result = 0;

	error = mkdir(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_rmdir(struct linux_rmdir_args *args)
{
	struct rmdir_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(rmdir))
		printf(ARGS(rmdir, "%s"), args->path);
#endif
	bsd.path = args->path;
	bsd.sysmsg_result = 0;

	error = rmdir(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_rename(struct linux_rename_args *args)
{
	struct rename_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->from);
	CHECKALTCREAT(&sg, args->to);

#ifdef DEBUG
	if (ldebug(rename))
		printf(ARGS(rename, "%s, %s"), args->from, args->to);
#endif
	bsd.from = args->from;
	bsd.to = args->to;
	bsd.sysmsg_result = 0;

	error = rename(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_symlink(struct linux_symlink_args *args)
{
	struct symlink_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);
	CHECKALTCREAT(&sg, args->to);

#ifdef DEBUG
	if (ldebug(symlink))
		printf(ARGS(symlink, "%s, %s"), args->path, args->to);
#endif
	bsd.path = args->path;
	bsd.link = args->to;
	bsd.sysmsg_result = 0;

	error = symlink(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_readlink(struct linux_readlink_args *args)
{
	struct readlink_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->name);

#ifdef DEBUG
	if (ldebug(readlink))
		printf(ARGS(readlink, "%s, %p, %d"),
		    args->name, (void *)args->buf, args->count);
#endif
	bsd.path = args->name;
	bsd.buf = args->buf;
	bsd.count = args->count;
	bsd.sysmsg_result = 0;

	error = readlink(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_truncate(struct linux_truncate_args *args)
{
	struct truncate_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(truncate))
		printf(ARGS(truncate, "%s, %ld"), args->path,
		    (long)args->length);
#endif
	bsd.path = args->path;
	bsd.length = args->length;
	bsd.sysmsg_result = 0;

	error = truncate(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_link(struct linux_link_args *args)
{
    struct link_args bsd;
    caddr_t sg;
    int error;

    sg = stackgap_init();
    CHECKALTEXIST(&sg, args->path);
    CHECKALTCREAT(&sg, args->to);

#ifdef DEBUG
	if (ldebug(link))
		printf(ARGS(link, "%s, %s"), args->path, args->to);
#endif

    bsd.path = args->path;
    bsd.link = args->to;
    bsd.sysmsg_result = 0;

    error = link(&bsd);
    args->sysmsg_result = bsd.sysmsg_result;
    return(error);
}

#ifndef __alpha__
int
linux_fdatasync(struct linux_fdatasync_args *uap)
{
	struct fsync_args bsd;
	int error;

	bsd.fd = uap->fd;
	bsd.sysmsg_result = 0;

	error = fsync(&bsd);
	uap->sysmsg_result = bsd.sysmsg_result;
	return(error);
}
#endif /*!__alpha__*/

int
linux_pread(struct linux_pread_args *uap)
{
	struct pread_args bsd;
	int error;

	bsd.fd = uap->fd;
	bsd.buf = uap->buf;
	bsd.nbyte = uap->nbyte;
	bsd.offset = uap->offset;
	bsd.sysmsg_result = 0;

	error = pread(&bsd);
	uap->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_pwrite(struct linux_pwrite_args *uap)
{
	struct pwrite_args bsd;
	int error;

	bsd.fd = uap->fd;
	bsd.buf = uap->buf;
	bsd.nbyte = uap->nbyte;
	bsd.offset = uap->offset;
	bsd.sysmsg_result = 0;

	error = pwrite(&bsd);
	uap->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_oldumount(struct linux_oldumount_args *args)
{
	struct linux_umount_args args2;
	int error;

	args2.path = args->path;
	args2.flags = 0;
	args2.sysmsg_result = 0;
	error = linux_umount(&args2);
	args->sysmsg_result = args2.sysmsg_result;
	return(error);
}

int
linux_umount(struct linux_umount_args *args)
{
	struct unmount_args bsd;
	int error;

	bsd.path = args->path;
	bsd.flags = args->flags;	/* XXX correct? */
	bsd.sysmsg_result = 0;

	error = unmount(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

/*
 * fcntl family of syscalls
 */

struct l_flock {
	l_short		l_type;
	l_short		l_whence;
	l_off_t		l_start;
	l_off_t		l_len;
	l_pid_t		l_pid;
};

static void
linux_to_bsd_flock(struct l_flock *linux_flock, struct flock *bsd_flock)
{
	switch (linux_flock->l_type) {
	case LINUX_F_RDLCK:
		bsd_flock->l_type = F_RDLCK;
		break;
	case LINUX_F_WRLCK:
		bsd_flock->l_type = F_WRLCK;
		break;
	case LINUX_F_UNLCK:
		bsd_flock->l_type = F_UNLCK;
		break;
	default:
		bsd_flock->l_type = -1;
		break;
	}
	bsd_flock->l_whence = linux_flock->l_whence;
	bsd_flock->l_start = (off_t)linux_flock->l_start;
	bsd_flock->l_len = (off_t)linux_flock->l_len;
	bsd_flock->l_pid = (pid_t)linux_flock->l_pid;
}

static void
bsd_to_linux_flock(struct flock *bsd_flock, struct l_flock *linux_flock)
{
	switch (bsd_flock->l_type) {
	case F_RDLCK:
		linux_flock->l_type = LINUX_F_RDLCK;
		break;
	case F_WRLCK:
		linux_flock->l_type = LINUX_F_WRLCK;
		break;
	case F_UNLCK:
		linux_flock->l_type = LINUX_F_UNLCK;
		break;
	}
	linux_flock->l_whence = bsd_flock->l_whence;
	linux_flock->l_start = (l_off_t)bsd_flock->l_start;
	linux_flock->l_len = (l_off_t)bsd_flock->l_len;
	linux_flock->l_pid = (l_pid_t)bsd_flock->l_pid;
}

#if defined(__i386__)
struct l_flock64 {
	l_short		l_type;
	l_short		l_whence;
	l_loff_t	l_start;
	l_loff_t	l_len;
	l_pid_t		l_pid;
};

static void
linux_to_bsd_flock64(struct l_flock64 *linux_flock, struct flock *bsd_flock)
{
	switch (linux_flock->l_type) {
	case LINUX_F_RDLCK:
		bsd_flock->l_type = F_RDLCK;
		break;
	case LINUX_F_WRLCK:
		bsd_flock->l_type = F_WRLCK;
		break;
	case LINUX_F_UNLCK:
		bsd_flock->l_type = F_UNLCK;
		break;
	default:
		bsd_flock->l_type = -1;
		break;
	}
	bsd_flock->l_whence = linux_flock->l_whence;
	bsd_flock->l_start = (off_t)linux_flock->l_start;
	bsd_flock->l_len = (off_t)linux_flock->l_len;
	bsd_flock->l_pid = (pid_t)linux_flock->l_pid;
}

static void
bsd_to_linux_flock64(struct flock *bsd_flock, struct l_flock64 *linux_flock)
{
	switch (bsd_flock->l_type) {
	case F_RDLCK:
		linux_flock->l_type = LINUX_F_RDLCK;
		break;
	case F_WRLCK:
		linux_flock->l_type = LINUX_F_WRLCK;
		break;
	case F_UNLCK:
		linux_flock->l_type = LINUX_F_UNLCK;
		break;
	}
	linux_flock->l_whence = bsd_flock->l_whence;
	linux_flock->l_start = (l_loff_t)bsd_flock->l_start;
	linux_flock->l_len = (l_loff_t)bsd_flock->l_len;
	linux_flock->l_pid = (l_pid_t)bsd_flock->l_pid;
}
#endif /* __i386__ */

#if defined(__alpha__)
#define	linux_fcntl64_args	linux_fcntl_args
#endif

static int
fcntl_common(struct linux_fcntl64_args *args)
{
	struct proc *p = curproc;
	struct l_flock linux_flock;
	struct flock *bsd_flock;
	struct fcntl_args fcntl_args;
	struct filedesc *fdp;
	struct file *fp;
	int error, result;
	caddr_t sg;

	sg = stackgap_init();
	bsd_flock = (struct flock *)stackgap_alloc(&sg, sizeof(bsd_flock));

	fcntl_args.fd = args->fd;
	fcntl_args.sysmsg_result = 0;

	switch (args->cmd) {
	case LINUX_F_DUPFD:
		fcntl_args.cmd = F_DUPFD;
		fcntl_args.arg = args->arg;
		break;
	case LINUX_F_GETFD:
		fcntl_args.cmd = F_GETFD;
		break;
	case LINUX_F_SETFD:
		fcntl_args.cmd = F_SETFD;
		fcntl_args.arg = args->arg;
		break;
	case LINUX_F_GETFL:
		fcntl_args.cmd = F_GETFL;
		error = fcntl(&fcntl_args);
		result = fcntl_args.sysmsg_result;
		args->sysmsg_result = 0;
		if (result & O_RDONLY)
			args->sysmsg_result |= LINUX_O_RDONLY;
		if (result & O_WRONLY)
			args->sysmsg_result |= LINUX_O_WRONLY;
		if (result & O_RDWR)
			args->sysmsg_result |= LINUX_O_RDWR;
		if (result & O_NDELAY)
			args->sysmsg_result |= LINUX_O_NONBLOCK;
		if (result & O_APPEND)
			args->sysmsg_result |= LINUX_O_APPEND;
		if (result & O_FSYNC)
			args->sysmsg_result |= LINUX_O_SYNC;
		if (result & O_ASYNC)
			args->sysmsg_result |= LINUX_FASYNC;
		return (error);

	case LINUX_F_SETFL:
		fcntl_args.arg = 0;
		if (args->arg & LINUX_O_NDELAY)
			fcntl_args.arg |= O_NONBLOCK;
		if (args->arg & LINUX_O_APPEND)
			fcntl_args.arg |= O_APPEND;
		if (args->arg & LINUX_O_SYNC)
			fcntl_args.arg |= O_FSYNC;
		if (args->arg & LINUX_FASYNC)
			fcntl_args.arg |= O_ASYNC;
		fcntl_args.cmd = F_SETFL;
		break;

	case LINUX_F_GETLK:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_GETLK;
		fcntl_args.arg = (long)bsd_flock;
		error = fcntl(&fcntl_args);
		args->sysmsg_result = fcntl_args.sysmsg_result;
		if (error)
			return (error);
		bsd_to_linux_flock(bsd_flock, &linux_flock);
		return (copyout(&linux_flock, (caddr_t)args->arg,
		    sizeof(linux_flock)));

	case LINUX_F_SETLK:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLK;
		fcntl_args.arg = (long)bsd_flock;
		break;
	case LINUX_F_SETLKW:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLKW;
		fcntl_args.arg = (long)bsd_flock;
		break;
	case LINUX_F_GETOWN:
		fcntl_args.cmd = F_GETOWN;
		break;
	case LINUX_F_SETOWN:
		/*
		 * XXX some Linux applications depend on F_SETOWN having no
		 * significant effect for pipes (SIGIO is not delivered for
		 * pipes under Linux-2.2.35 at least).
		 */
		fdp = p->p_fd;
		if ((u_int)args->fd >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[args->fd]) == NULL)
			return (EBADF);
		if (fp->f_type == DTYPE_PIPE)
			return (EINVAL);
		fcntl_args.cmd = F_SETOWN;
		fcntl_args.arg = args->arg;
		break;
	default:
		return (EINVAL);
	}
	error = fcntl(&fcntl_args);
	args->sysmsg_result = fcntl_args.sysmsg_result;
	return(error);
}

int
linux_fcntl(struct linux_fcntl_args *args)
{
	struct linux_fcntl64_args args64;
	int error;

#ifdef DEBUG
	if (ldebug(fcntl))
		printf(ARGS(fcntl, "%d, %08x, *"), args->fd, args->cmd);
#endif

	args64.fd = args->fd;
	args64.cmd = args->cmd;
	args64.arg = args->arg;
	args64.sysmsg_result = 0;
	error = fcntl_common(&args64);
	args->sysmsg_result = args64.sysmsg_result;
	return(error);
}

#if defined(__i386__)
int
linux_fcntl64(struct linux_fcntl64_args *args)
{
	struct fcntl_args fcntl_args;
	struct l_flock64 linux_flock;
	struct flock *bsd_flock;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	bsd_flock = (struct flock *)stackgap_alloc(&sg, sizeof(bsd_flock));

#ifdef DEBUG
	if (ldebug(fcntl64))
		printf(ARGS(fcntl64, "%d, %08x, *"), args->fd, args->cmd);
#endif
	fcntl_args.sysmsg_result = 0;

	switch (args->cmd) {
	case LINUX_F_GETLK64:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_GETLK;
		fcntl_args.arg = (long)bsd_flock;
		error = fcntl(&fcntl_args);
		args->sysmsg_result = fcntl_args.sysmsg_result;
		if (error)
			return (error);
		bsd_to_linux_flock64(bsd_flock, &linux_flock);
		return (copyout(&linux_flock, (caddr_t)args->arg,
		    sizeof(linux_flock)));

	case LINUX_F_SETLK64:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLK;
		fcntl_args.arg = (long)bsd_flock;
		error = fcntl(&fcntl_args);
		args->sysmsg_result = fcntl_args.sysmsg_result;
		return(error);

	case LINUX_F_SETLKW64:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLKW;
		fcntl_args.arg = (long)bsd_flock;
		error = fcntl(&fcntl_args);
		args->sysmsg_result = fcntl_args.sysmsg_result;
		return(error);
	}

	return (fcntl_common(args));
}
#endif /* __i386__ */

int
linux_chown(struct linux_chown_args *args)
{
	struct chown_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(chown))
		printf(ARGS(chown, "%s, %d, %d"), args->path, args->uid,
		    args->gid);
#endif

	bsd.path = args->path;
	bsd.uid = args->uid;
	bsd.gid = args->gid;
	bsd.sysmsg_result = 0;
	error = chown(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_lchown(struct linux_lchown_args *args)
{
	struct lchown_args bsd;
	caddr_t sg;
	int error;

	sg = stackgap_init();
	CHECKALTEXIST(&sg, args->path);

#ifdef DEBUG
	if (ldebug(lchown))
		printf(ARGS(lchown, "%s, %d, %d"), args->path, args->uid,
		    args->gid);
#endif

	bsd.path = args->path;
	bsd.uid = args->uid;
	bsd.gid = args->gid;
	bsd.sysmsg_result = 0;

	error = lchown(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

