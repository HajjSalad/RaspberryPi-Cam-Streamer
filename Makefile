# obj-m += cam_stream.o

# all:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# clean:
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# --- Kernel Module ---
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# --- User Program ---
USER_PROG := camera_client
USER_SRC := $(shell find ./src -name "*.c" -o -name "*.cpp")
USER_INC := -I$(PWD)/src -I$(PWD)/kernel

USER_CFLAGS += -std=c++17
USER_LIBS   += -ltensorflow-lite -ljpeg

all: module user

# Build kernel module
module:
	$(MAKE) -C $(KDIR) M=$(PWD)/kernel modules

# Build user-space program
user:
	@echo "Building user-space program..."
	g++ $(USER_CFLAGS) $(USER_SRC) $(USER_INC) -o $(USER_PROG) $(USER_LIBS)

# Clean both kernel and user builds
clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/kernel clean
	rm -f $(USER_PROG)
