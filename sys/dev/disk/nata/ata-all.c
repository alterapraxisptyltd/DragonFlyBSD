/*-
 * Copyright (c) 1998 - 2006 S�ren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD: src/sys/dev/ata/ata-all.c,v 1.279 2007/02/23 16:25:08 jhb Exp $
 * $DragonFly: src/sys/dev/disk/nata/ata-all.c,v 1.14 2008/03/24 06:41:56 dillon Exp $
 */

#include "opt_ata.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/ctype.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>		/* for {get,rel}_mplock() */
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/nata.h>
#include <sys/objcache.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <sys/spinlock2.h>
#include <sys/mplock2.h>

#include "ata-all.h"
#include "ata_if.h"

/* device structure */
static  d_ioctl_t       ata_ioctl;
static struct dev_ops ata_ops = {
	{ "ata", 159, 0 },
	.d_open =	nullopen,
	.d_close =	nullclose,
	.d_ioctl =	ata_ioctl,
};

/* prototypes */
#if 0
static void ata_boot_attach(void);
#endif
static device_t ata_add_child(device_t, struct ata_device *, int);
static int ata_getparam(struct ata_device *, int);
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);

/* global vars */
MALLOC_DEFINE(M_ATA, "ata_generic", "ATA driver generic layer");
int (*ata_raid_ioctl_func)(u_long cmd, caddr_t data) = NULL;
devclass_t ata_devclass;
struct objcache *ata_request_cache;
struct objcache *ata_composite_cache;
struct objcache_malloc_args ata_request_malloc_args = {
	sizeof(struct ata_request), M_ATA };
struct objcache_malloc_args ata_composite_malloc_args = {
	sizeof(struct ata_composite), M_ATA };
int ata_wc = 1;

/* local vars */
static int ata_dma = 1;
static int atapi_dma = 1;

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, ata, CTLFLAG_RD, 0, "ATA driver parameters");
TUNABLE_INT("hw.ata.ata_dma", &ata_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma, CTLFLAG_RW, &ata_dma, 0,
	   "ATA disk DMA mode control");
TUNABLE_INT("hw.ata.atapi_dma", &atapi_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, atapi_dma, CTLFLAG_RW, &atapi_dma, 0,
	   "ATAPI device DMA mode control");
TUNABLE_INT("hw.ata.wc", &ata_wc);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_wc, CTLFLAG_RW, &ata_wc, 0,
	   "ATA disk write caching");

/*
 * newbus device interface related functions
 */
int
ata_probe(device_t dev)
{
    return 0;
}

int
ata_attach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    int error, rid;

    /* check that we have a virgin channel to attach */
    if (ch->r_irq)
	return EEXIST;

    /* initialize the softc basics */
    ch->dev = dev;
    ch->state = ATA_IDLE;
    spin_init(&ch->state_mtx);
    spin_init(&ch->queue_mtx);
    ata_queue_init(ch);

    /* reset the controller HW, the channel and device(s) */
    while (ATA_LOCKING(dev, ATA_LF_LOCK) != ch->unit)
	tsleep(&error, 0, "ataatch", 1);
    ATA_RESET(dev);
    ATA_LOCKING(dev, ATA_LF_UNLOCK);

    /* setup interrupt delivery */
    rid = ATA_IRQ_RID;
    ch->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
				       RF_SHAREABLE | RF_ACTIVE);
    if (!ch->r_irq) {
	device_printf(dev, "unable to allocate interrupt\n");
	return ENXIO;
    }
    if ((error = bus_setup_intr(dev, ch->r_irq, ATA_INTR_FLAGS,
				(driver_intr_t *)ata_interrupt, ch, &ch->ih,
				NULL))) {
	device_printf(dev, "unable to setup interrupt\n");
	return error;
    }

    /* probe and attach devices on this channel unless we are in early boot */
    ata_identify(dev);
    return 0;
}

