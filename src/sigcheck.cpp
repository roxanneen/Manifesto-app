#include "sigcheck.h"

#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/random.h>
#endif
#endif

namespace sig {

// ---------------------------------------------------------------------------
// Keccak-256 (the original Keccak padding, as Ethereum uses; not SHA3-256).

namespace {

const uint64_t kRC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};
const int kRot[24] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};
const int kPil[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};

inline uint64_t rotl(uint64_t x, int n) { return (x << n) | (x >> (64 - n)); }

void keccakF(uint64_t st[25])
{
    uint64_t bc[5];
    for (int round = 0; round < 24; ++round) {
        for (int i = 0; i < 5; ++i)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        for (int i = 0; i < 5; ++i) {
            const uint64_t t = bc[(i + 4) % 5] ^ rotl(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5)
                st[j + i] ^= t;
        }
        uint64_t t = st[1];
        for (int i = 0; i < 24; ++i) {
            const int j = kPil[i];
            const uint64_t keep = st[j];
            st[j] = rotl(t, kRot[i]);
            t = keep;
        }
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; ++i)
                bc[i] = st[j + i];
            for (int i = 0; i < 5; ++i)
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }
        st[0] ^= kRC[round];
    }
}

const secp256k1_context* ctx()
{
    static secp256k1_context* c = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    return c;
}

} // namespace

Hash32 keccak256(const uint8_t* data, size_t len)
{
    constexpr size_t rate = 136;
    uint64_t st[25] = {0};
    uint8_t block[rate];

    auto absorb = [&st](const uint8_t* b) {
        for (size_t i = 0; i < rate / 8; ++i) {
            uint64_t lane = 0;
            for (int k = 7; k >= 0; --k)
                lane = (lane << 8) | b[i * 8 + k];
            st[i] ^= lane;
        }
        keccakF(st);
    };

    while (len >= rate) {
        absorb(data);
        data += rate;
        len -= rate;
    }
    std::memset(block, 0, rate);
    if (len)
        std::memcpy(block, data, len);
    block[len] ^= 0x01;
    block[rate - 1] ^= 0x80;
    absorb(block);

    Hash32 out;
    for (size_t i = 0; i < 32; ++i)
        out[i] = static_cast<uint8_t>(st[i / 8] >> (8 * (i % 8)));
    return out;
}

