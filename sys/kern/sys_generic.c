/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)sys_generic.c	8.5 (Berkeley) 1/21/94
 * $FreeBSD: src/sys/kern/sys_generic.c,v 1.55.2.10 2001/03/17 10:39:32 peter Exp $
 * $DragonFly: src/sys/kern/sys_generic.c,v 1.10 2003/07/30 00:19:14 dillon Exp $
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/buf.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <sys/file2.h>

#include <machine/limits.h>

static MALLOC_DEFINE(M_IOCTLOPS, "ioctlops", "ioctl data buffer");
static MALLOC_DEFINE(M_SELECT, "select", "select() buffer");
MALLOC_DEFINE(M_IOV, "iov", "large iov's");

static int	pollscan __P((struct proc *, struct pollfd *, u_int, int *));
static int	selscan __P((struct proc *, fd_mask **, fd_mask **,
			int, int *));
static int	dofileread __P((struct file *, int, void *,
			size_t, off_t, int, int *));
static int	dofilewrite __P((struct file *, int,
			const void *, size_t, off_t, int, int *));

struct file*
holdfp(fdp, fd, flag)
	struct filedesc* fdp;
	int fd, flag;
{
	struct file* fp;

	if (((u_int)fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL ||
	    (fp->f_flag & flag) == 0) {
		return (NULL);
	}
	fhold(fp);
	return (fp);
}

/*
 * Read system call.
 */
int
read(struct read_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct file *fp;
	int error;

	KKASSERT(p);
	if ((fp = holdfp(p->p_fd, uap->fd, FREAD)) == NULL)
		return (EBADF);
	error = dofileread(fp, uap->fd, uap->buf, uap->nbyte, (off_t)-1, 0,
			&uap->sysmsg_result);
	fdrop(fp, td);
	return(error);
}

/*
 * Pread system call
 */
int
pread(struct pread_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct file *fp;
	int error;

	KKASSERT(p);
	if ((fp = holdfp(p->p_fd, uap->fd, FREAD)) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE) {
		error = ESPIPE;
	} else {
	    error = dofileread(fp, uap->fd, uap->buf, uap->nbyte, 
		uap->offset, FOF_OFFSET, &uap->sysmsg_result);
	}
	fdrop(fp, td);
	return(error);
}

/*
 * Code common for read and pread
 */
int
dofileread(fp, fd, buf, nbyte, offset, flags, res)
	struct file *fp;
	int fd, flags;
	void *buf;
	size_t nbyte;
	off_t offset;
	int *res;
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
	struct uio ktruio;
	int didktr = 0;
#endif

	aiov.iov_base = (caddr_t)buf;
	aiov.iov_len = nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = offset;
	if (nbyte > INT_MAX)
		return (EINVAL);
	auio.uio_resid = nbyte;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(td, KTR_GENIO)) {
		ktriov = aiov;
		ktruio = auio;
		didktr = 1;
	}
#endif
	cnt = nbyte;

	if ((error = fo_read(fp, &auio, fp->f_cred, flags, td))) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (didktr && error == 0) {
		ktruio.uio_iov = &ktriov;
		ktruio.uio_resid = cnt;
		ktrgenio(p->p_tracep, fd, UIO_READ, &ktruio, error);
	}
#endif
	*res = cnt;
	return (error);
}

/*
 * Scatter read system call.
 */
int
readv(struct readv_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct file *fp;
	struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec *iov;
	struct iovec *needfree;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	u_int iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
	struct uio ktruio;
#endif

	if ((fp = holdfp(fdp, uap->fd, FREAD)) == NULL)
		return (EBADF);
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = uap->iovcnt * sizeof (struct iovec);
	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV)
			return (EINVAL);
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		needfree = iov;
	} else {
		iov = aiov;
		needfree = NULL;
	}
	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	auio.uio_offset = -1;
	if ((error = copyin((caddr_t)uap->iovp, (caddr_t)iov, iovlen)))
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < uap->iovcnt; i++) {
		if (iov->iov_len > INT_MAX - auio.uio_resid) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid += iov->iov_len;
		iov++;
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(td, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
		ktruio = auio;
	}
#endif
	cnt = auio.uio_resid;
	if ((error = fo_read(fp, &auio, fp->f_cred, 0, td))) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0) {
			ktruio.uio_iov = ktriov;
			ktruio.uio_resid = cnt;
			ktrgenio(p->p_tracep, uap->fd, UIO_READ, &ktruio,
			    error);
		}
		FREE(ktriov, M_TEMP);
	}
