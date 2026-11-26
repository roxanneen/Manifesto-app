// Command-line host for src/sigcheck.*, used by tests/sig_interop.py.
//   sig_host selftest
//   sig_host keccak <hex>
//   sig_host eip191 <text>
//   sig_host recover <text> <signature hex>
//   sig_host sign <text> <key hex>      (test keys only: the key is on the command line)
//   sig_host address <key hex>
//   sig_host genkey
#include "sigcheck.h"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "selftest") {
        const std::string r = sig::selfTest();
        std::printf("%s\n", r.empty() ? "ok" : r.c_str());
        return r.empty() ? 0 : 1;
    }
    if (cmd == "keccak" && argc == 3) {
        std::vector<uint8_t> b;
        if (!sig::fromHex(argv[2], b)) { std::printf("bad hex\n"); return 2; }
        const sig::Hash32 h = sig::keccak256(b.data(), b.size());
        std::printf("%s\n", sig::toHex(h.data(), 32).c_str());
        return 0;
    }
    if (cmd == "eip191" && argc == 3) {
        const sig::Hash32 h = sig::eip191Hash(argv[2]);
        std::printf("%s\n", sig::toHex(h.data(), 32).c_str());
        return 0;
    }
    if (cmd == "recover" && argc == 4) {
        sig::Address a;
        std::string err;
        if (!sig::recoverSigner(argv[2], argv[3], a, err)) { std::printf("error: %s\n", err.c_str()); return 1; }
        std::printf("%s\n", sig::checksumAddress(a).c_str());
        return 0;
    }
    if ((cmd == "sign" && argc == 4) || (cmd == "address" && argc == 3)) {
        std::vector<uint8_t> kb;
        const std::string keyHex = cmd == "sign" ? argv[3] : argv[2];
        sig::SecretKey k;
        if (!sig::fromHex(keyHex, kb) || kb.size() != 32) { std::printf("error: bad key\n"); return 2; }
        for (size_t i = 0; i < 32; ++i) k[i] = kb[i];
        if (cmd == "address") {
            sig::Address a;
            if (!sig::addressOfKey(k, a)) { std::printf("error: unusable key\n"); return 1; }
            std::printf("%s\n", sig::checksumAddress(a).c_str());
            return 0;
        }
        std::string s, err;
        if (!sig::signMessage(argv[2], k, s, err)) { std::printf("error: %s\n", err.c_str()); return 1; }
        std::printf("%s\n", s.c_str());
        return 0;
    }
    if (cmd == "genkey") {
        sig::SecretKey k;
        if (!sig::generateKey(k)) { std::printf("error: no randomness\n"); return 1; }
        std::printf("%s\n", sig::toHex(k.data(), 32).c_str());
        return 0;
    }
    std::fprintf(stderr, "usage: sig_host selftest | keccak <hex> | eip191 <text> | recover <text> <sig> | sign <text> <key> | address <key> | genkey\n");
    return 2;
}
