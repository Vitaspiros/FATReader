# Thanks to rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

main.out: src/main.cpp $(call rwildcard,src/fs,*.h)
	g++ src/main.cpp -g -o main.out