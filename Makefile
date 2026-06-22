# ============================================================
#  net_chat — TCP 聊天服务端
# ============================================================

CC      = gcc
TARGET  = build/output/net_chat_server

# 目录
SRCDIR  = src
INCDIR  = inc
BUILDDIR = build

# 源文件（根目录 main.c + src/ 下所有 .c）
MAIN_SRC = main.c
SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(patsubst %.c,$(BUILDDIR)/%.o,$(MAIN_SRC) $(SRCS))
DEPS     = $(OBJS:.o=.d)

# 编译选项
CFLAGS   = -Wall -Wextra -g -O2 -I$(INCDIR)
LDFLAGS  =

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "[OK] $(TARGET) 编译完成"

# 编译 .c → build/*.o，同时生成 .d 依赖文件
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR)/$(SRCDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# 引入自动生成的依赖
-include $(DEPS)

.PHONY: clean run

clean:
	rm -rf $(BUILDDIR)
	@echo "[OK] 清理完成"

run: $(TARGET)
	./$(TARGET)