#endif
	uap->sysmsg_result = cnt;
done:
	fdrop(fp, td);
	if (needfree)
		FREE(needfree, M_IOV);
	return (error);
}

/*
 * Write system call
 */
int
write(struct write_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct file *fp;
	int error;

	KKASSERT(p);

	if ((fp = holdfp(p->p_fd, uap->fd, FWRITE)) == NULL)
		return (EBADF);
	error = dofilewrite(fp, uap->fd, uap->buf, uap->nbyte, (off_t)-1, 0,
			&uap->sysmsg_result);
	fdrop(fp, td);
	return(error);
}

/*
 * Pwrite system call
 */
int
pwrite(struct pwrite_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct file *fp;
	int error;

	KKASSERT(p);
	if ((fp = holdfp(p->p_fd, uap->fd, FWRITE)) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE) {
		error = ESPIPE;
	} else {
	    error = dofilewrite(fp, uap->fd, uap->buf, uap->nbyte,
		uap->offset, FOF_OFFSET, &uap->sysmsg_result);
	}
	fdrop(fp, td);
	return(error);
}

static int
dofilewrite(
	struct file *fp,
	int fd,
	const void *buf,
	size_t nbyte,
	off_t offset,
	int flags,
	int *res
) {
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
	struct uio ktruio;
	int didktr = 0;
#endif

	aiov.iov_base = (void *)(uintptr_t)buf;
	aiov.iov_len = nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = offset;
	if (nbyte > INT_MAX)
		return (EINVAL);
	auio.uio_resid = nbyte;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec and uio
	 */
	if (KTRPOINT(td, KTR_GENIO)) {
		ktriov = aiov;
		ktruio = auio;
		didktr = 1;
	}
#endif
	cnt = nbyte;
	if (fp->f_type == DTYPE_VNODE)
		bwillwrite();
	if ((error = fo_write(fp, &auio, fp->f_cred, flags, td))) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (didktr && error == 0) {
		ktruio.uio_iov = &ktriov;
		ktruio.uio_resid = cnt;
		ktrgenio(p->p_tracep, fd, UIO_WRITE, &ktruio, error);
	}
#endif
	*res = cnt;
	return (error);
}

/*
 * Gather write system call
 */
int
writev(struct writev_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct file *fp;
	struct filedesc *fdp;
	struct uio auio;
	struct iovec *iov;
	struct iovec *needfree;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	u_int iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
	struct uio ktruio;
#endif

	KKASSERT(p);
	fdp = p->p_fd;

	if ((fp = holdfp(fdp, uap->fd, FWRITE)) == NULL)
		return (EBADF);
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = uap->iovcnt * sizeof (struct iovec);
	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV) {
			needfree = NULL;
			error = EINVAL;
			goto done;
		}
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		needfree = iov;
	} else {
		iov = aiov;
		needfree = NULL;
	}
	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	auio.uio_offset = -1;
	if ((error = copyin((caddr_t)uap->iovp, (caddr_t)iov, iovlen)))
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < uap->iovcnt; i++) {
		if (iov->iov_len > INT_MAX - auio.uio_resid) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid += iov->iov_len;
		iov++;
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec and uio
	 */
	if (KTRPOINT(td, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
		ktruio = auio;
	}
#endif
	cnt = auio.uio_resid;
	if (fp->f_type == DTYPE_VNODE)
		bwillwrite();
	if ((error = fo_write(fp, &auio, fp->f_cred, 0, td))) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0) {
			ktruio.uio_iov = ktriov;
			ktruio.uio_resid = cnt;
			ktrgenio(p->p_tracep, uap->fd, UIO_WRITE, &ktruio,
			    error);
		}
		FREE(ktriov, M_TEMP);
	}
#endif
	uap->sysmsg_result = cnt;
done:
	fdrop(fp, td);
	if (needfree)
		FREE(needfree, M_IOV);
	return (error);
}

/*
 * Ioctl system call
 */
