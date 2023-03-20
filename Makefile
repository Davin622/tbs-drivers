kernelver ?= $(shell uname -r)
KDIR	?= /lib/modules/$(kernelver)/build
KMEDIADIR	?= /lib/modules/$(kernelver)/kernel/drivers/media
PWD	:= $(shell pwd)

MODDEFS := CONFIG_DVB_CORE=m CONFIG_DVB_TBSECP3=m CONFIG_TBS_PCIE_CI=m CONFIG_TBS_PCIE_MOD=m CONFIG_SAA716X_CORE=m CONFIG_DVB_SAA716X_TBS=m CONFIG_DVB_SAA716X_HYBRID=m CONFIG_DVB_TAS2971=m  CONFIG_DVB_SI2168=m CONFIG_DVB_TAS2101=m CONFIG_DVB_GX1133=m CONFIG_DVB_SI2183=m CONFIG_DVB_CXD2878=m CONFIG_DVB_STV090x=m CONFIG_DVB_STV6110x=m CONFIG_DVB_STV091X=m CONFIG_DVB_TBSPRIV=m CONFIG_DVB_STID135=m CONFIG_DVB_MN88436=m CONFIG_DVB_M88RS6060=m CONFIG_DVB_STV0910=m CONFIG_DVB_STV6111=m CONFIG_DVB_CX24117=m CONFIG_DVB_MXL58X=m CONFIG_DVB_MXL5XX=m CONFIG_MEDIA_TUNER_SI2157=m CONFIG_DVB_GX1503=m CONFIG_DVB_MTV23X=m CONFIG_MEDIA_TUNER_RDA5816=m CONFIG_MEDIA_TUNER_AV201X=m CONFIG_MEDIA_TUNER_STV6120=m CONFIG_DVB_USB=m  CONFIG_DVB_USB_TBS5520=m CONFIG_DVB_USB_TBS5301=m CONFIG_DVB_USB_TBS5520se=m CONFIG_DVB_USB_TBS5580=m CONFIG_DVB_USB_TBS5590=m CONFIG_DVB_USB_TBS5880=m CONFIG_DVB_USB_TBS5881=m CONFIG_DVB_USB_TBS5922se=m CONFIG_DVB_USB_TBS5925=m CONFIG_DVB_USB_TBS5927=m CONFIG_DVB_USB_TBS5930=m  CONFIG_DVB_NET=y

KBUILD_EXTMOD = $(PWD)

TBSDVB_INC = "-I$(KBUILD_EXTMOD)/include -I$(KBUILD_EXTMOD)/include/linux -I$(KBUILD_EXTMOD)/dvb-core -I$(KBUILD_EXTMOD)/frontends -I$(KBUILD_EXTMOD)/stid135 -I$(KBUILD_EXTMOD)/tuners"


all: 
	$(MAKE) -C $(KDIR) KBUILD_EXTMOD=$(PWD) $(MODDEFS) modules NOSTDINC_FLAGS=$(TBSDVB_INC)

dep:
	DIR=`pwd`; (cd $(TOPDIR); make KBUILD_EXTMOD=$$DIR dep)

install: all
	cp $(KBUILD_EXTMOD)/dvb-core/*.ko  ${KMEDIADIR}/dvb-core
	
	cp $(KBUILD_EXTMOD)/frontends/*.ko  ${KMEDIADIR}/dvb-frontends
	cp $(KBUILD_EXTMOD)/tuners/*.ko  ${KMEDIADIR}/tuners
	
	$(shell mkdir  -p ${KMEDIADIR}/dvb-frontends/stid135)
	cp $(KBUILD_EXTMOD)/frontends/stid135/*.ko  ${KMEDIADIR}/dvb-frontends/stid135
	
	$(shell mkdir -p "${KMEDIADIR}/pci/tbsecp3")		
	cp $(KBUILD_EXTMOD)/tbsecp3/*.ko  /lib/modules/$(kernelver)/kernel/drivers/media/pci/tbsecp3
	
	$(shell mkdir -p "/lib/modules/$(kernelver)/kernel/drivers/media/pci/tbsci")
	cp $(KBUILD_EXTMOD)/tbsci/*.ko  ${KMEDIADIR}/pci/tbsci
	
	#test = $(shell if [ -d "${KMEDIADIR}/pci/tbsmod" ]; then echo "exist"; else echo "noexist"; fi)
	#ifeq ("$(test)", "noexist")
	$(shell mkdir -p "${KMEDIADIR}/pci/tbsmod")
	#endif	
	cp $(KBUILD_EXTMOD)/tbsmod/*.ko  ${KMEDIADIR}/pci/tbsmod

	$(shell mkdir -p "${KMEDIADIR}/pci/saa716x")
	cp $(KBUILD_EXTMOD)/saa716x/*.ko  ${KMEDIADIR}/pci/saa716x
	
	cp $(KBUILD_EXTMOD)/usb/dvb-usb/dvb-usb*.ko ${KMEDIADIR}/usb/dvb-usb
	
	depmod $(kernelver)

clean:
	rm -rf */.*.o.d */*.o */*.ko */*.mod */*.mod.c */.*.cmd */*.order */*.dwo .tmp_versions Module* modules* .*odule* frontends/stid135/*.o frontends/stid135/*.ko frontends/stid135/*.mod* frontends/stid135/.*.cmd usb/dvb-usb/*.o usb/dvb-usb/*.ko usb/dvb-usb/*.mod* usb/dvb-usb/.*.cmd usb/dvb-usb/.*.o.d usb/cx231xx/*.o usb/cx231xx/.*.o.* usb/cx231xx/*.ko usb/cx231xx/*.mod* usb/cx231xx/.*.cmd saa716x/*.o saa716x/*.ko saa716x/*.mod* saa716x/.*.cmd 


