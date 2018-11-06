snd-soc-adau19xx-objs := adau19xx.o
snd-soc-adau19xx-objs += adau19xx-i2c.o
snd-soc-adau19xx-objs += adau19xx-debug.o

obj-m += snd-soc-adau19xx.o

PWD = $(shell pwd)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	sudo cp snd-soc-adau19xx.ko /lib/modules/$(shell uname -r)/kernel/sound/soc/codecs/
	sudo depmod -a
