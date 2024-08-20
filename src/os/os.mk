SRCS:=$(SRCS) src/os/time.cpp \
              src/os/memory.cpp

include src/os/linux/linux.mk
