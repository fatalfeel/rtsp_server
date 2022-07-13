#ifndef BITSTREAM_H
#define BITSTREAM_H

typedef struct _BS_t
{
    uint8_t*    start;      // 指向buf头部指针
    uint8_t*    p;          // 当前指针位置
    uint8_t*    end;        // 指向buf尾部指针
    int         bits_left;  // 当前读取字节的剩余(可用/未读)比特个数
}BS_t;

extern void     bs_init(uint8_t tmp);
extern uint32_t bs_read_u1();
extern uint32_t bs_read_u(int n);
extern uint32_t bs_read_ue();

#endif
