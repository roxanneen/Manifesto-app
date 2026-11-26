#pragma once

#include "rep_manifesto_ui_source.h"
#include "logos_ui_plugin_context.h"
#include "sigcheck.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTimer>


// Manifesto backend. Runs in the module's ui-host process; the QML view in
// Basecamp reaches it over QtRemoteObjects (src/manifesto_ui.rep).
//
// Signing: the backend builds a one-line statement naming the sha256 of the
// bundled manifesto text and today's date, and signs it with a key of its own,
// made on first run and kept in module_data (ensureAppKey). No account and no
// other module is involved. The signature is checked here (it must recover to
// the key's address over our own statement) before a receipt is saved.
//
// Verifying: a receipt is { statement, address, signature }. Checking one
// needs nothing but the receipt and the manifesto text: recover the address
// from the signature and the statement (src/sigcheck.*), and compare the hash
// the statement names with the hash of the text in this build.
class ManifestoUiBackend : public ManifestoUiSimpleSource,
                           public LogosUiPluginContext
{
public:
    ManifestoUiBackend();
    ~ManifestoUiBackend() override;   // wipes the key from memory

    // .rep SLOTs
    void sign() override;
    void verifyReceipt(QString receiptJson) override;
    void removeReceipt(QString file) override;

    void onContextReady() override;

    // Pure helpers, also used by the checks at start-up.
    static QString buildStatement(const QString& title, const QString& inscriptionId,
                                  const QString& hashHex, const QString& isoDate);
    // Splits a statement back into its parts. False when it is not ours.
    // inscriptionId comes back empty for the earlier, text-hash form.
    static bool parseStatement(const QString& statement, QString& title, QString& inscriptionId,
                               QString& hashHex, QString& isoDate);
    // The whole check of one receipt object, against `currentHash`.
    static QJsonObject checkReceipt(const QJsonObject& receipt, const QString& currentHash);
    // Finds the statement, address and signature in text that is no longer
    // clean JSON (see the .cpp). Empty when they are not there.
    static QJsonObject receiptFromLooseText(const QString& text);

private:
    // ---- manifesto ---------------------------------------------------------
    void loadManifesto();
    QString todayStatement() const;
    QTimer m_dateTimer;     // keeps the shown statement on today's date

    // ---- storage -----------------------------------------------------------
    QString m_baseDir;      // <...>/module_data/manifesto_ui
    QString m_receiptsDir;
    bool initStorage();
    void loadReceipts();
    bool saveReceipt(const QJsonObject& receipt, QString& path);
    QJsonArray m_receipts;  // newest first
    QStringList m_signedAddresses;  // lowercase

    // ---- the app's own key -------------------------------------------------
    QString m_keyPath;
    sig::SecretKey m_appKey{};
    bool m_haveAppKey = false;
    QString m_keyError;         // why own-key signing is off, when it is
    bool m_started = false;     // start-up finished; state may be published
    bool ensureAppKey();        // load module_data/manifesto_ui/signing-key, or make it
    void signWithAppKey();

    void publishState();        // signerState, signerInfo, ownSigned for the view

    void setSign(const QString& state, const QString& info);
};