int
ata_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int nchildren, i;

    /* check that we have a valid channel to detach */
    if (!ch->r_irq)
	return ENXIO;

    /* grap the channel lock so no new requests gets launched */
    spin_lock(&ch->state_mtx);
    ch->state |= ATA_STALL_QUEUE;
    spin_unlock(&ch->state_mtx);

    /* detach & delete all children */
    if (!device_get_children(dev, &children, &nchildren)) {
	for (i = 0; i < nchildren; i++)
	    if (children[i])
		device_delete_child(dev, children[i]);
	kfree(children, M_TEMP);
    } 

    /* release resources */
    bus_teardown_intr(dev, ch->r_irq, ch->ih);
    bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, ch->r_irq);
    ch->r_irq = NULL;
    spin_uninit(&ch->state_mtx);
    spin_uninit(&ch->queue_mtx);
    return 0;
}

int
ata_reinit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_request *request;
    device_t *children;
    int nchildren, i;

    /* check that we have a valid channel to reinit */
    if (!ch || !ch->r_irq)
	return ENXIO;

    if (bootverbose)
	device_printf(dev, "reiniting channel ..\n");

    /* poll for locking the channel */
    while (ATA_LOCKING(dev, ATA_LF_LOCK) != ch->unit)
	tsleep(&dev, 0, "atarini", 1);

    /* catch eventual request in ch->running */
    spin_lock(&ch->state_mtx);
    if ((request = ch->running))
	callout_stop(&request->callout);
    ch->running = NULL;

    /* unconditionally grap the channel lock */
    ch->state |= ATA_STALL_QUEUE;
    spin_unlock(&ch->state_mtx);

    /* reset the controller HW, the channel and device(s) */
    ATA_RESET(dev);

    /* reinit the children and delete any that fails */
    if (!device_get_children(dev, &children, &nchildren)) {
	get_mplock();
	for (i = 0; i < nchildren; i++) {
	    /* did any children go missing ? */
	    if (children[i] && device_is_attached(children[i]) &&
		ATA_REINIT(children[i])) {
		/*
		 * if we had a running request and its device matches
		 * this child we need to inform the request that the 
		 * device is gone.
		 */
		if (request && request->dev == children[i]) {
		    request->result = ENXIO;
		    device_printf(request->dev, "FAILURE - device detached\n");

		    /* if not timeout finish request here */
		    if (!(request->flags & ATA_R_TIMEOUT))
			    ata_finish(request);
		    request = NULL;
		}
		device_delete_child(dev, children[i]);
	    }
	}
	kfree(children, M_TEMP);
	rel_mplock();
    }

    /* if we still have a good request put it on the queue again */
    if (request && !(request->flags & ATA_R_TIMEOUT)) {
	device_printf(request->dev,
		      "WARNING - %s requeued due to channel reset",
		      ata_cmd2str(request));
	if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
	    kprintf(" LBA=%ju", request->u.ata.lba);
	kprintf("\n");
	request->flags |= ATA_R_REQUEUE;
	ata_queue_request(request);
    }

    /* we're done release the channel for new work */
    spin_lock(&ch->state_mtx);
    ch->state = ATA_IDLE;
    spin_unlock(&ch->state_mtx);
    ATA_LOCKING(dev, ATA_LF_UNLOCK);

    if (bootverbose)
	device_printf(dev, "reinit done ..\n");

    /* kick off requests on the queue */
    ata_start(dev);
    return 0;
}

int
ata_suspend(device_t dev)
{
    struct ata_channel *ch;

    /* check for valid device */
    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    /* wait for the channel to be IDLE or detached before suspending */
    while (ch->r_irq) {
	spin_lock(&ch->state_mtx);
	if (ch->state == ATA_IDLE) {
	    ch->state = ATA_ACTIVE;
	    spin_unlock(&ch->state_mtx);
	    break;
	}
	spin_unlock(&ch->state_mtx);
	tsleep(ch, 0, "atasusp", hz/10);
    }
    ATA_LOCKING(dev, ATA_LF_UNLOCK);
    return 0;
}

int
ata_resume(device_t dev)
{
    struct ata_channel *ch;
    int error;

    /* check for valid device */
    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    /* reinit the devices, we dont know what mode/state they are in */
    error = ata_reinit(dev);

    /* kick off requests on the queue */
    ata_start(dev);
    return error;
}