Hash32 keccak256(const std::string& bytes)
{
    return keccak256(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

Hash32 eip191Hash(const std::string& text)
{
    std::string m("\x19" "Ethereum Signed Message:\n");
    m += std::to_string(text.size());
    m += text;
    return keccak256(m);
}

// ---------------------------------------------------------------------------
// Hex

bool fromHex(const std::string& hexIn, std::vector<uint8_t>& out)
{
    std::string h = hexIn;
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X'))
        h = h.substr(2);
    if (h.size() % 2 != 0)
        return false;
    out.clear();
    out.reserve(h.size() / 2);
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < h.size(); i += 2) {
        const int hi = val(h[i]), lo = val(h[i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

std::string toHex(const uint8_t* data, size_t len)
{
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        s.push_back(d[data[i] >> 4]);
        s.push_back(d[data[i] & 0x0f]);
    }
    return s;
}

namespace {
void addressOfPub(const secp256k1_pubkey& pub, Address& out);
}

// ---------------------------------------------------------------------------
// Recovery

bool recoverSigner(const std::string& text, const std::string& sigHex, Address& out, std::string& err)
{
    std::vector<uint8_t> s;
    if (!fromHex(sigHex, s)) {
        err = "the signature is not hex";
        return false;
    }
    if (s.size() != 65) {
        err = "a signature is 65 bytes, this one is " + std::to_string(s.size());
        return false;
    }
    int recid = s[64];
    if (recid >= 27)
        recid -= 27;
    if (recid < 0 || recid > 1) {
        err = "the signature's last byte is not a recovery id";
        return false;
    }
    secp256k1_ecdsa_recoverable_signature rs;
    if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx(), &rs, s.data(), recid)) {
        err = "the signature is out of range";
        return false;
    }
    const Hash32 h = eip191Hash(text);
    secp256k1_pubkey pub;
    if (!secp256k1_ecdsa_recover(ctx(), &pub, &rs, h.data())) {
        err = "no key matches this signature and text";
        return false;
    }
    addressOfPub(pub, out);
    return true;
}

// ---------------------------------------------------------------------------
// Own key: generate, derive the address, sign

namespace {

bool systemRandom(uint8_t* out, size_t len)
{
#if defined(_WIN32)
    return BCryptGenRandom(nullptr, out, static_cast<ULONG>(len), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    return getentropy(out, len) == 0;   // len is 32 here; the call allows up to 256
#endif
}

void addressOfPub(const secp256k1_pubkey& pub, Address& out)
{
    uint8_t ser[65];
    size_t n = sizeof(ser);
    secp256k1_ec_pubkey_serialize(ctx(), ser, &n, &pub, SECP256K1_EC_UNCOMPRESSED);
    const Hash32 ph = keccak256(ser + 1, 64);
    std::memcpy(out.data(), ph.data() + 12, 20);
}

} // namespace

void wipe(SecretKey& key)
{
    volatile uint8_t* p = key.data();
    for (size_t i = 0; i < key.size(); ++i)
        p[i] = 0;
}

bool validKey(const SecretKey& key)
{
    return secp256k1_ec_seckey_verify(ctx(), key.data()) == 1;
}

bool generateKey(SecretKey& out)
{
    // A random 32-byte string is a valid key except with negligible
    // probability; the loop covers that case without bias.
    for (int i = 0; i < 16; ++i) {
        if (!systemRandom(out.data(), out.size()))
            break;
        if (validKey(out))
            return true;
    }
    wipe(out);
    return false;
}

bool addressOfKey(const SecretKey& key, Address& out)
{
    secp256k1_pubkey pub;
    if (!secp256k1_ec_pubkey_create(ctx(), &pub, key.data()))
        return false;
    addressOfPub(pub, out);
    return true;
}

bool signMessage(const std::string& text, const SecretKey& key, std::string& sigHexOut, std::string& err)
{
    if (!validKey(key)) {
        err = "the key is not usable";
        return false;
    }
    const Hash32 h = eip191Hash(text);
    secp256k1_ecdsa_recoverable_signature rs;
    if (!secp256k1_ecdsa_sign_recoverable(ctx(), &rs, h.data(), key.data(), nullptr, nullptr)) {
        err = "signing failed";
        return false;
    }
    uint8_t out[65];
    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx(), out, &recid, &rs);
    out[64] = static_cast<uint8_t>(27 + recid);
    sigHexOut = "0x" + toHex(out, sizeof(out));

    // Never hand out a signature this code cannot itself verify.
    Address want, got;
    std::string rerr;
    if (!addressOfKey(key, want) || !recoverSigner(text, sigHexOut, got, rerr) || want != got) {
        sigHexOut.clear();
        err = "the new signature did not verify";
        return false;
    }
    return true;
}

std::string checksumAddress(const Address& a)
{
    std::string h = toHex(a.data(), a.size());
    const Hash32 k = keccak256(h);
    for (size_t i = 0; i < h.size(); ++i) {
        const int nibble = (i % 2 == 0) ? (k[i / 2] >> 4) : (k[i / 2] & 0x0f);
        if (h[i] >= 'a' && h[i] <= 'f' && nibble >= 8)
            h[i] = static_cast<char>(h[i] - 'a' + 'A');
    }
    return "0x" + h;
}

bool parseAddress(const std::string& s, Address& out)
{
    if (s.size() != 42 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
        return false;
    std::vector<uint8_t> b;
    if (!fromHex(s, b) || b.size() != 20)
        return false;
    std::memcpy(out.data(), b.data(), 20);
    return true;
}

// ---------------------------------------------------------------------------
// Self-test

std::string selfTest()
{
    // keccak256("") and keccak256("abc"), from the Keccak team's vectors.
    {
        const Hash32 e = keccak256(std::string());
        if (toHex(e.data(), 32) != "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470")
            return "keccak256(empty)";
        const Hash32 a = keccak256(std::string("abc"));
        if (toHex(a.data(), 32) != "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45")
            return "keccak256(abc)";
        // More than one 136-byte block: 200 bytes of 'a' (value from eth-utils).
        const Hash32 b = keccak256(std::string(200, 'a'));
        if (toHex(b.data(), 32) != "96ea54061def936c4be90b518992fdc6f12f535068a256229aca54267b4d084d")
            return "keccak256(multi-block)";
    }
    // EIP-55's own example address.
    {
        Address a;
        if (!parseAddress("0x5aaeb6053f3e94c9b9a09f33669435e7ef1beaed", a))
            return "parseAddress";
        if (checksumAddress(a) != "0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed")
            return "checksumAddress";
    }
    // The personal message hash of "hello world", widely published.
    {
        const Hash32 h = eip191Hash("hello world");
        if (toHex(h.data(), 32) != "d9eba16ed0ecae432b71fe008c98cc872bb4cc214d3220a36f365326cf807d68")
            return "eip191Hash";
    }
    // One whole recovery: "hello world" signed by the first published Anvil
    // test key (signature made with eth-account), and the same signature
    // refused for another text.
    {
        const std::string s =
            "a461f509887bd19e312c0c58467ce8ff8e300d3c1a90b608a760c5b80318eaf1"
            "5fe57c96f9175d6cd4daad4663763baa7e78836e067d0163e9a2ccf2ff753f5b1b";
        Address a;
        std::string err;
        if (!recoverSigner("hello world", s, a, err)
            || checksumAddress(a) != "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266")
            return "recoverSigner";
        Address b;
        if (recoverSigner("hello worle", s, b, err) && a == b)
            return "recoverSigner(other text)";
    }
    // Signing with the same published test key gives the same signature
    // (RFC 6979 makes it deterministic), and a fresh key signs and verifies.
    {
        SecretKey k;
        std::vector<uint8_t> kb;
        fromHex("ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80", kb);
        std::memcpy(k.data(), kb.data(), 32);
        std::string s, err;
        if (!signMessage("hello world", k, s, err)
            || s != "0xa461f509887bd19e312c0c58467ce8ff8e300d3c1a90b608a760c5b80318eaf1"
                    "5fe57c96f9175d6cd4daad4663763baa7e78836e067d0163e9a2ccf2ff753f5b1b")
            return "signMessage";
        SecretKey fresh;
        Address a, b;
        if (!generateKey(fresh) || !addressOfKey(fresh, a) || !signMessage("x", fresh, s, err)
            || !recoverSigner("x", s, b, err) || a != b)
            return "generateKey";
        wipe(fresh);
        wipe(k);
    }
    return std::string();
}

} // namespace sig
