# SPDX-License-Identifier: GPL-2.0
cx231xx-y += cx231xx-video.o cx231xx-i2c.o cx231xx-cards.o cx231xx-core.o
cx231xx-y += cx231xx-avcore.o cx231xx-417.o cx231xx-pcb-cfg.o cx231xx-vbi.o
cx231xx-$(CONFIG_VIDEO_CX231XX_RC) += cx231xx-input.o

cx231xx-alsa-objs := cx231xx-audio.o

obj-$(CONFIG_VIDEO_CX231XX) += cx231xx.o
obj-$(CONFIG_VIDEO_CX231XX_ALSA) += cx231xx-alsa.o
obj-$(CONFIG_VIDEO_CX231XX_DVB) += cx231xx-dvb-ci.o
cx231xx-dvb-ci-objs += cx231xx-dvb.o tbscxci.o

ccflags-y += -I$(KBUILD_EXTMOD)/tuners
ccflags-y += -I$(KBUILD_EXTMOD)/frontends