int
ata_interrupt(void *data)
{
    struct ata_channel *ch = (struct ata_channel *)data;
    struct ata_request *request;

    spin_lock(&ch->state_mtx);
    do {
	/*
	 * Ignore interrupt if its not for us.  This may also have the
	 * side effect of processing events unrelated to I/O requests.
	 */
	if (ch->hw.status && !ch->hw.status(ch->dev))
	    break;

	/*
	 * Check if we have a running request, and make sure it has been
	 * completely queued.  Otherwise the channel status may indicate
	 * not-busy when, in fact, the command had not yet been issued.
	 */
	if ((request = ch->running) == NULL)
	    break;
	if ((request->flags & ATA_R_HWCMDQUEUED) == 0) {
	    kprintf("ata_interrupt: early interrupt\n");
	    break;
	}

	ATA_DEBUG_RQ(request, "interrupt");

	/* safetycheck for the right state */
	if (ch->state == ATA_IDLE) {
	    device_printf(request->dev, "interrupt on idle channel ignored\n");
	    break;
	}

	/*
	 * we have the HW locks, so end the transaction for this request
	 * if it finishes immediately otherwise wait for next interrupt
	 */
	if (ch->hw.end_transaction(request) == ATA_OP_FINISHED) {
	    ch->running = NULL;
	    if (ch->state == ATA_ACTIVE)
		ch->state = ATA_IDLE;
	    spin_unlock(&ch->state_mtx);
	    ATA_LOCKING(ch->dev, ATA_LF_UNLOCK);
	    ata_finish(request);
	    return 1;
	}
    } while (0);
    spin_unlock(&ch->state_mtx);
    return 0;
}

/*
 * device related interfaces
 */
static int
ata_ioctl(struct dev_ioctl_args *ap)
{
    device_t device, *children;
    struct ata_ioc_devices *devices = (struct ata_ioc_devices *)ap->a_data;
    int *value = (int *)ap->a_data;
    int i, nchildren, error = ENOTTY;

    switch (ap->a_cmd) {
    case IOCATAGMAXCHANNEL:
	*value = devclass_get_maxunit(ata_devclass);
	error = 0;
	break;

    case IOCATAREINIT:
	if (*value > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)))
	    return ENXIO;
	error = ata_reinit(device);
	ata_start(device);
	break;

    case IOCATAATTACH:
	if (*value > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)))
	    return ENXIO;
	/* XXX SOS should enable channel HW on controller */
	error = ata_attach(device);
	break;

    case IOCATADETACH:
	if (*value > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)))
	    return ENXIO;
	error = ata_detach(device);
	/* XXX SOS should disable channel HW on controller */
	break;

    case IOCATADEVICES:
	if (devices->channel > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, devices->channel)))
	    return ENXIO;
	bzero(devices->name[0], 32);
	bzero(&devices->params[0], sizeof(struct ata_params));
	bzero(devices->name[1], 32);
	bzero(&devices->params[1], sizeof(struct ata_params));
	if (!device_get_children(device, &children, &nchildren)) {
	    for (i = 0; i < nchildren; i++) {
		if (children[i] && device_is_attached(children[i])) {
		    struct ata_device *atadev = device_get_softc(children[i]);

		    if (atadev->unit == ATA_MASTER) {
			strncpy(devices->name[0],
				device_get_nameunit(children[i]), 32);
			bcopy(&atadev->param, &devices->params[0],
			      sizeof(struct ata_params));
		    }
		    if (atadev->unit == ATA_SLAVE) {
			strncpy(devices->name[1],
				device_get_nameunit(children[i]), 32);
			bcopy(&atadev->param, &devices->params[1],
			      sizeof(struct ata_params));
		    }
		}
	    }
	    kfree(children, M_TEMP);
	    error = 0;
	}
	else
	    error = ENODEV;
	break;

    default:
	if (ata_raid_ioctl_func)
	    error = ata_raid_ioctl_func(ap->a_cmd, ap->a_data);
    }
    return error;
}

