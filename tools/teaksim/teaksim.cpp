// teaksim: runs a Teak ELF in teakra and talks to it with the dsp-bench
// protocol (examples/dsp-bench/*/common/proto.h).
//
// usage: teaksim teak.elf < script
//
// Script lines (numbers in hex):
//   cmd <replies> <cmd0> <arg>    send a command, print "rep <rep0> <value>"
//                                 for each reply, then "cycles <n>"
//   mem <arm_addr> <words>        print emulated ARM memory (16-bit words)
//   quit
//
// ARM memory is emulated for AHBM/DMA: 4 MB at 0x02000000 and 512 KB at
// 0x06000000.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <teakra/teakra.h>

static std::vector<uint8_t> main_ram(4 * 1024 * 1024);
static std::vector<uint8_t> vram(512 * 1024);

static uint8_t *arm_ptr(uint32_t addr)
{
    if (addr >= 0x02000000 && addr < 0x02000000 + main_ram.size())
        return &main_ram[addr - 0x02000000];
    if (addr >= 0x06000000 && addr < 0x06000000 + vram.size())
        return &vram[addr - 0x06000000];
    static uint8_t dummy[4];
    fprintf(stderr, "AHBM access outside emulated memory: %08X\n", addr);
    return dummy;
}

static bool load_elf(Teakra::Teakra &t, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    std::vector<uint8_t> d;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        d.insert(d.end(), buf, buf + n);
    fclose(f);

    auto u16 = [&](size_t o) { return (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8); };
    auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };

    uint32_t shoff = u32(0x20);
    uint32_t shentsize = u16(0x2E);
    uint32_t shnum = u16(0x30);

    for (uint32_t i = 0; i < shnum; i++)
    {
        size_t sh = shoff + i * shentsize;
        uint32_t type = u32(sh + 4);
        uint32_t flags = u32(sh + 8);
        uint32_t addr = u32(sh + 12);
        uint32_t off = u32(sh + 16);
        uint32_t size = u32(sh + 20);

        if (type != 1 || !(flags & 2) || size == 0) // PROGBITS + ALLOC
            continue;

        for (uint32_t w = 0; w < size / 2; w++)
        {
            uint16_t v = u16(off + w * 2);
            if (addr >= 0x10000000)
                t.DataWrite((addr - 0x10000000) / 2 + w, v, true);
            else
                t.ProgramWrite(addr / 2 + w, v);
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: teaksim teak.elf < script\n");
        return 1;
    }

    Teakra::UserConfig config;
    Teakra::Teakra t(config);

    Teakra::AHBMCallback cb;
    cb.read8 = [](uint32_t a) { return *arm_ptr(a); };
    cb.write8 = [](uint32_t a, uint8_t v) { *arm_ptr(a) = v; };
    cb.read16 = [](uint32_t a) { uint8_t *p = arm_ptr(a); return (uint16_t)(p[0] | (p[1] << 8)); };
    cb.write16 = [](uint32_t a, uint16_t v) { uint8_t *p = arm_ptr(a); p[0] = v; p[1] = v >> 8; };
    cb.read32 = [](uint32_t a) { uint8_t *p = arm_ptr(a); return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); };
    cb.write32 = [](uint32_t a, uint32_t v) { uint8_t *p = arm_ptr(a); p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; };
    t.SetAHBMCallback(cb);

    t.Reset();
    if (!load_elf(t, argv[1]))
    {
        fprintf(stderr, "can't load %s\n", argv[1]);
        return 1;
    }

    const uint64_t budget = 4000000000ull;
    std::string line;

    while (std::getline(std::cin, line))
    {
        std::istringstream in(line);
        std::string op;
        in >> op;

        if (op == "cmd")
        {
            unsigned replies, cmd0, arg;
            in >> std::hex >> replies >> cmd0 >> arg;

            uint64_t cycles = 0;
            auto send = [&](int i, uint16_t v) {
                while (!t.SendDataIsEmpty(i) && cycles < budget) { t.Run(16); cycles += 16; }
                t.SendData(i, v);
            };
            auto recv = [&](int i, bool &ok) -> uint16_t {
                while (!t.RecvDataIsReady(i) && cycles < budget) { t.Run(16); cycles += 16; }
                ok = t.RecvDataIsReady(i);
                return ok ? t.RecvData(i) : 0;
            };

            send(1, arg >> 16);
            send(2, arg & 0xFFFF);
            send(0, cmd0);

            bool ok = true;
            for (unsigned r = 0; r < replies && ok; r++)
            {
                uint16_t rep0 = recv(0, ok);
                uint16_t hi = recv(1, ok);
                uint16_t lo = recv(2, ok);
                if (!ok)
                    printf("timeout\n");
                else
                    printf("rep %04X %08X\n", rep0, ((uint32_t)hi << 16) | lo);
            }
            printf("cycles %llu\n", (unsigned long long)cycles);
            fflush(stdout);
        }
        else if (op == "mem")
        {
            unsigned addr, words;
            in >> std::hex >> addr >> words;
            for (unsigned w = 0; w < words; w++)
            {
                uint8_t *p = arm_ptr(addr + w * 2);
                printf("%04X%c", p[0] | (p[1] << 8), (w % 16 == 15) ? '\n' : ' ');
            }
            printf("\n");
            fflush(stdout);
        }
        else if (op == "quit")
        {
            break;
        }
    }

    return 0;
}
