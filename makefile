CXXFLAGS = -std=c++23 -g -MMD -MP $(shell pkg-config --cflags libpipewire-0.3 libspa-0.2 libportal)
LDFLAGS = $(shell pkg-config --libs libpipewire-0.3 libspa-0.2 libportal) -lSDL3 -lobs \
          -lavdevice -lavformat -lavcodec -lswscale -lavutil -lenet -lm \
          -lpthread -lwayland-client -lminiupnpc

SRCS = $(wildcard src/*.cc)
OBJS = $(patsubst src/%.cc, obj/%.o, $(SRCS))

out obj:
	mkdir -p $@

obj/%.o: src/%.cc | obj
	g++ $(CXXFLAGS) -c $< -o $@

out/ss: $(OBJS) | out
	g++ $(OBJS) -o $@ $(LDFLAGS)
	:
run: out/ss 
	./out/ss

clean:
	rm -rf obj out

