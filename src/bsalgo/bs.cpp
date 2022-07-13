#include <stdint.h>
#include <memory.h>
#include "bs.h"

static uint8_t  s_buf[4];
static BS_t     s_bs;

void bs_init(uint8_t tmp)
{
    memset(s_buf, 0, sizeof(s_buf));

    s_buf[0]        = tmp;
    s_bs.start      = s_buf;                  // 指向buf起始位置
    s_bs.p          = s_buf;                  // 初始位置与start保持一致
    s_bs.end        = s_buf + sizeof(s_buf);  // 指向buf末尾
    s_bs.bits_left  = 8;
}

static bool bs_eof()
{
    if (s_bs.p >= s_bs.end)
        return true;

    return false;
}

//读取1个比特
uint32_t bs_read_u1()
{
    uint32_t r = 0; // 读取比特返回值

    // 1.剩余比特先减1
    s_bs.bits_left--;

    if (!bs_eof())
    {
        // 2.计算返回值
        r = ((*(s_bs.p)) >> s_bs.bits_left) & 0x01;
    }

    // 3.判断是否读到字节末尾，如果是指针位置移向下一字节，比特位初始为8
    if (s_bs.bits_left == 0) { s_bs.p ++; s_bs.bits_left = 8; }

    return r;
}

//读取n个比特
uint32_t bs_read_u(int n)
{
    uint32_t r = 0; // 读取比特返回值
    int i;  // 当前读取到的比特位索引
    for (i = 0; i < n; i++)
    {
        // 1.每次读取1比特，并依次从高位到低位放在r中
        r |= ( bs_read_u1() << ( n - i - 1 ) );
    }
    return r;
}

//ue(v) 解码
uint32_t bs_read_ue()
{
    int32_t r = 0; // 解码得到的返回值
    int i = 0;     // leadingZeroBits

    // 1.计算leadingZeroBits
    while( (bs_read_u1() == 0) && (i < 32) && (!bs_eof()) )
    {
        i++;
    }
    // 2.计算read_bits( leadingZeroBits )
    r = bs_read_u(i);
    // 3.计算codeNum，1 << i即为2的i次幂
    r += (1 << i) - 1;
    return r;
}
