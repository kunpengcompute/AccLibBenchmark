FOLLY_HOME := /home/g00570279/ins/folly_POC
# 新增 fmt 的安装根目录
FMT_HOME   := /home/g00570279/folly/build/installed/fmt

# 加上 fmt 的头文件路径（以防后续需要）
INC      := -I$(FOLLY_HOME)/include -I$(FMT_HOME)/include

# 增加 -lfmt，放到 -lfolly 后面
LIBS     := -lpthread -lfolly -lfmt -levent -lglog -lgflags

# 在 -L 和 -rpath 中加入 fmt 的 lib 目录
LDFLAGS  := -L$(FOLLY_HOME)/lib -Wl,-rpath,$(FOLLY_HOME)/lib \
            -L$(FMT_HOME)/lib -Wl,-rpath,$(FMT_HOME)/lib

# 修正了之前的拼写错误 CPPCLAGS -> CXXFLAGS
CXXFLAGS := -O3 -std=c++17
OBJS     := net-client-ori
CXX      := g++

all: $(OBJS)

net-client-ori: net-client-ori.cpp request-ori.h
	$(CXX) $(CXXFLAGS) $(INC) -o $@ $< $(LDFLAGS) $(LIBS) -DCHECK_RESPONSE

clean:
	rm -f $(OBJS)
