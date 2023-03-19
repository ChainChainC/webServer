TARGET=lib/libreactor.a
CXX=g++
CFLAGS= -g -O0 -Wall -fPIC -Wno-deprecated

SRC=./src
INC=-I./include -pthread
# wildcard 对于通配符*.cpp，如果没有wildcard，那么本质上和宏类似，使用wildcard能够使其全部扩展开
# basename 取前缀函数 addsuffix 加后缀函数
# OBJS= $(addsuffix .o, $(basename $(wildcard $(SRC)/*.cpp)))
OBJS = $(addprefix ./obj/, $(addsuffix .o, $(basename $(notdir $(wildcard $(SRC)/*.cpp)))))

O_PATH = ./obj

$(info vbs="$(OBJS)")

# @$目标文件完整名称 @^所有不重复目标文件
$(TARGET): $(OBJS)
	mkdir -p lib
	# 该语句是将多个.o文件链接为.a文件
	ar cqs $@ $^

# $< 第一个目标依赖名称
$(O_PATH)/%.o: $(SRC)/%.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

.PHONY: clean

clean:
	-rm -f src/*.o $(TARGET)