int
ata_device_ioctl(device_t dev, u_long cmd, caddr_t data)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct ata_ioc_request *ioc_request = (struct ata_ioc_request *)data;
    struct ata_params *params = (struct ata_params *)data;
    int *mode = (int *)data;
    struct ata_request *request;
    caddr_t buf;
    int error;

    switch (cmd) {
    case IOCATAREQUEST:
	if (!(buf = kmalloc(ioc_request->count, M_ATA, M_WAITOK | M_NULLOK))) {
	    return ENOMEM;
	}
	if (!(request = ata_alloc_request())) {
	    kfree(buf, M_ATA);
	    return  ENOMEM;
	}
	if (ioc_request->flags & ATA_CMD_WRITE) {
	    error = copyin(ioc_request->data, buf, ioc_request->count);
	    if (error) {
		kfree(buf, M_ATA);
		ata_free_request(request);
		return error;
	    }
	}
	request->dev = dev;
	if (ioc_request->flags & ATA_CMD_ATAPI) {
	    request->flags = ATA_R_ATAPI;
	    bcopy(ioc_request->u.atapi.ccb, request->u.atapi.ccb, 16);
	}
	else {
	    request->u.ata.command = ioc_request->u.ata.command;
	    request->u.ata.feature = ioc_request->u.ata.feature;
	    request->u.ata.lba = ioc_request->u.ata.lba;
	    request->u.ata.count = ioc_request->u.ata.count;
	}
	request->timeout = ioc_request->timeout;
	request->data = buf;
	request->bytecount = ioc_request->count;
	request->transfersize = request->bytecount;
	if (ioc_request->flags & ATA_CMD_CONTROL)
	    request->flags |= ATA_R_CONTROL;
	if (ioc_request->flags & ATA_CMD_READ)
	    request->flags |= ATA_R_READ;
	if (ioc_request->flags & ATA_CMD_WRITE)
	    request->flags |= ATA_R_WRITE;
	ata_queue_request(request);
	if (request->flags & ATA_R_ATAPI) {
	    bcopy(&request->u.atapi.sense, &ioc_request->u.atapi.sense,
		  sizeof(struct atapi_sense));
	}
	else {
	    ioc_request->u.ata.command = request->u.ata.command;
	    ioc_request->u.ata.feature = request->u.ata.feature;
	    ioc_request->u.ata.lba = request->u.ata.lba;
	    ioc_request->u.ata.count = request->u.ata.count;
	}
	ioc_request->error = request->result;
	if (ioc_request->flags & ATA_CMD_READ)
	    error = copyout(buf, ioc_request->data, ioc_request->count);
	else
	    error = 0;
	kfree(buf, M_ATA);
	ata_free_request(request);
	return error;
   
    case IOCATAGPARM:
	ata_getparam(atadev, 0);
	bcopy(&atadev->param, params, sizeof(struct ata_params));
	return 0;
	
    case IOCATASMODE:
	atadev->mode = *mode;
	ATA_SETMODE(device_get_parent(dev), dev);
	return 0;

    case IOCATAGMODE:
	*mode = atadev->mode;
	return 0;
    default:
	return ENOTTY;
    }
}

#if 0

static void
ata_boot_attach(void)
{
    struct ata_channel *ch;
    int ctlr;

    get_mplock();

    /* kick of probe and attach on all channels */
    for (ctlr = 0; ctlr < devclass_get_maxunit(ata_devclass); ctlr++) {
	if ((ch = devclass_get_softc(ata_devclass, ctlr))) {
	    ata_identify(ch->dev);
	}
    }

    rel_mplock();
}

#endif


/*
 * misc support functions
 */
static device_t
ata_add_child(device_t parent, struct ata_device *atadev, int unit)
{
    device_t child;

    if ((child = device_add_child(parent, NULL, unit))) {
	device_set_softc(child, atadev);
	device_quiet(child);
	atadev->dev = child;
	atadev->max_iosize = DEV_BSIZE;
	atadev->mode = ATA_PIO_MAX;
    }
    return child;
}

