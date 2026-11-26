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
# The same valid receipt as it arrives after a trip through a chat app or an
# email. Each of these is still a genuine receipt and must verify.
pretty = json.dumps(valid, indent=4, sort_keys=True)
no_backslashes = pretty.replace('\\"', '"')
st, ad, sg = valid["statement"], valid["address"], valid["signature"]
good = {
    "noBackslashes": no_backslashes,
    "curly": no_backslashes.replace('"Logos', '“Logos').replace('Cyberspace"', 'Cyberspace”'),
    "curlyJson": json.dumps(dict(valid, statement=st.replace('"Logos', '“Logos').replace('Cyberspace"', 'Cyberspace”')), ensure_ascii=False),
    "wrapped": no_backslashes.replace("Declaration of ", "Declaration of\n      ").replace("Text sha256:", "Text\n sha256:"),
    "sentence": f"Here is my signature: {st} Address {ad} and signature {sg} thanks!",
    "splitSignature": no_backslashes.replace(sg, sg[:60] + "\n" + sg[60:100] + " " + sg[100:]),
    "zeroWidth": no_backslashes.replace(ad, ad[:10] + "​" + ad[10:]).replace(sg, sg[:30] + "­" + sg[30:]),
    "emailQuoted": "\n".join("> " + line for line in no_backslashes.split("\n")),
    "htmlQuotes": no_backslashes.replace('"Logos', '&quot;Logos').replace('Cyberspace"', 'Cyberspace&quot;'),
    "doubleEscaped": pretty.replace('\\"', '\\\\"'),
    "otherAddressFirst": f"Sent from my wallet {b.address}. " + no_backslashes,
}

# Damaged in transit AND not genuine. Every one of these must be refused.
sig_bad = sg[:20] + ("b" if sg[20] == "a" else "a") + sg[21:]
random_sig = "0x" + os.urandom(64).hex() + "1b"
by_b = receipt(b, st)   # b really signed the line
bad = {
    "wrongAddress": no_backslashes.replace(ad, b.address),
    "edited": no_backslashes.replace("2026-11-26", "2025-01-01"),
    "badSignature": no_backslashes.replace(sg, sig_bad),
    # No address in the text: nothing to check the signature against.
    "noAddress": f"{st}\n{sg}",
    "noAddressRandomSignature": f"{st}\n{random_sig}",
    "noAddressEdited": f"{st.replace('2026-11-26', '2020-01-01')}\n{sg}",
    "noAddressBadSignature": f"{st}\n{sig_bad}",
    "randomSignature": no_backslashes.replace(sg, random_sig),
    # Well-formed JSON that names address a but carries b's signature, with
    # b's address planted in another field: the named address did not sign.
    "decoyField": json.dumps({"note": b.address, **dict(by_b, address=ad)}, indent=2),
    "uppercasePrefixDecoy": json.dumps(dict(by_b, address="0X" + ad[2:]), indent=2),
    "signatureOnly": sg,
    "statementOnly": st,
}
loose = {"good": good, "bad": bad}

json.dump({"hash": text_hash, "loose": loose, "address": a.address, "valid": valid, "otherText": other_text,
           "wrongAddress": wrong_address, "edited": edited, "notOurs": not_ours},
          open(sys.argv[1], "w"), indent=2)
print("wrote", sys.argv[1])
