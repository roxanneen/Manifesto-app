#!/usr/bin/env python3
"""Verify a receipt the app made, with an independent implementation.
Usage: check_receipt.py <receipt.json> [<key file>]
With the key file it also checks the receipt's address is that key's address."""
import hashlib
import json
import os
import re
import sys

from eth_account import Account
from eth_account.messages import encode_defunct

r = json.load(open(sys.argv[1]))
here = os.path.dirname(os.path.abspath(__file__))
INSCRIPTION = "1c3990078b6a4e68d2e5c87c88cff9782f261cb22b5375bfc1cdbe711cbf4686i0"
INSCRIPTION_SHA = "441e1c2ae342848fb6298e62cc427d22c2a954b73b6e1f2899da25072757aead"
signer = Account.recover_message(encode_defunct(text=r["statement"]), signature=bytes.fromhex(r["signature"][2:]))
m = re.fullmatch(r'I sign the Logos manifesto, "(.+)", inscribed on Bitcoin as ([0-9a-f]{64}i[0-9]+)\. Image sha256: ([0-9a-f]{64})\. Signed on (\d{4}-\d{2}-\d{2})\.', r["statement"])
checks = [("signature recovers to the receipt's address", signer == r["address"]),
          ("statement has the expected form", m is not None),
          ("statement names the manifesto's title", bool(m) and m.group(1) == "Logos: A Declaration of Independence in Cyberspace"),
          ("statement names the Bitcoin inscription", bool(m) and m.group(2) == INSCRIPTION),
          ("statement names the inscribed file's sha256", bool(m) and m.group(3) == INSCRIPTION_SHA)]
if len(sys.argv) > 2:
    key = open(sys.argv[2]).read().strip()
    checks.append(("address is the key file's address", Account.from_key("0x" + key).address == r["address"]))
failed = 0
for name, ok in checks:
    print(("  ok   " if ok else "  FAIL ") + name)
    failed += not ok
print(f"\n{len(checks) - failed} passed, {failed} failed")
sys.exit(1 if failed else 0)