static int
ata_getparam(struct ata_device *atadev, int init)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(atadev->dev));
    struct ata_request *request;
    u_int8_t command = 0;
    int error = ENOMEM, retries = 2;

    if (ch->devices &
	(atadev->unit == ATA_MASTER ? ATA_ATA_MASTER : ATA_ATA_SLAVE))
	command = ATA_ATA_IDENTIFY;
    if (ch->devices &
	(atadev->unit == ATA_MASTER ? ATA_ATAPI_MASTER : ATA_ATAPI_SLAVE))
	command = ATA_ATAPI_IDENTIFY;
    if (!command)
	return ENXIO;

    while (retries-- > 0 && error) {
	if (!(request = ata_alloc_request()))
	    break;
	request->dev = atadev->dev;
	request->timeout = 1;
	request->retries = 0;
	request->u.ata.command = command;
	request->flags = (ATA_R_READ|ATA_R_AT_HEAD|ATA_R_DIRECT|ATA_R_QUIET);
	request->data = (void *)&atadev->param;
	request->bytecount = sizeof(struct ata_params);
	request->donecount = 0;
	request->transfersize = DEV_BSIZE;
	ata_queue_request(request);
	error = request->result;
	ata_free_request(request);
    }

    if (!error && (isprint(atadev->param.model[0]) ||
		   isprint(atadev->param.model[1]))) {
	struct ata_params *atacap = &atadev->param;
	char buffer[64];
	int16_t *ptr;

	for (ptr = (int16_t *)atacap;
	     ptr < (int16_t *)atacap + sizeof(struct ata_params)/2; ptr++) {
	    *ptr = le16toh(*ptr);
	}
	if (!(!strncmp(atacap->model, "FX", 2) ||
	      !strncmp(atacap->model, "NEC", 3) ||
	      !strncmp(atacap->model, "Pioneer", 7) ||
	      !strncmp(atacap->model, "SHARP", 5))) {
	    bswap(atacap->model, sizeof(atacap->model));
	    bswap(atacap->revision, sizeof(atacap->revision));
	    bswap(atacap->serial, sizeof(atacap->serial));
	}
	btrim(atacap->model, sizeof(atacap->model));
	bpack(atacap->model, atacap->model, sizeof(atacap->model));
	btrim(atacap->revision, sizeof(atacap->revision));
	bpack(atacap->revision, atacap->revision, sizeof(atacap->revision));
	btrim(atacap->serial, sizeof(atacap->serial));
	bpack(atacap->serial, atacap->serial, sizeof(atacap->serial));

	if (bootverbose)
	    kprintf("ata%d-%s: pio=%s wdma=%s udma=%s cable=%s wire\n",
		   device_get_unit(ch->dev),
		   atadev->unit == ATA_MASTER ? "master" : "slave",
		   ata_mode2str(ata_pmode(atacap)),
		   ata_mode2str(ata_wmode(atacap)),
		   ata_mode2str(ata_umode(atacap)),
		   (atacap->hwres & ATA_CABLE_ID) ? "80":"40");

	if (init) {
	    ksprintf(buffer, "%.40s/%.8s", atacap->model, atacap->revision);
	    device_set_desc_copy(atadev->dev, buffer);
	    if ((atadev->param.config & ATA_PROTO_ATAPI) &&
		(atadev->param.config != ATA_CFA_MAGIC1) &&
		(atadev->param.config != ATA_CFA_MAGIC2)) {
		if (atapi_dma && ch->dma &&
		    (atadev->param.config & ATA_DRQ_MASK) != ATA_DRQ_INTR &&
		    ata_umode(&atadev->param) >= ATA_UDMA2)
		    atadev->mode = ATA_DMA_MAX;
	    }
	    else {
		if (ata_dma && ch->dma &&
		    (ata_umode(&atadev->param) > 0 ||
		     ata_wmode(&atadev->param) > 0))
		    atadev->mode = ATA_DMA_MAX;
	    }
	}
    }
    else {
	if (!error)
	    error = ENXIO;
    }
    return error;
}

