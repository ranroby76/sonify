// D:\workspace\Fanan_web_hub\src\Ed25519.cpp

#include "Ed25519.h"

#include <cstring>
#include <vector>

namespace
{
    using i64 = std::int64_t;
    using u64 = std::uint64_t;
    using u8 = std::uint8_t;

    // A field element mod 2^255 - 19: 16 limbs of 16 bits (signed, with room for carries).
    using Field = i64[16];

    const Field zero {};
    const Field one { 1 };
    const Field curveD  { 0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070, 0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203 };
    const Field curveD2 { 0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0, 0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406 };
    const Field baseX   { 0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c, 0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169 };
    const Field baseY   { 0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666 };
    const Field sqrtM1  { 0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43, 0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83 };

    // The group's order, L = 2^252 + 27742317777372353535851937790883648493, little-endian bytes.
    const i64 orderL[32] { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
                           0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x10 };

    /** 0 when the 32 bytes are equal, -1 otherwise (in constant time). */
    int differ32 (const u8* x, const u8* y) noexcept
    {
        std::uint32_t d = 0;

        for (int i = 0; i < 32; ++i)
            d |= (std::uint32_t) (x[i] ^ y[i]);

        return (int) (1 & ((d - 1) >> 8)) - 1;
    }

    void copy (Field r, const Field a) noexcept
    {
        for (int i = 0; i < 16; ++i)
            r[i] = a[i];
    }

    void carry (Field o) noexcept
    {
        for (int i = 0; i < 16; ++i)
        {
            o[i] += (i64) 1 << 16;
            const i64 c = o[i] >> 16;
            o[(i + 1) * (i < 15 ? 1 : 0)] += c - 1 + 37 * (c - 1) * (i == 15 ? 1 : 0);
            o[i] -= c * ((i64) 1 << 16);
        }
    }

    /** Swaps p and q when b is 1 (in constant time). */
    void select (Field p, Field q, int b) noexcept
    {
        const i64 c = ~((i64) b - 1);

        for (int i = 0; i < 16; ++i)
        {
            const i64 t = c & (p[i] ^ q[i]);
            p[i] ^= t;
            q[i] ^= t;
        }
    }

    void pack (u8* o, const Field n) noexcept
    {
        Field m, t;
        copy (t, n);
        carry (t);
        carry (t);
        carry (t);

        for (int j = 0; j < 2; ++j)
        {
            m[0] = t[0] - 0xffed;

            for (int i = 1; i < 15; ++i)
            {
                m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
                m[i - 1] &= 0xffff;
            }

            m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
            const int b = (int) ((m[15] >> 16) & 1);
            m[14] &= 0xffff;
            select (t, m, 1 - b);
        }

        for (int i = 0; i < 16; ++i)
        {
            o[2 * i] = (u8) (t[i] & 0xff);
            o[2 * i + 1] = (u8) (t[i] >> 8);
        }
    }

    bool notEqual (const Field a, const Field b) noexcept
    {
        u8 c[32], d[32];
        pack (c, a);
        pack (d, b);
        return differ32 (c, d) != 0;
    }

    u8 parity (const Field a) noexcept
    {
        u8 d[32];
        pack (d, a);
        return (u8) (d[0] & 1);
    }

    void unpack (Field o, const u8* n) noexcept
    {
        for (int i = 0; i < 16; ++i)
            o[i] = n[2 * i] + ((i64) n[2 * i + 1] << 8);

        o[15] &= 0x7fff;
    }

    void add (Field o, const Field a, const Field b) noexcept
    {
        for (int i = 0; i < 16; ++i)
            o[i] = a[i] + b[i];
    }

    void subtract (Field o, const Field a, const Field b) noexcept
    {
        for (int i = 0; i < 16; ++i)
            o[i] = a[i] - b[i];
    }

    void multiply (Field o, const Field a, const Field b) noexcept
    {
        i64 t[31] {};

        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 16; ++j)
                t[i + j] += a[i] * b[j];

        for (int i = 0; i < 15; ++i)
            t[i] += 38 * t[i + 16];

        for (int i = 0; i < 16; ++i)
            o[i] = t[i];

