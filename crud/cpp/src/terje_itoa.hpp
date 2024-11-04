#pragma once
#include <string>

typedef uint32_t fix4_28;
/// Terje Mathisens high speed itoa
/// \param buf Buffer to write to
/// \param val integer to convert
void itoa(char *buf, uint32_t val)
{
    fix4_28 const f1_10000 = (1 << 28) / 10000;
    fix4_28 tmplo, tmphi;

    auto lo = val % 100000;
    auto hi = val / 100000;

    tmplo = lo * (f1_10000 + 1) - (lo / 4);
    tmphi = hi * (f1_10000 + 1) - (hi / 4);

    for(size_t i = 0; i < 5; i++)
    {
        buf[i + 0] = '0' + (char)(tmphi >> 28);
        buf[i + 5] = '0' + (char)(tmplo >> 28);
        tmphi = (tmphi & 0x0fffffff) * 10;
        tmplo = (tmplo & 0x0fffffff) * 10;
    }
}