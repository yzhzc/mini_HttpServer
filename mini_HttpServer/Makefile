bin=httpserver
cgi=test_cgi
cc=g++
LD_FLAGS=-std=c++11 -lpthread
curr=$(shell pwd)	# 获取当前目录
src=main.cc

.PHONY:all
all:$(bin) $(cgi)

$(bin):$(src)
	$(cc) $^ -o $@ $(LD_FLAGS)

$(cgi):cgi/test_cgi.cc
	$(cc) $^ -o $@ -std=c++11

.PHONY:clean
clean:
	rm $(bin) $(cgi)

.PHONY:output
output:
	mkdir -p output
	cp $(bin) output
	cp -rf wwwroot output
	cp $(cgi) output/wwwroot