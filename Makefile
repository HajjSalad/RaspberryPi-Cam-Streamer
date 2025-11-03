# obj-m += cam_stream.o

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# --- Kernel Module ---
obj-m := cam_stream.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# --- User Program ---
USER_PROG := camera_client
USER_SRC := camera_client.c

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	@echo "Building user-space program..."
	gcc $(USER_SRC) -o $(USER_PROG)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_PROG)
