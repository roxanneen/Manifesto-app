#!/usr/bin/env python3
"""Check src/sigcheck.* against an independent implementation (eth-account).

  python3 -m venv tests/.venv && tests/.venv/bin/pip install eth-account
  tests/.venv/bin/python tests/sig_interop.py tests/work/sig_host

Keys are generated fresh for each run, plus the first well-known Anvil test
key so one case has a published address. Nothing here touches a real account.
"""
import hashlib
import os
import subprocess
import sys

from eth_account import Account
from eth_account.messages import encode_defunct
from eth_utils import keccak

HOST = sys.argv[1]
ANVIL0 = "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
ANVIL0_ADDR = "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266"

passed = failed = 0


def check(name, ok, detail=""):
    global passed, failed
    if ok:
        passed += 1
        print(f"  ok   {name}")
    else:
        failed += 1
        print(f"  FAIL {name} {detail}")


def host(*args):
    r = subprocess.run([HOST, *args], capture_output=True, text=True)
    return r.returncode, r.stdout.strip()


def statement(h, date="2026-11-26"):
    return ('I sign the Logos manifesto, "Logos: A Declaration of Independence in Cyberspace", inscribed on Bitcoin as '
            '1c3990078b6a4e68d2e5c87c88cff9782f261cb22b5375bfc1cdbe711cbf4686i0. '
            f"Image sha256: {h}. Signed on {date}.")


check("selftest", host("selftest") == (0, "ok"))

# keccak over sizes around the 136-byte block boundary
for n in (0, 1, 55, 135, 136, 137, 271, 272, 273, 1000, 8103):
    data = os.urandom(n)
    code, out = host("keccak", data.hex()) if n else host("keccak", "")
    check(f"keccak256 over {n} bytes", code == 0 and out == keccak(data).hex(), out)

manifesto = open(os.path.join(os.path.dirname(__file__), "..", "src", "manifesto.txt"), "rb").read()
text = statement(hashlib.sha256(manifesto).hexdigest())

accounts = [Account.from_key(ANVIL0)] + [Account.create() for _ in range(20)]
check("published test key has its published address", accounts[0].address == ANVIL0_ADDR)

for i, acct in enumerate(accounts):
    signed = acct.sign_message(encode_defunct(text=text))
    sig = signed.signature.hex()
    code, out = host("recover", text, sig)
    check(f"recover #{i} (v=27/28)", code == 0 and out == acct.address, out)
    # the same signature with v as 0/1
    raw = bytearray(signed.signature)
    raw[64] -= 27
    code, out = host("recover", text, raw.hex())
    check(f"recover #{i} (v=0/1)", code == 0 and out == acct.address, out)
    # with a 0x prefix
    code, out = host("recover", text, "0x" + sig.removeprefix("0x"))
    check(f"recover #{i} (0x prefix)", code == 0 and out == acct.address, out)

a = accounts[1]
sig = a.sign_message(encode_defunct(text=text)).signature.hex()

# A changed text must not recover to the signer.
other = statement("0" * 64)
code, out = host("recover", other, sig)
check("another text does not recover to the signer", out != a.address, out)
code, out = host("recover", text + " ", sig)
check("a trailing space does not recover to the signer", out != a.address, out)

# A flipped signature bit must not recover to the signer.
raw = bytearray.fromhex(sig.removeprefix("0x"))
raw[10] ^= 0x01
code, out = host("recover", text, raw.hex())
check("a flipped bit does not recover to the signer", out != a.address, out)

# Malformed signatures are refused.
for name, bad in (("short", "ab" * 64), ("long", "ab" * 66), ("not hex", "zz" * 65),
                  ("bad recovery id", "11" * 64 + "05"), ("r = 0", "00" * 32 + "11" * 32 + "1b")):
    code, out = host("recover", text, bad)
    check(f"refuses a malformed signature ({name})", code != 0 and out.startswith("error"), out)

# Signing in C++: the other library must recover the same address, and since
# both are deterministic (RFC 6979, low s) the signatures must be identical.
for i, acct in enumerate(accounts[:8]):
    key = acct.key.hex().removeprefix("0x")
    code, out = host("address", key)
    check(f"address of key #{i}", code == 0 and out == acct.address, out)
    code, sig_hex = host("sign", text, key)
    rec = Account.recover_message(encode_defunct(text=text), signature=bytes.fromhex(sig_hex.removeprefix("0x"))) if code == 0 else ""
    check(f"signature #{i} recovers in eth-account", rec == acct.address, f"{rec} {sig_hex}")
    theirs = "0x" + acct.sign_message(encode_defunct(text=text)).signature.hex().removeprefix("0x")
    check(f"signature #{i} is identical to eth-account's", sig_hex == theirs)
    s_val = int(sig_hex[2 + 64:2 + 128], 16)
    check(f"signature #{i} has low s", s_val <= 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A0)

# Generated keys: distinct, valid, and usable by the other library.
keys = [host("genkey")[1] for _ in range(50)]
check("50 generated keys are 32 bytes of hex", all(len(k) == 64 and int(k, 16) > 0 for k in keys))
check("50 generated keys are all different", len(set(keys)) == 50)
k = keys[0]
code, addr = host("address", k)
check("a generated key has the address eth-account derives", addr == Account.from_key("0x" + k).address, addr)

# Unusable keys are refused.
for name, bad in (("zero", "00" * 32), ("group order", "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"),
                  ("all ones", "ff" * 32)):
    code, out = host("sign", text, bad)
    check(f"refuses to sign with an unusable key ({name})", code != 0 and out.startswith("error"), out)

print(f"\n{passed} passed, {failed} failed")
sys.exit(1 if failed else 0)