int
ata_identify(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_device *master = NULL, *slave = NULL;
    device_t master_child = NULL, slave_child = NULL;
    int master_unit = -1, slave_unit = -1;

    if (ch->devices & (ATA_ATA_MASTER | ATA_ATAPI_MASTER)) {
	if (!(master = kmalloc(sizeof(struct ata_device),
			      M_ATA, M_INTWAIT | M_ZERO))) {
	    device_printf(dev, "out of memory\n");
	    return ENOMEM;
	}
	master->unit = ATA_MASTER;
    }
    if (ch->devices & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE)) {
	if (!(slave = kmalloc(sizeof(struct ata_device),
			     M_ATA, M_INTWAIT | M_ZERO))) {
	    kfree(master, M_ATA);
	    device_printf(dev, "out of memory\n");
	    return ENOMEM;
	}
	slave->unit = ATA_SLAVE;
    }

#ifdef ATA_STATIC_ID
    if (ch->devices & ATA_ATA_MASTER)
	master_unit = (device_get_unit(dev) << 1);
#endif
    if (master && !(master_child = ata_add_child(dev, master, master_unit))) {
	kfree(master, M_ATA);
	master = NULL;
    }
#ifdef ATA_STATIC_ID
    if (ch->devices & ATA_ATA_SLAVE)
	slave_unit = (device_get_unit(dev) << 1) + 1;
#endif
    if (slave && !(slave_child = ata_add_child(dev, slave, slave_unit))) {
	kfree(slave, M_ATA);
	slave = NULL;
    }

    if (slave && ata_getparam(slave, 1)) {
	device_delete_child(dev, slave_child);
	kfree(slave, M_ATA);
    }
    if (master && ata_getparam(master, 1)) {
	device_delete_child(dev, master_child);
	kfree(master, M_ATA);
    }

    bus_generic_probe(dev);
    bus_generic_attach(dev);
    return 0;
}

void
ata_default_registers(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* fill in the defaults from whats setup already */
    ch->r_io[ATA_ERROR].res = ch->r_io[ATA_FEATURE].res;
    ch->r_io[ATA_ERROR].offset = ch->r_io[ATA_FEATURE].offset;
    ch->r_io[ATA_IREASON].res = ch->r_io[ATA_COUNT].res;
    ch->r_io[ATA_IREASON].offset = ch->r_io[ATA_COUNT].offset;
    ch->r_io[ATA_STATUS].res = ch->r_io[ATA_COMMAND].res;
    ch->r_io[ATA_STATUS].offset = ch->r_io[ATA_COMMAND].offset;
    ch->r_io[ATA_ALTSTAT].res = ch->r_io[ATA_CONTROL].res;
    ch->r_io[ATA_ALTSTAT].offset = ch->r_io[ATA_CONTROL].offset;
}

void
ata_modify_if_48bit(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);

    atadev->flags &= ~ATA_D_48BIT_ACTIVE;

    if ((request->u.ata.lba + request->u.ata.count >= ATA_MAX_28BIT_LBA ||
	 request->u.ata.count > 256) &&
	atadev->param.support.command2 & ATA_SUPPORT_ADDRESS48) {

	/* translate command into 48bit version */
	switch (request->u.ata.command) {
	case ATA_READ:
	    request->u.ata.command = ATA_READ48;
	    break;
	case ATA_READ_MUL:
	    request->u.ata.command = ATA_READ_MUL48;
	    break;
	case ATA_READ_DMA:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_READ_MUL48;
		else
		    request->u.ata.command = ATA_READ48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_READ_DMA48;
	    break;
	case ATA_READ_DMA_QUEUED:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_READ_MUL48;
		else
		    request->u.ata.command = ATA_READ48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_READ_DMA_QUEUED48;
	    break;
	case ATA_WRITE:
	    request->u.ata.command = ATA_WRITE48;
	    break;
	case ATA_WRITE_MUL:
	    request->u.ata.command = ATA_WRITE_MUL48;
	    break;
	case ATA_WRITE_DMA:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_WRITE_MUL48;
		else
		    request->u.ata.command = ATA_WRITE48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_WRITE_DMA48;
	    break;
	case ATA_WRITE_DMA_QUEUED:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_WRITE_MUL48;
		else
		    request->u.ata.command = ATA_WRITE48;
		request->u.ata.command = ATA_WRITE48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_WRITE_DMA_QUEUED48;
	    break;
	case ATA_FLUSHCACHE:
	    request->u.ata.command = ATA_FLUSHCACHE48;
	    break;
	case ATA_READ_NATIVE_MAX_ADDDRESS:
	    request->u.ata.command = ATA_READ_NATIVE_MAX_ADDDRESS48;
	    break;
	case ATA_SET_MAX_ADDRESS:
	    request->u.ata.command = ATA_SET_MAX_ADDRESS48;
	    break;
	default:
	    return;
	}
	atadev->flags |= ATA_D_48BIT_ACTIVE;
    }
}