/* ARGSUSED */
int
ioctl(struct ioctl_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct file *fp;
	struct filedesc *fdp;
	u_long com;
	int error;
	u_int size;
	caddr_t data, memp;
	int tmp;
#define STK_PARAMS	128
	union {
	    char stkbuf[STK_PARAMS];
	    long align;
	} ubuf;

	KKASSERT(p);
	fdp = p->p_fd;
	if ((u_int)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

	switch (com = uap->com) {
	case FIONCLEX:
		fdp->fd_ofileflags[uap->fd] &= ~UF_EXCLOSE;
		return (0);
	case FIOCLEX:
		fdp->fd_ofileflags[uap->fd] |= UF_EXCLOSE;
		return (0);
	}

	/*
	 * Interpret high order word to find amount of data to be
	 * copied to/from the user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX)
		return (ENOTTY);

	fhold(fp);

	memp = NULL;
	if (size > sizeof (ubuf.stkbuf)) {
		memp = (caddr_t)malloc((u_long)size, M_IOCTLOPS, M_WAITOK);
		data = memp;
	} else {
		data = ubuf.stkbuf;
	}
	if (com&IOC_IN) {
		if (size) {
			error = copyin(uap->data, data, (u_int)size);
			if (error) {
				if (memp)
					free(memp, M_IOCTLOPS);
				fdrop(fp, td);
				return (error);
			}
		} else {
			*(caddr_t *)data = uap->data;
		}
	} else if ((com&IOC_OUT) && size) {
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(data, size);
	} else if (com&IOC_VOID) {
		*(caddr_t *)data = uap->data;
	}

	switch (com) {

	case FIONBIO:
		if ((tmp = *(int *)data))
			fp->f_flag |= FNONBLOCK;
		else
			fp->f_flag &= ~FNONBLOCK;
		error = fo_ioctl(fp, FIONBIO, (caddr_t)&tmp, td);
		break;

	case FIOASYNC:
		if ((tmp = *(int *)data))
			fp->f_flag |= FASYNC;
		else
			fp->f_flag &= ~FASYNC;
		error = fo_ioctl(fp, FIOASYNC, (caddr_t)&tmp, td);
		break;

	default:
		error = fo_ioctl(fp, com, data, td);
		/*
		 * Copy any data to user, size was
		 * already set and checked above.
		 */
		if (error == 0 && (com&IOC_OUT) && size)
			error = copyout(data, uap->data, (u_int)size);
		break;
	}
	if (memp)
		free(memp, M_IOCTLOPS);
	fdrop(fp, td);
	return (error);
}

static int	nselcoll;	/* Select collisions since boot */
int	selwait;
SYSCTL_INT(_kern, OID_AUTO, nselcoll, CTLFLAG_RD, &nselcoll, 0, "");

/*
 * Select system call.
 */