        carry (o);
        carry (o);
    }

    void square (Field o, const Field a) noexcept
    {
        multiply (o, a, a);
    }

    void invert (Field o, const Field in) noexcept
    {
        Field c;
        copy (c, in);

        for (int a = 253; a >= 0; --a)
        {
            square (c, c);

            if (a != 2 && a != 4)
                multiply (c, c, in);
        }

        copy (o, c);
    }

    void pow2523 (Field o, const Field in) noexcept
    {
        Field c;
        copy (c, in);

        for (int a = 250; a >= 0; --a)
        {
            square (c, c);

            if (a != 1)
                multiply (c, c, in);
        }

        copy (o, c);
    }

    //==========================================================================
    // Points (extended coordinates X, Y, Z, T).
    struct Point
    {
        Field v[4];
    };

    void pointAdd (Point& p, const Point& q) noexcept
    {
        Field a, b, c, d, t, e, f, g, h;
        subtract (a, p.v[1], p.v[0]);
        subtract (t, q.v[1], q.v[0]);
        multiply (a, a, t);
        add (b, p.v[0], p.v[1]);
        add (t, q.v[0], q.v[1]);
        multiply (b, b, t);
        multiply (c, p.v[3], q.v[3]);
        multiply (c, c, curveD2);
        multiply (d, p.v[2], q.v[2]);
        add (d, d, d);
        subtract (e, b, a);
        subtract (f, d, c);
        add (g, d, c);
        add (h, b, a);

        multiply (p.v[0], e, f);
        multiply (p.v[1], h, g);
        multiply (p.v[2], g, f);
        multiply (p.v[3], e, h);
    }

    void swapPoints (Point& p, Point& q, u8 b) noexcept
    {
        for (int i = 0; i < 4; ++i)
            select (p.v[i], q.v[i], b);
    }

    void packPoint (u8* r, const Point& p) noexcept
    {
        Field tx, ty, zi;
        invert (zi, p.v[2]);
        multiply (tx, p.v[0], zi);
        multiply (ty, p.v[1], zi);
        pack (r, ty);
        r[31] ^= (u8) (parity (tx) << 7);
    }

    void scalarMultiply (Point& p, Point& q, const u8* s) noexcept
    {
        copy (p.v[0], zero);
        copy (p.v[1], one);
        copy (p.v[2], one);
        copy (p.v[3], zero);

        for (int i = 255; i >= 0; --i)
        {
            const auto b = (u8) ((s[i / 8] >> (i & 7)) & 1);
            swapPoints (p, q, b);
            pointAdd (q, p);
            pointAdd (p, p);
            swapPoints (p, q, b);
        }
    }

    void scalarBase (Point& p, const u8* s) noexcept
    {
        Point q;
        copy (q.v[0], baseX);
        copy (q.v[1], baseY);
        copy (q.v[2], one);
        multiply (q.v[3], baseX, baseY);
        scalarMultiply (p, q, s);
    }

    /** The point a public key names, negated. False when it isn't a point on the curve. */
    bool unpackNegated (Point& r, const u8 p[32]) noexcept
    {
        Field t, chk, num, den, den2, den4, den6;
        copy (r.v[2], one);
        unpack (r.v[1], p);
        square (num, r.v[1]);
        multiply (den, num, curveD);
        subtract (num, num, r.v[2]);
        add (den, r.v[2], den);

        square (den2, den);
        square (den4, den2);
        multiply (den6, den4, den2);
        multiply (t, den6, num);
        multiply (t, t, den);

        pow2523 (t, t);
        multiply (t, t, num);
        multiply (t, t, den);
        multiply (t, t, den);
        multiply (r.v[0], t, den);

        square (chk, r.v[0]);
        multiply (chk, chk, den);

        if (notEqual (chk, num))
            multiply (r.v[0], r.v[0], sqrtM1);

        square (chk, r.v[0]);
        multiply (chk, chk, den);

        if (notEqual (chk, num))
            return false;

        if (parity (r.v[0]) == (p[31] >> 7))
            subtract (r.v[0], zero, r.v[0]);

        multiply (r.v[3], r.v[0], r.v[1]);
        return true;
    }

    /** x (64 limbs) mod L, into 32 bytes. */
    void modL (u8* r, i64 x[64]) noexcept
    {
        i64 c = 0;

        for (int i = 63; i >= 32; --i)
        {
            c = 0;
            int j = i - 32;

            for (; j < i - 12; ++j)
            {
                x[j] += c - 16 * x[i] * orderL[j - (i - 32)];
                c = (x[j] + 128) >> 8;
                x[j] -= c * 256;
            }

            x[j] += c;
            x[i] = 0;
        }

        c = 0;

        for (int j = 0; j < 32; ++j)
        {
            x[j] += c - (x[31] >> 4) * orderL[j];
            c = x[j] >> 8;
            x[j] &= 255;
        }

        for (int j = 0; j < 32; ++j)
            x[j] -= c * orderL[j];

        for (int i = 0; i < 32; ++i)
        {
            x[i + 1] += x[i] >> 8;
            r[i] = (u8) (x[i] & 255);
        }
    }

    void reduce (u8* r) noexcept
    {
        i64 x[64];

        for (int i = 0; i < 64; ++i)
            x[i] = (i64) r[i];

        for (int i = 0; i < 64; ++i)
            r[i] = 0;

        modL (r, x);
    }

    //==========================================================================
    // SHA-512.
    struct Sha512
    {
        u64 h[8] { 0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
                   0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull };
        u8 block[128] {};
        u64 length = 0;
        int used = 0;

        static u64 rotr (u64 x, int n) noexcept    { return (x >> n) | (x << (64 - n)); }

        void compress() noexcept
        {
            static const u64 k[80] {
                0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull, 0x3956c25bf348b538ull,
                0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull, 0xd807aa98a3030242ull, 0x12835b0145706fbeull,
                0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull, 0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull,
                0xc19bf174cf692694ull, 0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
                0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull, 0x983e5152ee66dfabull,
                0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull, 0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
                0x06ca6351e003826full, 0x142929670a0e6e70ull, 0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull,
                0x53380d139d95b3dfull, 0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
                0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull, 0xd192e819d6ef5218ull,
                0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull, 0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull,
                0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull, 0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull,
                0x682e6ff3d6b2b8a3ull, 0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
                0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull, 0xca273eceea26619cull,
                0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull, 0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull,
                0x113f9804bef90daeull, 0x1b710b35131c471bull, 0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull,
                0x431d67c49c100d4cull, 0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull };

            u64 w[80];

            for (int i = 0; i < 16; ++i)
            {
                w[i] = 0;

                for (int j = 0; j < 8; ++j)
                    w[i] = (w[i] << 8) | block[i * 8 + j];
            }

            for (int i = 16; i < 80; ++i)
            {
                const auto s0 = rotr (w[i - 15], 1) ^ rotr (w[i - 15], 8) ^ (w[i - 15] >> 7);
                const auto s1 = rotr (w[i - 2], 19) ^ rotr (w[i - 2], 61) ^ (w[i - 2] >> 6);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }

            auto a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];

            for (int i = 0; i < 80; ++i)
            {
                const auto t1 = hh + (rotr (e, 14) ^ rotr (e, 18) ^ rotr (e, 41)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
                const auto t2 = (rotr (a, 28) ^ rotr (a, 34) ^ rotr (a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }

            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }

        void add (const u8* data, std::size_t size) noexcept
        {
            for (std::size_t i = 0; i < size; ++i)
            {
                block[used++] = data[i];
                ++length;

                if (used == 128)
                {
                    compress();
                    used = 0;
                }
            }
        }

        void finish (u8 out[64]) noexcept
        {
            const auto bits = length * 8;
            const u8 marker = 0x80, nothing = 0;
            add (&marker, 1);

            while (used != 112)
                add (&nothing, 1);

            for (int i = 0; i < 8; ++i)   // the high 64 bits of the length: 0
                add (&nothing, 1);

            for (int i = 7; i >= 0; --i)
            {
                const auto byte = (u8) (bits >> (i * 8));
                add (&byte, 1);
            }

            for (int i = 0; i < 8; ++i)
                for (int j = 0; j < 8; ++j)
                    out[i * 8 + j] = (u8) (h[i] >> (56 - j * 8));
        }
    };
}

//==============================================================================
void Ed25519::sha512 (const std::uint8_t* data, std::size_t size, std::uint8_t out[64]) noexcept
{
    Sha512 hash;
    hash.add (data, size);
    hash.finish (out);
}

bool Ed25519::verify (const std::uint8_t signature[64], const std::uint8_t* message, std::size_t messageSize,
                      const std::uint8_t publicKey[32]) noexcept
{
    if (signature == nullptr || publicKey == nullptr || (message == nullptr && messageSize > 0))
        return false;

    // s must be below the group's order (RFC 8032): a signature has one form only.
    for (int i = 31; i >= 0; --i)
    {
        const auto limb = (i64) signature[32 + i];

        if (limb < orderL[i]) break;
        if (limb > orderL[i] || i == 0) return false;
    }

    Point a;

    if (! unpackNegated (a, publicKey))
        return false;

    // h = SHA-512 (R, A, message) mod L; then s B - h A must be R.
    u8 h[64];
    Sha512 hash;
    hash.add (signature, 32);
    hash.add (publicKey, 32);

    if (messageSize > 0)
        hash.add (message, messageSize);

    hash.finish (h);
    reduce (h);

    Point p, q;
    scalarMultiply (p, a, h);
    scalarBase (q, signature + 32);
    pointAdd (p, q);

    u8 r[32];
    packPoint (r, p);
    return differ32 (signature, r) == 0;
}