void
ata_udelay(int interval)
{
    /*
     * We can't use tsleep here, because we might be called from callout
     * context.
     */
    if (1 || interval < (1000000/hz))
	DELAY(interval);
    else
	tsleep(&interval, 0, "ataslp", 1 + interval / (1000000 / hz));
}

char *
ata_mode2str(int mode)
{
    switch (mode) {
    case -1: return "UNSUPPORTED";
    case ATA_PIO0: return "PIO0";
    case ATA_PIO1: return "PIO1";
    case ATA_PIO2: return "PIO2";
    case ATA_PIO3: return "PIO3";
    case ATA_PIO4: return "PIO4";
    case ATA_WDMA0: return "WDMA0";
    case ATA_WDMA1: return "WDMA1";
    case ATA_WDMA2: return "WDMA2";
    case ATA_UDMA0: return "UDMA16";
    case ATA_UDMA1: return "UDMA25";
    case ATA_UDMA2: return "UDMA33";
    case ATA_UDMA3: return "UDMA40";
    case ATA_UDMA4: return "UDMA66";
    case ATA_UDMA5: return "UDMA100";
    case ATA_UDMA6: return "UDMA133";
    case ATA_SA150: return "SATA150";
    case ATA_SA300: return "SATA300";
    case ATA_USB: return "USB";
    case ATA_USB1: return "USB1";
    case ATA_USB2: return "USB2";
    default:
	if (mode & ATA_DMA_MASK)
	    return "BIOSDMA";
	else
	    return "BIOSPIO";
    }
}

int
ata_pmode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_64_70) {
	if (ap->apiomodes & 0x02)
	    return ATA_PIO4;
	if (ap->apiomodes & 0x01)
	    return ATA_PIO3;
    }
    if (ap->mwdmamodes & 0x04)
	return ATA_PIO4;
    if (ap->mwdmamodes & 0x02)
	return ATA_PIO3;
    if (ap->mwdmamodes & 0x01)
	return ATA_PIO2;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x200)
	return ATA_PIO2;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x100)
	return ATA_PIO1;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x000)
	return ATA_PIO0;
    return ATA_PIO0;
}

int
ata_wmode(struct ata_params *ap)
{
    if (ap->mwdmamodes & 0x04)
	return ATA_WDMA2;
    if (ap->mwdmamodes & 0x02)
	return ATA_WDMA1;
    if (ap->mwdmamodes & 0x01)
	return ATA_WDMA0;
    return -1;
}

int
ata_umode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_88) {
	if (ap->udmamodes & 0x40)
	    return ATA_UDMA6;
	if (ap->udmamodes & 0x20)
	    return ATA_UDMA5;
	if (ap->udmamodes & 0x10)
	    return ATA_UDMA4;
	if (ap->udmamodes & 0x08)
	    return ATA_UDMA3;
	if (ap->udmamodes & 0x04)
	    return ATA_UDMA2;
	if (ap->udmamodes & 0x02)
	    return ATA_UDMA1;
	if (ap->udmamodes & 0x01)
	    return ATA_UDMA0;
    }
    return -1;
}

