# Sign the Logos manifesto: step-by-step guide

This guide takes you from nothing installed to a signed manifesto. It takes about ten minutes. You
do not need an account, a wallet, a password or any technical knowledge.

> **Please note.** The Manifesto app is a personal, experimental hobby project. It is not an official
> Logos product and it has not been audited. It is provided as is, with no warranty of any kind. It
> is not affiliated with or endorsed by Logos, by Basecamp, or by the author of the manifesto.

You will do three things:

1. Install Logos Basecamp, the desktop app that runs Logos apps.
2. Add the Manifesto app to Basecamp.
3. Read the manifesto and click Sign.

## Before you start: check your computer

This works on **desktop and laptop computers running macOS or Linux**.

| Your computer | Does it work? |
|---|---|
| Mac with an Apple chip (M1, M2, M3, M4 or newer) | Yes |
| Linux desktop or laptop (Intel, AMD or ARM, 64-bit) | Yes |
| Windows | No, not yet. Basecamp's download page has a Windows file (`setup.exe`), but the Manifesto app has no Windows version, so do not continue on Windows |
| Mac with an Intel processor | No |
| Phone or tablet (iPhone, iPad, Android) | No |

**Not sure which Mac you have?** Click the Apple logo in the top-left corner of the screen, then
**About This Mac**. If the line says **Chip: Apple M1** (or M2, M3, M4), you are fine. If it says
**Processor: Intel**, this will not work on that Mac.

**Not sure which Linux file of Basecamp you need?** Open the Terminal app, type `uname -m` and press
Enter. If it answers `x86_64`, you need the Basecamp file with `x86_64` in the name. If it answers
`aarch64`, you need the one with `aarch64` in the name.

## Step 1: Install Logos Basecamp

1. Open this page: <https://github.com/logos-co/logos-basecamp/releases/latest>
2. Scroll down to the list called **Assets**. Click the file for your computer to download it:
   - **Mac:** the file whose name ends in `aarch64.dmg`
   - **Linux (Intel or AMD):** the file whose name ends in `x86_64.AppImage`
   - **Linux (ARM):** the file whose name ends in `aarch64.AppImage`

   The list also holds files you do not need. Ignore the one ending in `setup.exe`. It is for
   Windows, and the Manifesto app has no Windows version. Ignore the two called **Source code** as
   well.
3. Install it:

   **On a Mac**
   1. Open your Downloads folder and double-click the `.dmg` file you just downloaded.
   2. A window opens. Drag the **LogosBasecamp** icon onto the **Applications** folder in that window.
   3. Open your Applications folder and double-click **LogosBasecamp**.
   4. The first time, macOS may ask whether you are sure you want to open an app downloaded from the
      internet. Click **Open**.

   **On Linux**
   1. Open your Downloads folder and right-click the `.AppImage` file.
   2. Choose **Properties**, then find **Permissions**. Switch on the option that allows the file to
      run as a program. Its name differs between Linux versions, for example **Allow executing file
      as program** or **Executable as Program**. Close the window.

      If you cannot find that option, open the Terminal app, paste this line and press Enter:

      `chmod +x ~/Downloads/LogosBasecamp-*.AppImage`

      It prints nothing when it works.
   3. Double-click the file to start Basecamp.

4. Basecamp opens. You should see its main window with icons along the side. It may show a welcome
   page. You can leave that as it is. Keep Basecamp open and go on to Step 2.

You need Basecamp version 0.3.2 or newer. The link above always gives you the newest one.

## Step 2: Add the Manifesto app to Basecamp

There are two ways. The first is shorter and also brings you updates later.

### Way 1 (recommended): from the catalogue

A catalogue is a list of apps that Basecamp can install for you. This one is published by the same
person as the Manifesto app, under the name **thehat**, and its packages are signed.

1. In the Basecamp window, look at the icons along the side. Hold the mouse over each one for a
   moment to see its name, and click **Package Manager**.
2. Click **Manage Repositories**. If you do not see that button, open Basecamp's **Settings** and
   choose **Package Repositories**.
3. Find the field called **Add a repository**. Copy the address below, paste it into that field and
   add it:

   ```
   https://raw.githubusercontent.com/roxanneen/basecamp-apps/main/logos-repo.json
   ```

4. Go back to **Package Manager**. Find **Manifesto** in the list and install it.

   Newer versions of Basecamp then show who signed it, with a short code called a fingerprint, and
   a button **Trust & install**. The publisher is **thehat** and the fingerprint must be exactly:

   **`6AB9 AD2D 6E8D FA6D`**

   If it matches, click **Trust & install**. Basecamp remembers this publisher (you can see it later
   under **Settings**, **Trusted Publishers**) and will not ask again. If the fingerprint is
   different, stop: the package is not from this publisher.

   Basecamp 0.3.2 does not show this step yet and installs straight away.