int
select(struct select_args *uap)
{
	struct proc *p = curproc;

	/*
	 * The magic 2048 here is chosen to be just enough for FD_SETSIZE
	 * infds with the new FD_SETSIZE of 1024, and more than enough for
	 * FD_SETSIZE infds, outfds and exceptfds with the old FD_SETSIZE
	 * of 256.
	 */
	fd_mask s_selbits[howmany(2048, NFDBITS)];
	fd_mask *ibits[3], *obits[3], *selbits, *sbp;
	struct timeval atv, rtv, ttv;
	int s, ncoll, error, timo;
	u_int nbufbytes, ncpbytes, nfdbits;

	if (uap->nd < 0)
		return (EINVAL);
	if (uap->nd > p->p_fd->fd_nfiles)
		uap->nd = p->p_fd->fd_nfiles;   /* forgiving; slightly wrong */

	/*
	 * Allocate just enough bits for the non-null fd_sets.  Use the
	 * preallocated auto buffer if possible.
	 */
	nfdbits = roundup(uap->nd, NFDBITS);
	ncpbytes = nfdbits / NBBY;
	nbufbytes = 0;
	if (uap->in != NULL)
		nbufbytes += 2 * ncpbytes;
	if (uap->ou != NULL)
		nbufbytes += 2 * ncpbytes;
	if (uap->ex != NULL)
		nbufbytes += 2 * ncpbytes;
	if (nbufbytes <= sizeof s_selbits)
		selbits = &s_selbits[0];
	else
		selbits = malloc(nbufbytes, M_SELECT, M_WAITOK);

	/*
	 * Assign pointers into the bit buffers and fetch the input bits.
	 * Put the output buffers together so that they can be bzeroed
	 * together.
	 */
	sbp = selbits;
#define	getbits(name, x) \
	do {								\
		if (uap->name == NULL)					\
			ibits[x] = NULL;				\
		else {							\
			ibits[x] = sbp + nbufbytes / 2 / sizeof *sbp;	\
			obits[x] = sbp;					\
			sbp += ncpbytes / sizeof *sbp;			\
			error = copyin(uap->name, ibits[x], ncpbytes);	\
			if (error != 0)					\
				goto done;				\
		}							\
	} while (0)
	getbits(in, 0);
	getbits(ou, 1);
	getbits(ex, 2);
#undef	getbits
	if (nbufbytes != 0)
		bzero(selbits, nbufbytes / 2);

	if (uap->tv) {
		error = copyin((caddr_t)uap->tv, (caddr_t)&atv,
			sizeof (atv));
		if (error)
			goto done;
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		getmicrouptime(&rtv);
		timevaladd(&atv, &rtv);
	} else {
		atv.tv_sec = 0;
		atv.tv_usec = 0;
	}
	timo = 0;
retry:
	ncoll = nselcoll;
	p->p_flag |= P_SELECT;
	error = selscan(p, ibits, obits, uap->nd, &uap->sysmsg_result);
	if (error || uap->sysmsg_result)
		goto done;
	if (atv.tv_sec || atv.tv_usec) {
		getmicrouptime(&rtv);
		if (timevalcmp(&rtv, &atv, >=)) 
			goto done;
		ttv = atv;
		timevalsub(&ttv, &rtv);
		timo = ttv.tv_sec > 24 * 60 * 60 ?
		    24 * 60 * 60 * hz : tvtohz(&ttv);
	}
	s = splhigh();
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	p->p_flag &= ~P_SELECT;

	error = tsleep((caddr_t)&selwait, PCATCH, "select", timo);
	
	splx(s);
	if (error == 0)
		goto retry;
done:
	p->p_flag &= ~P_SELECT;
	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
#define	putbits(name, x) \
	if (uap->name && (error2 = copyout(obits[x], uap->name, ncpbytes))) \
		error = error2;
	if (error == 0) {
		int error2;

		putbits(in, 0);
		putbits(ou, 1);
		putbits(ex, 2);
#undef putbits
	}
	if (selbits != &s_selbits[0])
		free(selbits, M_SELECT);
	return (error);
}

static int
selscan(struct proc *p, fd_mask **ibits, fd_mask **obits, int nfd, int *res)
{
	struct thread *td = p->p_thread;
	struct filedesc *fdp = p->p_fd;
	int msk, i, fd;
	fd_mask bits;
	struct file *fp;
	int n = 0;
	/* Note: backend also returns POLLHUP/POLLERR if appropriate. */
	static int flag[3] = { POLLRDNORM, POLLWRNORM, POLLRDBAND };

	for (msk = 0; msk < 3; msk++) {
		if (ibits[msk] == NULL)
			continue;
		for (i = 0; i < nfd; i += NFDBITS) {
			bits = ibits[msk][i/NFDBITS];
			/* ffs(int mask) not portable, fd_mask is long */
			for (fd = i; bits && fd < nfd; fd++, bits >>= 1) {
				if (!(bits & 1))
					continue;
				fp = fdp->fd_ofiles[fd];
				if (fp == NULL)
					return (EBADF);
				if (fo_poll(fp, flag[msk], fp->f_cred, td)) {
					obits[msk][(fd)/NFDBITS] |=
					    ((fd_mask)1 << ((fd) % NFDBITS));
					n++;
				}
			}
		}
	}
	*res = n;
	return (0);
}

/*
 * Poll system call.
 */
