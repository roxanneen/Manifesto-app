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
text_hash = hashlib.sha256(open(os.path.join(here, "..", "src", "manifesto.txt"), "rb").read()).hexdigest()
signer = Account.recover_message(encode_defunct(text=r["statement"]), signature=bytes.fromhex(r["signature"][2:]))
m = re.fullmatch(r'I sign the Logos manifesto, "(.+)"\. Text sha256: ([0-9a-f]{64})\. Signed on (\d{4}-\d{2}-\d{2})\.', r["statement"])
checks = [("signature recovers to the receipt's address", signer == r["address"]),
          ("statement has the expected form", m is not None),
          ("statement names the bundled text's sha256", bool(m) and m.group(2) == text_hash)]
if len(sys.argv) > 2:
    key = open(sys.argv[2]).read().strip()
    checks.append(("address is the key file's address", Account.from_key("0x" + key).address == r["address"]))
failed = 0
for name, ok in checks:
    print(("  ok   " if ok else "  FAIL ") + name)
    failed += not ok
print(f"\n{len(checks) - failed} passed, {failed} failed")
sys.exit(1 if failed else 0)
