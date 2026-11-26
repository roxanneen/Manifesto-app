#!/usr/bin/env python3
"""Write test receipts for tests/ui-smoke.mjs, signed with a throwaway key by
an independent implementation (eth-account). Usage: make_fixtures.py <out.json>"""
import hashlib
import json
import os
import sys

from eth_account import Account
from eth_account.messages import encode_defunct

TITLE = "Logos: A Declaration of Independence in Cyberspace"
here = os.path.dirname(os.path.abspath(__file__))
text_hash = hashlib.sha256(open(os.path.join(here, "..", "src", "manifesto.txt"), "rb").read()).hexdigest()


def statement(h, date="2026-11-26"):
    return f'I sign the Logos manifesto, "{TITLE}". Text sha256: {h}. Signed on {date}.'


def receipt(acct, text):
    sig = acct.sign_message(encode_defunct(text=text)).signature.hex()
    return {"type": "logos-manifesto-signature", "version": 1, "scheme": "eip191", "statement": text,
            "address": acct.address, "signature": "0x" + sig.removeprefix("0x")}


a, b = Account.create(), Account.create()
valid = receipt(a, statement(text_hash))
other_text = receipt(a, statement("ab" * 32))
wrong_address = dict(valid, address=b.address)
edited = dict(valid, statement=statement(text_hash, "2025-01-01"))
not_ours = receipt(a, "I agree to transfer everything.")
json.dump({"hash": text_hash, "address": a.address, "valid": valid, "otherText": other_text,
           "wrongAddress": wrong_address, "edited": edited, "notOurs": not_ours},
          open(sys.argv[1], "w"), indent=2)
print("wrote", sys.argv[1])
