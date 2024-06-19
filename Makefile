# Thanks to https://stackoverflow.com/a/18258352
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

main.out: src/main.cpp src/extras.h $(call rwildcard,src/fs,*.h)
	g++ src/main.cpp -g -o main.out