5. **Manifesto** now appears among your apps in Basecamp. Click it to open it.

The same catalogue also lists three other apps by the same publisher (OneSecret, Chess and a
Presence pilot). You do not need them to sign the manifesto. The catalogue has its own page, with
the address and a copy button: <https://roxanneen.github.io/basecamp-apps/>

### Way 2: download the file yourself

Use this if the catalogue does not work for you. The file is the same signed package the catalogue
serves, and it is one file for every supported computer.

1. Click this link to download the app: <https://github.com/roxanneen/Manifesto-app/releases/latest/download/manifesto_ui.lgx>
2. Your browser saves a file called `manifesto_ui.lgx`, usually in your Downloads folder. Leave it
   closed. Basecamp opens it in the next steps.
3. In the Basecamp window, look at the icons along the side. Hold the mouse over each one for a
   moment to see its name, and click **Package Manager**. You are in the right place when you see
   the heading **Packages** and a button called **Install Local Package**.
4. Click **Install Local Package**.
5. A file window opens. Go to your Downloads folder and click the file called `manifesto_ui.lgx`.
   Then click **Open**.
6. A box titled **Install Package?** appears. The package is signed by the publisher **thehat**.
   Because you added the file by hand, Basecamp may say it does not know this publisher yet. That is
   expected. The Manifesto app needs no other packages. Click the button that confirms the install.
   Nothing is installed until you do.
7. Wait a few seconds for the install to finish.
8. **Manifesto** now appears among your apps in Basecamp. Look for it in the same side bar or in the
   list of applications, and click it to open it.

If you do not see Manifesto, quit Basecamp and open it again. Closing the window may not be enough.
On a Mac, click the Basecamp window and press the Command and Q keys together. On Linux, choose
**Quit** from Basecamp's menu, or right-click its small icon near the clock and choose **Quit**.

## Step 3: Read and sign

1. In the Manifesto app, read the text. It is "Logos: A Declaration of Independence in Cyberspace"
   by Jarrad Hope, the text on <https://logos.co/manifesto>, ending with the line "Hack the planet!"
   from the copy inscribed on Bitcoin.
2. Below the text, under the heading **Sign it**, you see one line that starts with **I sign the
   Logos manifesto**. That line is what you sign. It names the copy of the manifesto inscribed on
   Bitcoin, by two long codes of letters and numbers, and today's date. Because that copy can never
   change, your signature always points at the same manifesto.
3. Click **Sign the manifesto**.
4. A green box says **You signed the manifesto**. You are done.

Your signature is stored as a small piece of text called a receipt. To see it, open the tab whose
name starts with **Your signatures**. To share it with someone, click **Copy receipt** and paste it
wherever you like. Nothing is sent anywhere unless you share it yourself.

To check a receipt someone sent you, open the **Check a signature** tab, paste the receipt and click
**Check**. A green box means the signature is genuine and names the same copy of the manifesto, the one
inscribed on Bitcoin. A yellow box means the signature is genuine but names a different version, for
example one made with an older version of this app.

## What happens on your computer

- The first time it opens, the app creates a signing key for you. A signing key is a secret number
  that only your computer has. It is what makes the signature yours.
- The key stays on your computer in one file that only your user account can read. The file is not
  locked with a password, so someone who can use your computer account could copy it and sign the
  manifesto as you. The key holds no money and opens no account. This app uses it for signing the
  manifesto and nothing else.
- Anyone who has your receipt can check that it is genuine, without asking you or any company. They
  cannot use it to sign anything in your name.
- The app does not ask for your name, email or any other personal detail.

To sign with the same key on a new computer, copy the key file. The app shows where the file is, in
small text under the line you sign. On the new computer, install the Manifesto app and open it once,
then quit Basecamp and put your copied file in the same place there, replacing the one the app made.
Keep the file private, because anyone who has a copy can sign the manifesto as you. If this sounds
difficult, skip it. A new computer makes a new key and you can sign again.

## If something goes wrong

| What you see | What to do |
|---|---|
| You use Windows, an Intel Mac, a phone or a tablet | The Manifesto app has no version for these, even though Basecamp offers a Windows file. See the table at the top of this guide |
| macOS will not open Basecamp | Open **System Settings**, then **Privacy & Security**, scroll down and click **Open Anyway** next to LogosBasecamp. Your Mac may ask for your Mac password to allow this |
| On Linux, double-clicking the AppImage does nothing | The file may not be allowed to run yet. Repeat the Linux part of Step 1. If it still does not start, open a terminal in your Downloads folder, type `./` followed by the file name, press Enter, and copy the message it prints into a report (link below) |
| Basecamp says it does not know the publisher, or that the package is unsigned | This is about the app file and has nothing to do with your signature on the manifesto. Check that you downloaded the file from the link in Step 2, then carry on. Installing from the catalogue (Way 1) avoids this message |
| Manifesto does not appear after installing | Quit Basecamp and open it again (see the end of Step 2) |
| The app says **Signing is off** | Quit Basecamp and open it again (see the end of Step 2). If the message stays, report it using the link below and copy the message into your report |

