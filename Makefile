CC=g++ -O3 -std=c++14
SRCS=$(wildcard *.cpp */*.cpp)
OBJS=$(patsubst %.cpp, %.o, $(SRCS))

# # for MacOs
# INCLUDE = -I/usr/local/include/libtorch/include -I/usr/local/include/libtorch/include/torch/csrc/api/include
# LIB +=-L/usr/local/include/libtorch/lib -ltorch -lc10 -lpthread 
# FLAG = -Xlinker -rpath -Xlinker /usr/local/include/libtorch/lib

TYPE = CPU
LIBTORCH_PREFIX = /usr/local/libtorch
LIBTORCH_CPU_PREFIX=/usr/local/libtorch_1_4_0_cpu

#TYPE = GPU
# for linux
ifeq ($(TYPE), GPU)
	INCLUDE = -I$(LIBTORCH_PREFIX)/include -I$(LIBTORCH_PREFIX)/include/torch/csrc/api/include
	LIB +=-L$(LIBTORCH_PREFIX)/lib   -ltorch -lc10  -lpthread
	FLAG = -Wl,-rpath=$(LIBTORCH_PREFIX)/lib
else
	INCLUDE = -I$(LIBTORCH_CPU_PREFIX)/include -I$(LIBTORCH_CPU_PREFIX)/include/torch/csrc/api/include
	LIB +=-L$(LIBTORCH_CPU_PREFIX)/lib -ltorch -lc10 -lpthread
	FLAG = -Wl,-rpath=$(LIBTORCH_CPU_PREFIX)/lib
endif

# INCLUDE = -I/home/liuguanli/Documents/libtorch/include -I/home/liuguanli/Documents/libtorch/include/torch/csrc/api/include
# LIB +=-L/home/liuguanli/Documents/libtorch/lib -ltorch -lc10 -lpthread
# FLAG = -Wl,-rpath=/home/liuguanli/Documents/libtorch/lib

NAME=$(wildcard *.cpp)
TARGET=$(patsubst %.cpp, %, $(NAME))

$(TARGET):$(OBJS)
	$(CC) -o $@ $^ $(INCLUDE) $(LIB) $(FLAG)
%.o:%.cpp
	$(CC) -o $@ -c $< -g $(INCLUDE)

clean:
	rm -rf $(TARGET) $(OBJS)

# # g++ -std=c++11 Exp.cpp FileReader.o -ltensorflow -o Exp_tf