int
poll(struct poll_args *uap)
{
	caddr_t bits;
	char smallbits[32 * sizeof(struct pollfd)];
	struct timeval atv, rtv, ttv;
	int s, ncoll, error = 0, timo;
	u_int nfds;
	size_t ni;
	struct proc *p = curproc;

	nfds = SCARG(uap, nfds);
	/*
	 * This is kinda bogus.  We have fd limits, but that is not
	 * really related to the size of the pollfd array.  Make sure
	 * we let the process use at least FD_SETSIZE entries and at
	 * least enough for the current limits.  We want to be reasonably
	 * safe, but not overly restrictive.
	 */
	if (nfds > p->p_rlimit[RLIMIT_NOFILE].rlim_cur && nfds > FD_SETSIZE)
		return (EINVAL);
	ni = nfds * sizeof(struct pollfd);
	if (ni > sizeof(smallbits))
		bits = malloc(ni, M_TEMP, M_WAITOK);
	else
		bits = smallbits;
	error = copyin(SCARG(uap, fds), bits, ni);
	if (error)
		goto done;
	if (SCARG(uap, timeout) != INFTIM) {
		atv.tv_sec = SCARG(uap, timeout) / 1000;
		atv.tv_usec = (SCARG(uap, timeout) % 1000) * 1000;
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		getmicrouptime(&rtv);
		timevaladd(&atv, &rtv);
	} else {
		atv.tv_sec = 0;
		atv.tv_usec = 0;
	}
	timo = 0;
retry:
	ncoll = nselcoll;
	p->p_flag |= P_SELECT;
	error = pollscan(p, (struct pollfd *)bits, nfds, &uap->sysmsg_result);
	if (error || uap->sysmsg_result)
		goto done;
	if (atv.tv_sec || atv.tv_usec) {
		getmicrouptime(&rtv);
		if (timevalcmp(&rtv, &atv, >=))
			goto done;
		ttv = atv;
		timevalsub(&ttv, &rtv);
		timo = ttv.tv_sec > 24 * 60 * 60 ?
		    24 * 60 * 60 * hz : tvtohz(&ttv);
	} 
	s = splhigh(); 
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	p->p_flag &= ~P_SELECT;
	error = tsleep((caddr_t)&selwait, PCATCH, "poll", timo);
	splx(s);
	if (error == 0)
		goto retry;
done:
	p->p_flag &= ~P_SELECT;
	/* poll is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
	if (error == 0) {
		error = copyout(bits, SCARG(uap, fds), ni);
		if (error)
			goto out;
	}
out:
	if (ni > sizeof(smallbits))
		free(bits, M_TEMP);
	return (error);
}

static int
pollscan(struct proc *p, struct pollfd *fds, u_int nfd, int *res)
{
	struct thread *td = p->p_thread;
	struct filedesc *fdp = p->p_fd;
	int i;
	struct file *fp;
	int n = 0;

	for (i = 0; i < nfd; i++, fds++) {
		if (fds->fd >= fdp->fd_nfiles) {
			fds->revents = POLLNVAL;
			n++;
		} else if (fds->fd < 0) {
			fds->revents = 0;
		} else {
			fp = fdp->fd_ofiles[fds->fd];
			if (fp == NULL) {
				fds->revents = POLLNVAL;
				n++;
			} else {
				/*
				 * Note: backend also returns POLLHUP and
				 * POLLERR if appropriate.
				 */
				fds->revents = fo_poll(fp, fds->events,
				    fp->f_cred, td);
				if (fds->revents != 0)
					n++;
			}
		}
	}
	*res = n;
	return (0);
}

/*
 * OpenBSD poll system call.
 * XXX this isn't quite a true representation..  OpenBSD uses select ops.
 */
int
openbsd_poll(struct openbsd_poll_args *uap)
{
	return (poll((struct poll_args *)uap));
}

/*ARGSUSED*/
int
seltrue(dev_t dev, int events, struct thread *td)
{
	return (events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Record a select request.  A global wait must be used since a process/thread
 * might go away after recording its request.
 */
void
selrecord(struct thread *selector, struct selinfo *sip)
{
	struct proc *p;
	pid_t mypid;

	if ((p = selector->td_proc) == NULL)
		panic("selrecord: thread needs a process");

	mypid = p->p_pid;
	if (sip->si_pid == mypid)
		return;
	if (sip->si_pid && (p = pfind(sip->si_pid)) &&
	    p->p_wchan == (caddr_t)&selwait) {
		sip->si_flags |= SI_COLL;
	} else {
		sip->si_pid = mypid;
	}
}

/*
 * Do a wakeup when a selectable event occurs.
 */
void
selwakeup(struct selinfo *sip)
{
	struct proc *p;
	int s;

	if (sip->si_pid == 0)
		return;
	if (sip->si_flags & SI_COLL) {
		nselcoll++;
		sip->si_flags &= ~SI_COLL;
		wakeup((caddr_t)&selwait);	/* YYY fixable */
	}
	p = pfind(sip->si_pid);
	sip->si_pid = 0;
	if (p != NULL) {
		s = splhigh();
		if (p->p_wchan == (caddr_t)&selwait) {
			if (p->p_stat == SSLEEP)
				setrunnable(p);
			else
				unsleep(p->p_thread);
		} else if (p->p_flag & P_SELECT)
			p->p_flag &= ~P_SELECT;
		splx(s);
	}
}