Report a problem here: <https://github.com/roxanneen/Manifesto-app/issues>. Reporting needs
a free GitHub account. Installing and signing do not.

## Using an AI assistant to help you

You can ask an AI assistant such as Claude or ChatGPT to walk you through this. Copy the text below
into the assistant and it will guide you one step at a time.

```text
Please help me install an app, one small step at a time. Wait for me to confirm each step before
giving the next one. I am not technical.

Goal: sign the Logos manifesto using the Manifesto app in Logos Basecamp.
The Manifesto app is an unofficial hobby project. It is not made or endorsed by Logos.

Follow this guide. If you cannot open the link, use the facts below and do not invent other steps.
https://github.com/roxanneen/Manifesto-app/blob/main/docs/INSTALL.md

Facts you need:
- It only works on macOS with an Apple chip (M1 or newer) and on 64-bit Linux desktops
  (x86_64 or aarch64). It does not work on Windows, Intel Macs, phones or tablets. Check this with
  me first and stop if my computer is not supported.
- Basecamp download: https://github.com/logos-co/logos-basecamp/releases/latest
  Mac file ends in aarch64.dmg. Linux files end in x86_64.AppImage or aarch64.AppImage.
  Ignore the setup.exe file (Windows, not supported here) and the Source code files.
- Mac: open the .dmg, drag LogosBasecamp onto Applications, open it from Applications.
  Linux: allow the .AppImage file to run as a program, then double-click it.
- Preferred way to add the app: in Basecamp open Package Manager, then Manage Repositories (or
  Settings, then Package Repositories), paste this address into "Add a repository" and add it:
  https://raw.githubusercontent.com/roxanneen/basecamp-apps/main/logos-repo.json
  Then install "Manifesto" from the Package Manager list. It is signed by the publisher "thehat".
  If Basecamp shows a fingerprint and a "Trust & install" button, the fingerprint must be exactly
  6AB9 AD2D 6E8D FA6D. If it matches, click "Trust & install"; if not, stop.
- Fallback: download this one file (it is the same for every supported computer, and signed by
  "thehat"): https://github.com/roxanneen/Manifesto-app/releases/latest/download/manifesto_ui.lgx
  Then in Basecamp: Package Manager, Install Local Package, choose the file, confirm the
  "Install Package?" box. Basecamp may say it does not know the publisher yet; that is expected
  for a file added by hand. The app needs no other packages.
- Then open the Manifesto app and click "Sign the manifesto". A green box confirms it.
- The Manifesto app and Basecamp never need an account, a wallet, a payment or a password. The one
  exception is my own computer asking for my computer password to allow a newly downloaded app to
  open. If anything else asks for a password, payment or personal details, tell me to stop.
```

## For AI assistants and people who prefer a terminal

The Manifesto download can be done in a terminal. It is one file for every supported computer.

```bash
curl -L -o manifesto_ui.lgx https://github.com/roxanneen/Manifesto-app/releases/latest/download/manifesto_ui.lgx
```

```bash
curl -L -o manifesto_ui.lgx.sha256 https://github.com/roxanneen/Manifesto-app/releases/latest/download/manifesto_ui.lgx.sha256
```

The `.sha256` file holds the expected checksum of the package. Compare it with the output of
`shasum -a 256 manifesto_ui.lgx` on macOS or `sha256sum manifesto_ui.lgx` on Linux. The package is
signed by the publisher key listed at <https://github.com/roxanneen/basecamp-apps>.

Then install the file through Basecamp's **Package Manager** with **Install Local Package**, as in
Step 2, Way 2. If you have the `lgpm` command-line tool, it does the same. Set `PROFILE` first:
`PROFILE="$HOME/Library/Application Support/Logos/LogosBasecamp"` on macOS or
`PROFILE="$HOME/.local/share/Logos/LogosBasecamp"` on Linux.

```bash
lgpm --modules-dir "$PROFILE/modules" --ui-plugins-dir "$PROFILE/plugins" --allow-unsigned install --file manifesto_ui.lgx
```

Quit and reopen Basecamp after an `lgpm` install.

The signing key is the file `module_data/manifesto_ui/signing-key` inside that profile folder, and
receipts are in `module_data/manifesto_ui/receipts/`.
