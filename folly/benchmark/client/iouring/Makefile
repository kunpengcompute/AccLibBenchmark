FOLLY_HOME := /home/xxx/ins/folly_POC

FMT_HOME   := /home/xxx/folly/build/installed/fmt

# 加上 fmt 的头文件路径（以防后续需要）
INC      := -I$(FOLLY_HOME)/include -I$(FMT_HOME)/include

# 增加 -lfmt，放到 -lfolly 后面
LIBS     := -lpthread -lfolly -lfmt -levent -lglog -lgflags

# 在 -L 和 -rpath 中加入 fmt 的 lib 目录
LDFLAGS  := -L$(FOLLY_HOME)/lib -Wl,-rpath,$(FOLLY_HOME)/lib \
            -L$(FMT_HOME)/lib -Wl,-rpath,$(FMT_HOME)/lib


CPPCLAGS := -O3 -g -std=c++17
OBJS     := net-client
CXX      := g++

all: $(OBJS)

net-client: net-client-with-bs.cpp request.h
	$(CXX) $(CPPCLAGS) $(INC) -o $@ $< $(LDFLAGS) $(LIBS) -DCHECK_RESPONSE

clean:
	rm -f $(OBJS)
