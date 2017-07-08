
CC = arm-openwrt-linux-g++
LD = arm-openwrt-linux-g++

TARGET = wakeWordAgent

.PHONY: all clean

all: $(TARGET)

$(TARGET):
	arm-openwrt-linux-g++ -I$(STAGING_DIR)/include -I./ -L$(STAGING_DIR)/lib -std=c++11 *.cpp libthf.a -o wakeWordAgent -lm -lpulse -lpulse-simple -lpulsecommon-10.0 -ljson-c -lwrap -lsndfile -lcap -lasound
clean:
	@rm -f $(TARGET) *.o
