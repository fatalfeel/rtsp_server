TARGET1 = rtsp_server
TARGET2 = rtsp_pusher
TARGET3 = rtsp_h264_file
TARGET4 = rtsp_h265_file
TARGET5 = rtsp_aac_file

CROSS_COMPILE =
#CROSS_COMPILE = /opt/ivot/arm-ca9-linux-gnueabihf-6.5/bin/arm-ca9-linux-gnueabihf-
#CROSS_COMPILE = /opt/ivot/aarch64-ca53-linux-gnueabihf-8.4/bin/aarch64-ca53-linux-gnu-
CXX = $(CROSS_COMPILE)g++
INC	= -I$(shell pwd)/src/ -I$(shell pwd)/src/net -I$(shell pwd)/src/xop -I$(shell pwd)/src/bsalgo -I$(shell pwd)/src/3rdpart

#CXXFLAGS = -O0 -g -fPIC -pthread -fmessage-length=0 -std=c++14
CXXFLAGS 	= -O3 -g -fPIC -pthread -fmessage-length=0 -std=c++14
LDFLAGS		= -ldl -lm -lrt -lpthread
COMPILE_DIR	= ./objs

$(COMPILE_DIR)/%.o : ./src/bsalgo/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c  $< -o  $@
$(COMPILE_DIR)/%.o : ./src/net/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c  $< -o  $@
$(COMPILE_DIR)/%.o : ./src/xop/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c  $< -o  $@
$(COMPILE_DIR)/%.o : ./example/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c  $< -o  $@

SRC1  = $(notdir $(wildcard ./src/net/*.cpp))
OBJS1 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC1))

SRC2  = $(notdir $(wildcard ./src/xop/*.cpp))
OBJS2 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC2))

SRC3  = $(notdir $(wildcard ./src/bsalgo/*.cpp))
OBJS3 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC3))

SRC4  = $(notdir $(wildcard ./example/rtsp_server.cpp))
OBJS4 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC4))

SRC5  = $(notdir $(wildcard ./example/rtsp_pusher.cpp))
OBJS5 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC5))

SRC6  = $(notdir $(wildcard ./example/rtsp_h264_file.cpp))
OBJS6 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC6))

SRC7  = $(notdir $(wildcard ./example/rtsp_h265_file.cpp))
OBJS7 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC7))

SRC8  = $(notdir $(wildcard ./example/rtsp_aac_file.cpp))
OBJS8 = $(patsubst %.cpp,$(COMPILE_DIR)/%.o,$(SRC8))

$(TARGET1) : $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS4)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
$(TARGET2) : $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS5)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
$(TARGET3) : $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS6)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
$(TARGET4) : $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS7)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
$(TARGET5) : $(OBJS1) $(OBJS2) $(OBJS3) $(OBJS8)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)		
	
all: $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET5)
		
clean:
	rm -rf $(COMPILE_DIR) $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET5)
