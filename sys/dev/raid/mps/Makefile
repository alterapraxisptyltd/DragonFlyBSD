# $FreeBSD: src/sys/modules/mps/Makefile,v 1.4 2012/01/26 18:17:21 ken Exp $

KMOD=	mps
SRCS=	mps_pci.c mps.c mps_sas.c mps_table.c mps_user.c
SRCS+=	mps_config.c mps_mapping.c mps_sas_lsi.c
SRCS+=	opt_cam.h opt_compat.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS += -DMPS_DEBUG

.include <bsd.kmod.mk>