int
ata_limit_mode(device_t dev, int mode, int maxmode)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (maxmode && mode > maxmode)
	mode = maxmode;

    if (mode >= ATA_UDMA0 && ata_umode(&atadev->param) > 0)
	return min(mode, ata_umode(&atadev->param));

    if (mode >= ATA_WDMA0 && ata_wmode(&atadev->param) > 0)
	return min(mode, ata_wmode(&atadev->param));

    if (mode > ata_pmode(&atadev->param))
	return min(mode, ata_pmode(&atadev->param));

    return mode;
}

static void
bswap(int8_t *buf, int len)
{
    u_int16_t *ptr = (u_int16_t*)(buf + len);

    while (--ptr >= (u_int16_t*)buf)
	*ptr = ntohs(*ptr);
}

static void
btrim(int8_t *buf, int len)
{
    int8_t *ptr;

    for (ptr = buf; ptr < buf+len; ++ptr)
	if (!*ptr || *ptr == '_')
	    *ptr = ' ';
    for (ptr = buf + len - 1; ptr >= buf && *ptr == ' '; --ptr)
	*ptr = 0;
}

static void
bpack(int8_t *src, int8_t *dst, int len)
{
    int i, j, blank;

    for (i = j = blank = 0 ; i < len; i++) {
	if (blank && src[i] == ' ') continue;
	if (blank && src[i] != ' ') {
	    dst[j++] = src[i];
	    blank = 0;
	    continue;
	}
	if (src[i] == ' ') {
	    blank = 1;
	    if (i == 0)
		continue;
	}
	dst[j++] = src[i];
    }
    if (j < len)
	dst[j] = 0x00;
}


/*
 * module handeling
 */
static int
ata_module_event_handler(module_t mod, int what, void *arg)
{
    /* static because we need the reference at destruction time */
    static cdev_t atacdev;

    switch (what) {
    case MOD_LOAD:
	/* register controlling device */
	atacdev = make_dev(&ata_ops, 0, UID_ROOT, GID_OPERATOR, 0600, "ata");
	reference_dev(atacdev);
	return 0;

    case MOD_UNLOAD:
	/* deregister controlling device */
	destroy_dev(atacdev);
	dev_ops_remove_all(&ata_ops);
	return 0;

    default:
	return EOPNOTSUPP;
    }
}

static moduledata_t ata_moduledata = { "ata", ata_module_event_handler, NULL };
DECLARE_MODULE(ata, ata_moduledata, SI_SUB_CONFIGURE, SI_ORDER_SECOND);
MODULE_VERSION(ata, 1);

/*
 * Construct a completely zero'ed ata_request. On objcache_put(), an
 * ata_request object is also zero'ed, so objcache_get() is guaranteed to give
 * completely zero'ed objects without spending too much time.
 */
static boolean_t
ata_request_cache_ctor(void *obj, void *private, int ocflags)
{
    struct ata_request *arp = obj;

    bzero(arp, sizeof(struct ata_request));
    return(TRUE);
}

/*
 * Construct a completely zero'ed ata_composite. On objcache_put(), an
 * ata_composite object is also zero'ed, so objcache_get() is guaranteed to give
 * completely zero'ed objects without spending too much time.
 */
static boolean_t
ata_composite_cache_ctor(void *obj, void *private, int ocflags)
{
    struct ata_composite *acp = obj;

    bzero(acp, sizeof(struct ata_composite));
    return(TRUE);
}

static void
ata_init(void)
{
    ata_request_cache = objcache_create("ata_request", 0, 0,
					ata_request_cache_ctor, NULL, NULL,
					objcache_malloc_alloc,
					objcache_malloc_free,
					&ata_request_malloc_args);
    ata_composite_cache = objcache_create("ata_composite", 0, 0,
					  ata_composite_cache_ctor, NULL, NULL,
					  objcache_malloc_alloc,
					  objcache_malloc_free,
					  &ata_composite_malloc_args);
}
SYSINIT(ata_register, SI_SUB_DRIVERS, SI_ORDER_SECOND, ata_init, NULL);

static void
ata_uninit(void)
{
    objcache_destroy(ata_composite_cache);
    objcache_destroy(ata_request_cache);
}
SYSUNINIT(ata_unregister, SI_SUB_DRIVERS, SI_ORDER_SECOND, ata_uninit, NULL);
