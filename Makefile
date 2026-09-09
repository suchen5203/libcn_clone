# Makefile - libcn_clone.so 编译(32-bit Linux ELF)
#
# 默认:gcc -m32 -fPIC -shared
# 需要 32 位 libc(在 64 位 Linux 上:apt install gcc-multilib)
# 需要 OpenSSL 开发包(可选,见下方注释)
#
# 用 mingw-w64 在 Windows 交叉编译:见 cross-make.sh
# 用 Docker:docker build -t libcn_clone .

CC       ?= gcc
CFLAGS   ?= -m32 -fPIC -O2 -Wall -Wextra -std=gnu99 -D_GNU_SOURCE
LDFLAGS  ?= -m32 -shared -fPIC
LDLIBS   ?=

# 是否启用 OpenSSL AES(0 = 用 XOR 占位,1 = 用真 AES)
USE_OPENSSL ?= 1

ifeq ($(USE_OPENSSL),1)
CFLAGS  += -DOpenSSL=1
LDLIBS  += -lcrypto
else
CFLAGS  += -DOpenSSL=0
endif

SRCS = main.c hooks.c config.c resource.c keycheck.c
OBJS = $(SRCS:.c=.o)
TARGET = libcn_clone.so

.PHONY: all clean check

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check:
	@echo "=== 编译环境检查 ==="
	@$(CC) --version | head -1
	@echo "目标:" $(TARGET)
	@echo "CFLAGS:" $(CFLAGS)
	@echo "LDFLAGS:" $(LDFLAGS)
	@file $(TARGET) 2>/dev/null || echo "(未编译)"

clean:
	rm -f $(OBJS) $(TARGET)