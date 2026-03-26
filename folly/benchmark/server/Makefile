# 定义 fmt 的根目录
FMT_HOME := /home/g00570279/folly/build/installed/fmt

# 在原有的 INC 后面加上 fmt 的 include 目录
INC      := -I/home/g00570279/ins/folly_POC/include -I$(FMT_HOME)/include

# LIBS 保持不变 (注意你之前已经加上了 -lfmt)
LIBS     := -lpthread -lfolly -lfmt -levent -luring -lglog -lgflags -ldouble-conversion -lz -ldl -liberty -DENABLE_VLOG=1  -DGOOGLE_GLOG_DLL_DECL= -O3

# 在 LDFLAGS 中添加 folly 和 fmt 的 -L 路径和 -rpath 运行时路径
LDFLAGS  := -L/home/g00570279/ins/folly_POC/lib -Wl,-rpath,/home/g00570279/ins/folly_POC/lib \
            -L$(FMT_HOME)/lib -Wl,-rpath,$(FMT_HOME)/lib

CPPCLAGS := -std=c++17 -O3
#-fsanitize=address,alignment -fno-sanitize-recover=alignment -fno-omit-frame-pointer -g
OBJS     := net-server3-iouring net-server3-iouring-write net-server3
CXX      := g++

all: $(OBJS)

net-server3-iouring-write: net-server3_IOuring.cpp
	$(CXX) $(CPPCLAGS) $(INC) -o $@ $^ $(LDFLAGS) $(LIBS) -DUSING_WRITE

net-server3-iouring: net-server3_IOuring.cpp
	$(CXX) $(CPPCLAGS) $(INC) -o $@ $^ $(LDFLAGS) $(LIBS)

net-server3: net-server3.cpp
	$(CXX) $(CPPCLAGS) $(INC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS)
