TARGET0 = rtsp_server
TARGET1 = rtsp_pusher
TARGET2 = rtsp_h264_file
TARGET3 = rtsp_h265_file
TARGET4 = rtsp_aac_file

CROSS_COMPILE =
#CROSS_COMPILE = /opt/ivot/arm-ca9-linux-gnueabihf-6.5/bin/arm-ca9-linux-gnueabihf-
CXX = $(CROSS_COMPILE)g++

CXXFLAGS	 = -I./src
CXXFLAGS	+= -I./src/3rdpart
CXXFLAGS	+= -I./src/bsalgo
CXXFLAGS	+= -I./src/net
CXXFLAGS	+= -I./src/xop
#CXXFLAGS   += -O0 -g -fPIC -pthread -fmessage-length=0 -std=c++14
CXXFLAGS	+= -O3 -g -fPIC -pthread -fmessage-length=0 -std=c++14
LDFLAGS		 = -ldl -lm -lrt -lpthread
OBJDIR		 = ./objs

$(shell mkdir -p $(OBJDIR))

$(OBJDIR)/%.o: ./src/bsalgo/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(OBJDIR)/%.o: ./src/net/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
	
$(OBJDIR)/%.o: ./src/xop/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
	
$(OBJDIR)/%.o: ./example/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@	

CXXFILES0	= $(notdir $(wildcard ./src/net/*.cpp))
CXXOBJS0	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES0))

CXXFILES1	= $(notdir $(wildcard ./src/xop/*.cpp))
CXXOBJS1	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES1))

CXXFILES2	= $(notdir $(wildcard ./src/bsalgo/*.cpp))
CXXOBJS2	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES2))

CXXFILES3	= $(notdir $(wildcard ./example/rtsp_server.cpp))
CXXOBJS3	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES3))

CXXFILES4	= $(notdir $(wildcard ./example/rtsp_pusher.cpp))
CXXOBJS4	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES4))

CXXFILES5	= $(notdir $(wildcard ./example/rtsp_h264_file.cpp))
CXXOBJS5	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES5))

CXXFILES6	= $(notdir $(wildcard ./example/rtsp_h265_file.cpp))
CXXOBJS6	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES6))

CXXFILES7	= $(notdir $(wildcard ./example/rtsp_aac_file.cpp))
CXXOBJS7	= $(patsubst %.cpp, $(OBJDIR)/%.o, $(CXXFILES7))

$(TARGET0): $(CXXOBJS0) $(CXXOBJS1) $(CXXOBJS2) $(CXXOBJS3)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(TARGET1): $(CXXOBJS0) $(CXXOBJS1) $(CXXOBJS2) $(CXXOBJS4)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(TARGET2): $(CXXOBJS0) $(CXXOBJS1) $(CXXOBJS2) $(CXXOBJS5)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(TARGET3): $(CXXOBJS0) $(CXXOBJS1) $(CXXOBJS2) $(CXXOBJS6)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(TARGET4): $(CXXOBJS0) $(CXXOBJS1) $(CXXOBJS2) $(CXXOBJS7)
	$(CXX) $^ -o $@ $(LDFLAGS)		
	
all: $(TARGET0) $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4)
		
clean:
	rm -rf $(OBJDIR) $(TARGET0) $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4)
	
.PHONY: all clean
