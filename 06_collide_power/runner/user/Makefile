PWD := $(shell pwd)
DIR := $(shell dirname $(PWD))
NAME := $(shell basename $(DIR))

CC ?= g++-10

all: main_ pf_

main_:
	clang++ -stdlib=libc++ -std=c++20 main.cpp -O3 -g3  -Wall -Wno-unused-function -o main -DNAME="$(NAME)" -L../../common/ -pthread -lrt

pf_:
	make -C ../../pf

clean:
	-rm -rf main
	-make -C ../../pf clean

huge:
	sudo mount -t hugetlbfs -o pagesize=1G none /mnt/hugepage 
