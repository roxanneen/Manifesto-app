#pragma once

#include "rep_manifesto_ui_source.h"
#include "logos_ui_plugin_context.h"
#include "sigcheck.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>

// Manifesto backend. Runs in the module's ui-host process; the QML view in
// Basecamp reaches it over QtRemoteObjects (src/manifesto_ui.rep).
//
// Signing: the backend builds a one-line statement naming the sha256 of the
// bundled manifesto text and today's date. By default it signs that line with
// a key of its own, made on first run and kept in module_data (ensureAppKey),
// so no account is needed. If the Signer lists accounts, the person may pick
// one instead; the line then goes to signer_manager_module as a
// personal-message request and is approved in the Signer app. Either way the
// signature is checked here (it must recover to the chosen address over our
// own statement) before a receipt is saved.
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
    void refreshAccounts() override;
    void sign(QString address) override;
    void cancelSign() override;
    void verifyReceipt(QString receiptJson) override;
    void removeReceipt(QString file) override;

    void onContextReady() override;

    // Pure helpers, also used by the checks at start-up.
    static QString buildStatement(const QString& title, const QString& hashHex, const QString& isoDate);
    // Splits a statement back into its parts. False when it is not ours.
    static bool parseStatement(const QString& statement, QString& title, QString& hashHex, QString& isoDate);
    // The whole check of one receipt object, against `currentHash`.
    static QJsonObject checkReceipt(const QJsonObject& receipt, const QString& currentHash);

private:
    // ---- manifesto ---------------------------------------------------------
    void loadManifesto();
    QString todayStatement() const;

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
    bool m_started = false;     // the account list has been published once
    bool ensureAppKey();        // load module_data/manifesto_ui/signing-key, or make it
    void signWithAppKey();

    // ---- signer manager ----------------------------------------------------
    // One seam for every call: method name, string arguments, JSON reply.
    using Reply = std::function<void(bool ok, const QJsonObject& reply, const QString& error)>;
    void callSigner(const QString& method, const QStringList& args, Reply cb);
    void publishAccounts(const QJsonArray& raw);
    QJsonArray m_rawAccounts;

    // The one request in flight. The receipt authorises collecting the
    // result; it stays in memory only and is never logged or published.
    struct Pending {
        QString handle;
        QString receipt;
        QString address;    // as the signer listed it
        QString statement;
        bool active = false;
        bool polling = false;   // an approval_status call is in flight
        bool fetching = false;  // fetch_result is in flight
        int misses = 0;         // consecutive status calls that got no answer
    };
    Pending m_pending;
    // Bumped whenever a request starts or ends. Callbacks capture the value
    // they were issued under and do nothing once it has moved on.
    quint64 m_generation = 0;
    QTimer m_pollTimer;
    void pollPending();
    void collectResult();
    void finishPending(const QString& state, const QString& info);
    // finishPending, after withdrawing the request in the Signer.
    void abandonPending(const QString& state, const QString& info);

    void setSign(const QString& state, const QString& info);
};
