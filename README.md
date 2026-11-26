# Manifesto for Logos Basecamp

`manifesto_ui` lets a person read the Logos manifesto in Basecamp and sign it in one click. The app
makes a signing key for itself on first run, so no account is needed. The result is a small receipt
that anyone can check without a server. It is a `ui_qml` module built with the Logos module builder.

**New here? Follow the [step-by-step guide](docs/INSTALL.md).** It covers installing Basecamp,
adding this app and signing, in plain language. It works on macOS (Apple chip) and Linux desktops
only: not on Windows, Intel Macs, phones or tablets.

The manifesto is "Logos: A Declaration of Independence in Cyberspace" by Jarrad Hope. Source:
<https://logos.co/manifesto>. See [NOTICE.md](NOTICE.md).

![The app after signing](docs/screenshot.png)

## Status

Version 0.1.0, 26 November 2026. This version signs and verifies on one computer and exports the
receipt. There is no shared list of signers yet.

| Platform | State |
|---|---|
| macOS, Apple Silicon | Built, tested headless, and used in Basecamp |
| Linux x86_64 and ARM64 | Built and tested by CI (see the Actions tab for the latest run); not tried in Basecamp by hand |
| Intel Macs, phones, tablets | Not supported. Basecamp itself has no build for them |
| Windows | Not supported. The Windows code paths have never been compiled |

Signing with the app's own key is tested end to end (see Tests). Signing with an account from the
Signer app is implemented but has not been exercised, because approving a request there needs a
person.

## Install

The full walkthrough, including installing Basecamp itself, is in
[docs/INSTALL.md](docs/INSTALL.md). In short:

1. Install [Logos Basecamp](https://github.com/logos-co/logos-basecamp/releases/latest) 0.3.2 or
   newer.
2. Download the `.lgx` for your computer from the
   [latest release](https://github.com/roxanneen/Basecamp-manifesto-app/releases/latest):

   | Asset | Platform |
   |---|---|
   | `manifesto_ui-darwin-arm64.lgx` | macOS, Apple chip |
   | `manifesto_ui-linux-x86_64.lgx` | Linux x86_64 |
   | `manifesto_ui-linux-aarch64.lgx` | Linux ARM64 |

3. In Basecamp open **Package Manager**, click **Install Local Package**, choose the file and
   confirm the **Install Package?** box.
4. Open **Manifesto** and click **Sign the manifesto**.

The package is not signed; each one ships with a `.sha256` file to compare against. The app needs no
other module. If `signer_manager_module` happens to be installed, its accounts are offered as an
extra way to sign.

## What is signed

The app does not sign the whole text. It signs one line that names the text by its hash:

```
I sign the Logos manifesto, "Logos: A Declaration of Independence in Cyberspace". Text sha256: <64 hex>. Signed on <YYYY-MM-DD>.
```

- The hash is the sha256 of `src/manifesto.txt`, byte for byte. That file is baked into the build,
  so the file is the version being signed. Changing one character changes the hash.
- The date is the signer's own statement (UTC). Nothing timestamps it independently.
- The line is plain printable text on one line. The app shows it above the Sign button. When a
  Signer account is used, the Signer shows exactly this line before the person approves, and it
  refuses control characters.

`src/manifesto.txt` is the manifesto by Jarrad Hope, taken from its source,
<https://logos.co/manifesto>, on 26 November 2026 and checked
against the rendered page word for word: the title, the author, the abstract, the keywords and all
29 body paragraphs are identical and in the same order (1,260 words). The contact line under the
author's name (email, site, PGP) is left out. The page has no official plain-text form, so the line
breaks are this app's choice: the title and the author's name on the first two lines, then one
paragraph per line with a blank line between (the abstract starts with `Abstract. `), and `Keywords`
on a line of its own directly above the list. If Logos publishes a canonical text file, replace this one; the hash
and every new signature follow it.

## How a signature is made

**With the app's own key (the default).** On first run the backend takes 32 random bytes from the
operating system, checks they form a usable secp256k1 key, and writes them as hex to
`module_data/manifesto_ui/signing-key` in the Basecamp profile. The file is created owner-only and is
never overwritten: if it cannot be read later, signing switches off and the file is left alone. The
key has no password. It never leaves the backend process and no property, log line or receipt
carries it. Sign builds the statement, signs it (deterministic, RFC 6979), runs the same check a
stranger would run, and saves the receipt.

Back up that file to keep the same signing address on another computer or after a reinstall. Anyone
who can read it can sign the statement as that address, and nothing else: the key is used for
nothing but this.

**With an account from the Signer (optional).** `signer_manager_module` is an optional dependency.
The app loads and signs without it. If it is installed and lists accounts, they appear in a "Sign
with" box next to the app's key.

1. The backend calls `request_approval` with `{ address, purpose, legs: [{ kind: "message", text }] }`.
   That returns at once with a handle and a one-time token.
2. The view raises the `evm.signing.approve` intent with the handle, so the Signer app comes forward
   showing the line. The person approves with their wallet password, device or PIN.
3. The backend polls `approval_status`, then `fetch_result`, then `ack_result`.
4. The signature must recover to the chosen account over the statement this module built, or it is
   discarded.

## The receipt and how to verify it

```json
{
  "type": "logos-manifesto-signature",
  "version": 1,
  "scheme": "eip191",
  "statement": "I sign the Logos manifesto, \"…\". Text sha256: …. Signed on 2026-11-26.",
  "address": "0x…",
  "signature": "0x…"
}
```

The signature is an Ethereum personal message signature (EIP-191), 65 bytes `r‖s‖v`. To verify:

1. Recover the signer from `signature` and `statement`. Any EIP-191 tool does this, for example
   `cast wallet verify --address <address> "<statement>" <signature>` or
   `eth_account.Account.recover_message`.
2. Compare the result with `address`.
3. Compare the sha256 in the statement with the sha256 of the manifesto text you hold.

The app's Check a signature tab does the same three steps on the local machine
(`src/sigcheck.cpp`: Keccak-256, public key recovery through libsecp256k1, EIP-55 addresses).

What a valid receipt shows: the holder of that account's key approved that exact line. What it does
not show: who the holder is, or that one person signed once. A person can sign from many accounts.

Receipts are kept in `module_data/manifesto_ui/receipts/` in the Basecamp profile and go nowhere
until the person copies one out. A receipt from the app's own key points to no other activity,
because that key does nothing else. A receipt from a Signer account links that account to the
manifesto.

## Build

Requires Nix with flakes.

```bash
nix build .#lgx-portable -o result-lgx-portable   # the installable package
nix build .#ui-dev -o result-ui-dev               # standalone dev host for headless tests
nix build .#test-framework -o result-mcp          # QML inspector client
```

The package lands in `result-lgx-portable/logos-manifesto_ui-module.lgx`; install it as above.

## Tests

`tests/sig_interop.py` (122 checks) holds the C++ signature code to eth-account: Keccak over block
boundaries, recovery for 21 keys in three signature spellings, refusals for a changed text, a flipped
bit and malformed signatures, signatures made in C++ that are byte-identical to eth-account's,
generated keys, and refusal of unusable keys.

```bash
clang++ -std=c++17 -Isrc -I<secp256k1 include> src/sigcheck.cpp tests/sig_host.cpp -L<secp256k1 lib> -lsecp256k1 -o tests/work/sig_host
python3 -m venv tests/.venv && tests/.venv/bin/pip install eth-account
tests/.venv/bin/python tests/sig_interop.py tests/work/sig_host
```

`tests/ui-smoke.mjs` (26 checks, each run from an empty profile) launches the standalone host headless with an isolated profile. It
checks the text, hash and statement in the view; that the app made its key in an owner-only file
and shows it nowhere; that Sign produces one receipt and signing again replaces it; and the Check a
signature tab over the app's own receipt and over receipts made by eth-account
(`tests/make_fixtures.py`): valid, another text version, wrong address, edited statement, unrelated
text. It then restarts the app three times: the key and receipt survive a restart, a damaged key file
is left untouched with signing off and the receipt still listed, and restoring the file restores the
address. `tests/check_receipt.py` then verifies the receipt the app produced with eth-account.

```bash
tests/.venv/bin/python tests/make_fixtures.py tests/work/fixtures.json
MANIFESTO_RUN=$PWD/result-ui-dev/bin/run-logos-standalone-ui MANIFESTO_WORK=$PWD/tests/work/smoke \
MANIFESTO_FIXTURES=$PWD/tests/work/fixtures.json node tests/ui-smoke.mjs
tests/.venv/bin/python tests/check_receipt.py tests/work/smoke/signed-receipt.json \
  tests/work/smoke/home/basecamp/module_data/manifesto_ui/signing-key
```

Remove `tests/.venv` before a Nix build: a path flake copies the whole folder into the store.

Environment hook used by tests only: `MANIFESTO_DATE` pins the date in the statement.

## Not in this version

- A shared list of signers. The plan is to publish receipts on a Logos Messaging topic through
  `delivery_module`, have every copy of the app verify each one before counting it, and publish
  snapshots of the list to Logos Storage.
- One signature per person. A key costs nothing to make, so a count of receipts is a count of keys.
- A password or hardware protection for the app's own key, and a way to export or replace it from
  the view.

## Licence

The code is licensed under MIT or Apache-2.0, at your option. The manifesto text is not covered by
those licences; see [NOTICE.md](NOTICE.md).
