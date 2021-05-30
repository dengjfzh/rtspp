TARGET := rtspp
BUILD_DIR := ./build
SRC_DIRS := ./src


# 所有外部依赖写在这里
INCFLAGS := -I/usr/include/mysql \
            -I3rdparty/served_v1.6.0/include \
            -I3rdparty/spdlog_v1.8.1/include \
            -I3rdparty/rapidjson/include \
            -I3rdparty/ffmpeg_n4.1_flvhevc/include \
            -I3rdparty/aws-sdk-cpp_v1.8.112/include \
            -I3rdparty/hiredis_v1.0.0/include \
            -I3rdparty/cppkafka_5e4b350/include \
            -I3rdparty/librdkafka_v1.5.3/include

LDFLAGS := -rdynamic

LDLIBS := -lcurl

SRCS := $(shell find $(SRC_DIRS) \( -name "*.cpp" -or -name "*.c" -or -name "*.s" \) -and -not -name ".*")
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
GIT_VERSION := "$(shell git describe --dirty --always --tags)"
GIT_DATE := "$(shell git show -s --pretty='%ai')"

# 将src目录下的所有子目录加入头文件搜索路径
INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INCFLAGS := $(INCFLAGS) $(addprefix -I,$(INC_DIRS))

# 预处理选项，加入-MMD -MP是为了生成依赖文件
override CPPFLAGS += $(INCFLAGS) -MMD -MP -DGIT_VERSION=\"$(GIT_VERSION)\" -DGIT_DATE=\"$(GIT_DATE)\"

# 编译选项，C11/C++11必选
CFLAGS := -g3 -O0 -Wall
CXXFLAGS := -g3 -O0 -Wall
override CFLAGS += -std=c11
override CXXFLAGS += -std=c++11

.PHONY: all

all: $(BUILD_DIR)/$(TARGET)


$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) -o $@

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


PREFIX ?= /usr/local

.PHONY: install uninstall

install: all
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BUILD_DIR)/$(TARGET) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(TARGET)


# 这个dummy文件是一个辅助，用来实现当makefile被修改时，重新编译整个项目的目的
-include dummy
dummy: Makefile
	@touch $@
	@$(MAKE) -s clean


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

# 包含头文件依赖规则
-include $(DEPS)


MKDIR_P ?= mkdir -p
CP ?= cp
