#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Signature checking for manifesto receipts. Plain C++ over libsecp256k1, no
// Qt, so tests/sig_host.cpp can build it on its own.
//
// A signature here is an Ethereum personal message signature (EIP-191): 65
// bytes r‖s‖v over keccak256("\x19Ethereum Signed Message:\n" + len + text).
// Checking one means recovering the public key from the signature and the
// text, and comparing its address with the address the receipt claims.
namespace sig {

using Hash32 = std::array<uint8_t, 32>;
using Address = std::array<uint8_t, 20>;

Hash32 keccak256(const uint8_t* data, size_t len);
Hash32 keccak256(const std::string& bytes);

// The hash a wallet signs for a personal message.
Hash32 eip191Hash(const std::string& text);

// Hex helpers. fromHex accepts an optional 0x prefix; false on anything else.
bool fromHex(const std::string& hex, std::vector<uint8_t>& out);
std::string toHex(const uint8_t* data, size_t len);

// The address that signed `text`, from a 65-byte signature in hex (v as 27/28
// or 0/1). False when the signature is malformed or recovers to nothing.
bool recoverSigner(const std::string& text, const std::string& sigHex, Address& out, std::string& err);

// --- signing with a key this app holds itself -------------------------------
using SecretKey = std::array<uint8_t, 32>;

// A fresh key from the operating system's random source. False when the
// system gives no randomness; the key is then left zeroed.
bool generateKey(SecretKey& out);
// True for a usable secp256k1 secret (non-zero, below the group order).
bool validKey(const SecretKey& key);
// The Ethereum-style address of a key's public key.
bool addressOfKey(const SecretKey& key, Address& out);
// Sign `text` as a personal message (EIP-191). The result is 65 bytes in hex,
// 0x-prefixed, r‖s‖v with v as 27/28 and s in the lower half, the form every
// Ethereum tool accepts. Deterministic (RFC 6979): no randomness at signing.
bool signMessage(const std::string& text, const SecretKey& key, std::string& sigHexOut, std::string& err);
// Overwrite a key in memory.
void wipe(SecretKey& key);

// 0x-prefixed address in the mixed-case checksum form (EIP-55).
std::string checksumAddress(const Address& a);
// Parse 0x + 40 hex characters, any case. False otherwise.
bool parseAddress(const std::string& s, Address& out);

// Known-answer checks (keccak, checksum, one recovery). Empty when all pass,
// otherwise the name of the first that failed.
std::string selfTest();

} // namespace sig
