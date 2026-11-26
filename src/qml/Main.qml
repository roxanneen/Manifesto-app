import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// Manifesto view. Runs inside Basecamp; the backend runs in the module's
// ui-host process and is reached through the typed replica logos.module().
// Three surfaces: read and sign, your signatures, check a signature.
// By default the backend signs with a key the app made for itself, so no
// account is needed. If the Signer lists accounts, one of those can be picked
// instead and the signature is then approved in the Signer app.
Item {
    id: root
    objectName: "manifestoRoot"
    implicitWidth: 900
    implicitHeight: 680

    readonly property var backend: (typeof logos !== "undefined" && logos) ? logos.module("manifesto_ui") : null
    property bool ready: false

    // --- backend state (auto-synced PROPs, defensively coerced) ---
    readonly property string manifestoTitle: ready && backend && backend.manifestoTitle ? backend.manifestoTitle : ""
    readonly property string manifestoText: ready && backend && backend.manifestoText ? backend.manifestoText : ""
    readonly property string manifestoHash: ready && backend && backend.manifestoHash ? backend.manifestoHash : ""
    readonly property string statement: ready && backend && backend.statement ? backend.statement : ""
    readonly property string signerState: ready && backend && backend.signerState ? backend.signerState : "checking"
    readonly property string signerInfo: ready && backend && backend.signerInfo ? backend.signerInfo : ""
    readonly property string signState: ready && backend && backend.signState ? backend.signState : "idle"
    readonly property string signInfo: ready && backend && backend.signInfo ? backend.signInfo : ""
    readonly property string pendingHandle: ready && backend && backend.pendingHandle ? backend.pendingHandle : ""
    readonly property string appKeyFile: ready && backend && backend.appKeyFile ? backend.appKeyFile : ""
    readonly property string buildId: ready && backend && backend.buildId ? backend.buildId : ""

    function parse(text, fallback) {
        try { var v = JSON.parse(text); return v === null || v === undefined ? fallback : v }
        catch (e) { return fallback }
    }
    readonly property var accounts: parse(ready && backend && backend.accountsJson ? backend.accountsJson : "[]", [])
    readonly property var receipts: parse(ready && backend && backend.receiptsJson ? backend.receiptsJson : "[]", [])
    readonly property var verifyResult: parse(ready && backend && backend.verifyResultJson ? backend.verifyResultJson : "{}", ({}))

    readonly property bool busy: signState === "requesting" || signState === "awaiting" || signState === "signing"
    // The choice is kept by address: the account list is re-read after every
    // signature and refresh, which would otherwise reset the box to its first row.
    property string chosenAddress: ""
    readonly property var chosen: {
        for (var i = 0; i < accounts.length; i++)
            if (accounts[i].address === chosenAddress) return accounts[i]
        return accounts.length > 0 ? accounts[0] : null
    }
    // If the chosen address leaves the list, fall back openly to the first row
    // (and say so through the text under the statement) instead of keeping a
    // stale choice.
    onAccountsChanged: {
        for (var i = 0; i < accounts.length; i++)
            if (accounts[i].address === chosenAddress) return
        chosenAddress = accounts.length > 0 ? accounts[0].address : ""
    }
    readonly property string unsavedReceipt: ready && backend && backend.unsavedReceiptJson ? backend.unsavedReceiptJson : ""
    readonly property bool canRaiseIntents: typeof logos !== "undefined" && !!logos && typeof logos.request === "function"

    // --- local UI state ---
    property string signerNote: ""
    property string raisedFor: ""

    function shortAddress(a) { return a && a.length > 14 ? a.slice(0, 8) + "…" + a.slice(-6) : (a || "") }
    function accountLabel(a) {
        var name = a.own ? "This app's key  " : (a.label ? a.label + "  " : "Signer account  ")
        return name + shortAddress(a.address) + (a.signed ? "  (signed)" : "")
    }
    readonly property bool ownChosen: chosen !== null && chosen.own === true
    function signerColour(st) {
        if (st === "ready") return Theme.palette.success
        if (st === "checking") return Theme.palette.warning
        return Theme.palette.error
    }
    function signerLabel(st) {
        if (st === "ready") return "ready to sign"
        if (st === "checking") return "starting"
        return "signing is off"
    }

    // Bring the Signer forward on the request that is waiting. The answer is
    // advisory: the request's real state comes from the backend's poll.
    function openSigner(handle) {
        if (!handle) return
        if (!root.canRaiseIntents) {
            root.signerNote = "Open the Signer app and approve the request there."
            return
        }
        logos.request("evm.signing.approve", { handle: handle }, function (res) {
            var e = String((res && res.error) || "")
            if (e === "unavailable" || e === "cancelled")
                root.signerNote = "The Signer app did not open. Install or open it from the sidebar, then approve the request there."
            else if (e !== "")
                root.signerNote = "The Signer could not be opened (" + e + "). Open it from the sidebar and approve the request there."
            else
                root.signerNote = ""
        })
    }
    onPendingHandleChanged: {
        if (pendingHandle !== "" && pendingHandle !== raisedFor) {
            raisedFor = pendingHandle
            openSigner(pendingHandle)
        }
        if (pendingHandle === "") { raisedFor = ""; signerNote = "" }
    }

    Connections {
        target: logos
        function onViewModuleReadyChanged(moduleName, isReady) {
            if (moduleName === "manifesto_ui")
                root.ready = isReady && root.backend !== null
        }
    }
    Component.onCompleted: {
        root.ready = root.backend !== null && logos.isViewModuleReady("manifesto_ui")
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.background
    }

    // Hidden markers for headless tests: exact state words.
    Text { objectName: "signerMarker"; visible: false; text: root.signerState }
    Text { objectName: "signMarker"; visible: false; text: root.signState }
    Text { objectName: "hashMarker"; visible: false; text: root.manifestoHash }
    Text { objectName: "statementMarker"; visible: false; text: root.statement }
    Text { objectName: "accountsMarker"; visible: false; text: String(root.accounts.length) }
    Text { objectName: "ownAddressMarker"; visible: false; text: root.ready && root.backend && root.backend.appKeyAddress ? root.backend.appKeyAddress : "" }
    Text { objectName: "receiptsMarker"; visible: false; text: String(root.receipts.length) }
    Text { objectName: "verifyAddressMarker"; visible: false; text: root.verifyResult.address || "" }
    Text {
        objectName: "verifyMarker"; visible: false
        text: !root.verifyResult.checked ? "none"
              : (root.verifyResult.valid ? (root.verifyResult.sameText ? "valid" : "valid-other-text") : "invalid")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.large
        spacing: Theme.spacing.medium

        // ---------------------------------------------------------------- header
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.medium
            LogosText {
                text: "Manifesto"
                font.pixelSize: Theme.typography.panelTitleText
                font.weight: Theme.typography.weightBold
            }
            LogosBadge {
                objectName: "signerBadge"
                text: root.signerLabel(root.signerState)
                color: root.signerColour(root.signerState)
            }
            LogosText {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: root.signerInfo
                color: Theme.palette.textTertiary
                font.pixelSize: Theme.typography.secondaryText
            }
            LogosButton {
                objectName: "refreshButton"
                text: "Refresh"
                compact: true
                enabled: root.ready
                onClicked: root.backend.refreshAccounts()
            }
        }

        LogosTabBar {
            id: tabs
            objectName: "tabs"
            Layout.fillWidth: true
            LogosTabButton { text: "Read and sign" }
            LogosTabButton { text: root.receipts.length > 0 ? "Your signatures (" + root.receipts.length + ")" : "Your signatures" }
            LogosTabButton { text: "Check a signature" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // ===================================================== read and sign
            ColumnLayout {
                spacing: Theme.spacing.medium

                LogosFrame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    backgroundColor: Theme.palette.surfaceRaised
                    borderColor: Theme.palette.borderSecondary
                    radius: Theme.spacing.radiusLarge
                    padding: Theme.spacing.large
                    contentItem: ScrollView {
                        id: textScroll
                        objectName: "manifestoScroll"
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        LogosText {
                            objectName: "manifestoBody"
                            width: textScroll.availableWidth
                            wrapMode: Text.WordWrap
                            textFormat: Text.PlainText
                            text: root.manifestoText
                            lineHeight: 1.35
                        }
                    }
                }

                LogosText {
                    objectName: "sourceCredit"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.palette.textTertiary
                    font.pixelSize: Theme.typography.secondaryText
                    text: "Text by Jarrad Hope. Source: logos.co/manifesto. This app is an unofficial hobby project. It is not a Logos product and is not endorsed by Logos or by the author."
                }

                LogosFrame {
                    Layout.fillWidth: true
                    backgroundColor: Theme.palette.surfaceRaised
                    borderColor: Theme.palette.borderSecondary
                    radius: Theme.spacing.radiusLarge
                    padding: Theme.spacing.large
                    contentItem: ColumnLayout {
                        spacing: Theme.spacing.small

                        LogosText {
                            text: "Sign it"
                            font.pixelSize: Theme.typography.subtitleText
                            font.weight: Theme.typography.weightBold
                        }
                        LogosText {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.palette.textSecondary
                            font.pixelSize: Theme.typography.secondaryText
                            text: "You sign this one line. The sha256 value identifies the exact text above."
                        }
                        LogosText {
                            objectName: "statementText"
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                            text: root.statement
                            font.family: Theme.typography.mono
                            font.pixelSize: Theme.typography.secondaryText
                        }
                        LogosText {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.palette.textTertiary
                            font.pixelSize: Theme.typography.secondaryText
                            visible: root.chosen !== null
                            text: root.chosen === null ? "" : root.ownChosen
                                  ? "The signature is made on this computer with a key this app created for you. The key is used for nothing else, so a receipt points to no other activity of yours. It has no password and is kept in one file: " + root.appKeyFile
                                  : "This is an account from the Signer. The Signer app will show the same line and ask you to approve it. Anyone who sees your receipt can link this account to the manifesto."
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing.medium
                            LogosText {
                                visible: root.accounts.length > 1
                                text: "Sign with"
                                color: Theme.palette.textSecondary
                                font.pixelSize: Theme.typography.secondaryText
                            }
                            LogosComboBox {
                                id: accountBox
                                objectName: "accountBox"
                                Layout.preferredWidth: 340
                                // With only the app's own key there is nothing to choose.
                                visible: root.accounts.length > 1
                                enabled: !root.busy
                                model: root.accounts.map(root.accountLabel)
                                onActivated: function (i) {
                                    if (i >= 0 && i < root.accounts.length) root.chosenAddress = root.accounts[i].address
                                }
                                onModelChanged: {
                                    var at = -1
                                    for (var i = 0; i < root.accounts.length; i++)
                                        if (root.accounts[i].address === root.chosenAddress) at = i
                                    currentIndex = at >= 0 ? at : (root.accounts.length > 0 ? 0 : -1)
                                }
                            }
                            Item { Layout.fillWidth: true }
                            LogosButton {
                                objectName: "cancelButton"
                                visible: root.busy
                                text: "Cancel"
                                compact: true
                                onClicked: root.backend.cancelSign()
                            }
                            LogosButton {
                                objectName: "openSignerButton"
                                visible: root.signState === "awaiting"
                                text: "Open the Signer"
                                compact: true
                                onClicked: root.openSigner(root.pendingHandle)
                            }
                            LogosButton {
                                objectName: "signButton"
                                // The screen's one main action.
                                variant: LogosButton.Variant.Primary
                                text: root.busy ? "Waiting for the Signer…"
                                      : (root.chosen && root.chosen.signed ? "Sign again" : "Sign the manifesto")
                                enabled: root.ready && !root.busy && root.chosen !== null && root.chosen.available !== false
                                onClicked: root.backend.sign(root.chosen.address)
                            }
                        }

                        LogosNotice {
                            objectName: "offNotice"
                            Layout.fillWidth: true
                            visible: root.signerState === "error"
                            severity: LogosNotice.Error
                            title: "Signing is off"
                            message: root.signerInfo
                        }
                        LogosNotice {
                            objectName: "signNotice"
                            Layout.fillWidth: true
                            visible: root.signState !== "idle" && root.signState !== "requesting"
                            severity: root.signState === "signed" ? LogosNotice.Success
                                      : (root.signState === "error" ? LogosNotice.Error
                                         : (root.signState === "rejected" ? LogosNotice.Warning : LogosNotice.Info))
                            title: root.signState === "signed" ? "You signed the manifesto"
                                   : (root.signState === "awaiting" ? "Approve it in the Signer"
                                      : (root.signState === "signing" ? "Signing…"
                                         : (root.signState === "rejected" ? "Not signed" : "Something went wrong")))
                            message: root.signState === "signed"
                                     ? "The signature was checked against the signing address. Your receipt is under Your signatures."
                                     : (root.signerNote !== "" && root.signState === "awaiting" ? root.signerNote : root.signInfo)
                            actions: [
                                LogosButton {
                                    text: "See the receipt"
                                    compact: true
                                    visible: root.signState === "signed"
                                    onClicked: tabs.currentIndex = 1
                                },
                                LogosButton {
                                    text: "Copy the receipt"
                                    compact: true
                                    visible: root.unsavedReceipt !== ""
                                    onClicked: unsavedCopy.copy()
                                }
                            ]
                        }
                        LogosCopyButton {
                            id: unsavedCopy
                            visible: false
                            value: root.unsavedReceipt
                        }
                    }
                }
            }

            // ==================================================== your signatures
            ScrollView {
                id: mineScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: mineScroll.availableWidth
                    spacing: Theme.spacing.medium

                    LogosText {
                        Layout.fillWidth: true
                        visible: root.receipts.length === 0
                        wrapMode: Text.WordWrap
                        color: Theme.palette.textSecondary
                        text: "Nothing signed on this Basecamp yet."
                    }
                    LogosText {
                        Layout.fillWidth: true
                        visible: root.receipts.length > 0
                        wrapMode: Text.WordWrap
                        color: Theme.palette.textSecondary
                        font.pixelSize: Theme.typography.secondaryText
                        text: "A receipt is the proof. It stays on this computer until you share it. Anyone you give it to can check it under Check a signature, or with any tool that verifies Ethereum personal signatures."
                    }

                    Repeater {
                        model: root.receipts
                        delegate: LogosFrame {
                            id: card
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            backgroundColor: Theme.palette.surfaceRaised
                            borderColor: Theme.palette.borderSecondary
                            radius: Theme.spacing.radiusLarge
                            padding: Theme.spacing.large
                            contentItem: ColumnLayout {
                                spacing: Theme.spacing.small
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacing.medium
                                    LogosBadge {
                                        text: card.modelData.sameText ? "verified" : "verified, earlier text"
                                        color: card.modelData.sameText ? Theme.palette.success : Theme.palette.warning
                                    }
                                    LogosText {
                                        text: "Signed on " + card.modelData.date
                                        font.weight: Theme.typography.weightBold
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                                LogosText {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                    text: card.modelData.address
                                    font.family: Theme.typography.mono
                                    font.pixelSize: Theme.typography.secondaryText
                                }
                                LogosTextArea {
                                    objectName: "receiptArea" + card.index
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 170
                                    readOnly: true
                                    text: card.modelData.json
                                    font.family: Theme.typography.mono
                                    font.pixelSize: Theme.typography.secondaryText
                                    wrapMode: TextEdit.WrapAnywhere
                                }
                                LogosText {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WrapAnywhere
                                    color: Theme.palette.textTertiary
                                    font.pixelSize: Theme.typography.secondaryText
                                    text: "Saved at " + card.modelData.file
                                }
                                RowLayout {
                                    spacing: Theme.spacing.small
                                    LogosButton {
                                        text: "Copy receipt"
                                        compact: true
                                        onClicked: receiptCopy.copy()
                                    }
                                    LogosCopyButton {
                                        id: receiptCopy
                                        visible: false
                                        value: card.modelData.json
                                    }
                                    LogosButton {
                                        text: "Check it"
                                        compact: true
                                        onClicked: {
                                            verifyArea.text = card.modelData.json
                                            root.backend.verifyReceipt(card.modelData.json)
                                            tabs.currentIndex = 2
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                    LogosButton {
                                        text: "Remove from this computer"
                                        compact: true
                                        onClicked: root.backend.removeReceipt(card.modelData.file)
                                    }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            // =================================================== check a signature
            ScrollView {
                id: checkScroll
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: checkScroll.availableWidth
                    spacing: Theme.spacing.medium

                    LogosText {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.palette.textSecondary
                        font.pixelSize: Theme.typography.secondaryText
                        text: "Paste a receipt. The check runs on this computer: it works out which account made the signature and compares the text it names with the manifesto in this app."
                    }
                    LogosTextArea {
                        id: verifyArea
                        objectName: "verifyArea"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 190
                        placeholderText: "{ \"type\": \"logos-manifesto-signature\", … }"
                        font.family: Theme.typography.mono
                        font.pixelSize: Theme.typography.secondaryText
                        wrapMode: TextEdit.WrapAnywhere
                    }
                    RowLayout {
                        spacing: Theme.spacing.small
                        LogosButton {
                            objectName: "verifyButton"
                            text: "Check"
                            enabled: root.ready && verifyArea.text.trim().length > 0
                            onClicked: root.backend.verifyReceipt(verifyArea.text)
                        }
                        LogosButton {
                            objectName: "clearButton"
                            text: "Clear"
                            compact: true
                            onClicked: { verifyArea.text = ""; root.backend.verifyReceipt("") }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    LogosNotice {
                        objectName: "verifyNotice"
                        Layout.fillWidth: true
                        visible: root.verifyResult.checked === true
                        severity: !root.verifyResult.valid ? LogosNotice.Error
                                  : (root.verifyResult.sameText ? LogosNotice.Success : LogosNotice.Warning)
                        title: !root.verifyResult.valid ? "Not a valid signature"
                               : (root.verifyResult.sameText ? "Valid signature of this manifesto"
                                  : "Valid signature, of a different text")
                        message: !root.verifyResult.valid ? (root.verifyResult.error || "")
                                 : (root.verifyResult.sameText
                                    ? "Signed by " + root.verifyResult.address + " on " + root.verifyResult.date + ". The date is the signer's own statement."
                                    : "Signed by " + root.verifyResult.address + " on " + root.verifyResult.date
                                      + ", over a text with sha256 " + root.verifyResult.hash
                                      + ". The manifesto in this app has " + root.manifestoHash + ".")
                    }
                    LogosText {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.palette.textTertiary
                        font.pixelSize: Theme.typography.secondaryText
                        text: "A valid signature shows that whoever holds the key for that address signed that exact line. It does not show who that is, and one person can sign with several keys."
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        LogosText {
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            color: Theme.palette.textTertiary
            font.pixelSize: Theme.typography.secondaryText
            text: "Text sha256 " + root.manifestoHash + "   " + root.buildId
        }
    }
}
