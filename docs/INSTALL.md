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

**Not sure which Linux file you need?** Open the Terminal app, type `uname -m` and press Enter. If it
answers `x86_64`, you need the files with `x86_64` in the name. If it answers `aarch64`, you need the
files with `aarch64` in the name.

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

## Step 2: Download the Manifesto app

1. Open this page: <https://github.com/roxanneen/Manifesto-app/releases/latest>
2. Scroll down to **Assets** and click the file for your computer. Pick the name that ends in
   `.lgx`. Ignore the files ending in `.sha256` and the two called **Source code**.

   | Your computer | File to download |
   |---|---|
   | Mac with an Apple chip | `manifesto_ui-darwin-arm64.lgx` |
   | Linux, Intel or AMD | `manifesto_ui-linux-x86_64.lgx` |
   | Linux, ARM | `manifesto_ui-linux-aarch64.lgx` |

3. Your browser saves the file, usually in your Downloads folder. You should now see a file there
   whose name starts with `manifesto_ui-` and ends in `.lgx`. Leave it closed. Basecamp opens it in
   the next step.

## Step 3: Add the Manifesto app to Basecamp

1. In the Basecamp window, look at the icons along the side. Hold the mouse over each one for a
   moment to see its name, and click **Package Manager**. You are in the right place when you see
   the heading **Packages** and a button called **Install Local Package**.
2. Click **Install Local Package**.
3. A file window opens. Go to your Downloads folder and click the file whose name starts with
   `manifesto_ui-` and ends in `.lgx`. Then click **Open**.
4. A box titled **Install Package?** appears. It says the package is unsigned. That is expected for
   this app, and it has nothing to do with your own signature on the manifesto. If the box lists
   optional packages, leave them unticked, because the Manifesto app does not need anything else.
   Click the button that confirms the install. Nothing is installed until you do.
5. Wait a few seconds for the install to finish.
6. **Manifesto** now appears among your apps in Basecamp. Look for it in the same side bar or in the
   list of applications, and click it to open it.

If you do not see Manifesto, quit Basecamp and open it again. Closing the window may not be enough.
On a Mac, click the Basecamp window and press the Command and Q keys together. On Linux, choose
**Quit** from Basecamp's menu, or right-click its small icon near the clock and choose **Quit**.

## Step 4: Read and sign

1. In the Manifesto app, read the text. It is "Logos: A Declaration of Independence in Cyberspace"
   by Jarrad Hope, the same text as on <https://logos.co/manifesto>.
2. Below the text, under the heading **Sign it**, you see one line that starts with **I sign the
   Logos manifesto**. That line is what you sign. It also holds a long code of letters and numbers,
   which identifies the exact text you just read, and today's date.
3. Click **Sign the manifesto**.
4. A green box says **You signed the manifesto**. You are done.

Your signature is stored as a small piece of text called a receipt. To see it, open the tab whose
name starts with **Your signatures**. To share it with someone, click **Copy receipt** and paste it
wherever you like. Nothing is sent anywhere unless you share it yourself.

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
| Basecamp says the package is unsigned | This is about the app file and has nothing to do with your signature on the manifesto. The file carries no publisher's seal, which is expected for this app. If you downloaded it from the page in Step 2, carry on |
| Manifesto does not appear after installing | Quit Basecamp and open it again (see the end of Step 3) |
| The app says **Signing is off** | Quit Basecamp and open it again (see the end of Step 3). If the message stays, report it using the link below and copy the message into your report |

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
- Manifesto app download: https://github.com/roxanneen/Manifesto-app/releases/latest
  Files: manifesto_ui-darwin-arm64.lgx, manifesto_ui-linux-x86_64.lgx, manifesto_ui-linux-aarch64.lgx
  Ignore the .sha256 files and the Source code files.
- In Basecamp: Package Manager, then Install Local Package, then choose the .lgx file, then
  confirm the "Install Package?" box. It says the package is unsigned; that is expected. Leave any
  optional packages unticked.
- Then open the Manifesto app and click "Sign the manifesto". A green box confirms it.
- The Manifesto app and Basecamp never need an account, a wallet, a payment or a password. The one
  exception is my own computer asking for my computer password to allow a newly downloaded app to
  open. If anything else asks for a password, payment or personal details, tell me to stop.
```

## For AI assistants and people who prefer a terminal

The Manifesto download can be done in a terminal. Replace `<platform>` with `darwin-arm64`,
`linux-x86_64` or `linux-aarch64`.

```bash
curl -L -o manifesto_ui.lgx https://github.com/roxanneen/Manifesto-app/releases/latest/download/manifesto_ui-<platform>.lgx
```

```bash
curl -L -o manifesto_ui.lgx.sha256 https://github.com/roxanneen/Manifesto-app/releases/latest/download/manifesto_ui-<platform>.lgx.sha256
```

The `.sha256` file holds the expected checksum of the package. Compare it with the output of
`shasum -a 256 manifesto_ui.lgx` on macOS or `sha256sum manifesto_ui.lgx` on Linux.

Then install the file through Basecamp's **Package Manager** with **Install Local Package**, as in
Step 3. If you have the `lgpm` command-line tool, it does the same. Set `PROFILE` first:
`PROFILE="$HOME/Library/Application Support/Logos/LogosBasecamp"` on macOS or
`PROFILE="$HOME/.local/share/Logos/LogosBasecamp"` on Linux.

```bash
lgpm --modules-dir "$PROFILE/modules" --ui-plugins-dir "$PROFILE/plugins" --allow-unsigned install --file manifesto_ui.lgx
```

Quit and reopen Basecamp after an `lgpm` install.

The signing key is the file `module_data/manifesto_ui/signing-key` inside that profile folder, and
receipts are in `module_data/manifesto_ui/receipts/`.
