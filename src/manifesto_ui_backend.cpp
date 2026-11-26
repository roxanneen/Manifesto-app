#include "manifesto_ui_backend.h"

// Generated umbrella: LogosModules (behind modules()) from metadata.json's
// dependencies, here signer_manager_module.
#include "logos_sdk.h"
#include "sigcheck.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVariant>

#include <algorithm>

#ifndef Q_OS_WIN
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

#ifndef MANIFESTO_BUILD_ID
#define MANIFESTO_BUILD_ID "manifesto_ui/dev"
#endif

// src/manifesto.txt, byte for byte (CMake writes the include).
const unsigned char kManifesto[] = {
#include "manifesto_text.inc"
};

const QString kReceiptType = QStringLiteral("logos-manifesto-signature");
constexpr int kReceiptVersion = 1;
constexpr int kPollMs = 1000;
constexpr int kMaxPollMisses = 5;
constexpr int kMaxReceiptBytes = 16 * 1024;

// Replies arrive as a JSON string, or already decoded, depending on the
// generated wrapper. Either way the caller gets an object.
QJsonObject objectOf(const QVariant& v)
{
    if (v.canConvert<QVariantMap>() && !v.toMap().isEmpty())
        return QJsonObject::fromVariantMap(v.toMap());
    return QJsonDocument::fromJson(v.toString().toUtf8()).object();
}

struct Raw {
    bool ok = false;
    QVariant value;
    QString error;
};
Raw rawOf(const LogosResult& r) { return {r.success, r.value, r.success ? QString() : r.getError()}; }
Raw rawOf(const QString& s) { return {true, QVariant(s), QString()}; }
Raw rawOf(const QVariant& v) { return {true, v, QString()}; }
Raw rawOf(bool b) { return {true, QVariant(b), QString()}; }

QString std2q(const sig::Address& a) { return QString::fromStdString(sig::checksumAddress(a)); }

} // namespace

ManifestoUiBackend::ManifestoUiBackend()
{
    m_pollTimer.setInterval(kPollMs);
    QObject::connect(&m_pollTimer, &QTimer::timeout, [this]() { pollPending(); });
}

ManifestoUiBackend::~ManifestoUiBackend() { sig::wipe(m_appKey); }

void ManifestoUiBackend::setSign(const QString& state, const QString& info)
{
    setSignState(state);
    setSignInfo(info);
}

// ---------------------------------------------------------------------------
// Statement and receipt (pure)

QString ManifestoUiBackend::buildStatement(const QString& title, const QString& hashHex, const QString& isoDate)
{
    // One line of plain printable text: the Signer refuses control characters,
    // and a single line reads the same in every wallet that might show it.
    return QStringLiteral("I sign the Logos manifesto, \"%1\". Text sha256: %2. Signed on %3.")
        .arg(title, hashHex, isoDate);
}

bool ManifestoUiBackend::parseStatement(const QString& statement, QString& title, QString& hashHex, QString& isoDate)
{
    static const QRegularExpression re(QStringLiteral(
        "\\AI sign the Logos manifesto, \"([^\"]{1,200})\"\\. Text sha256: ([0-9a-f]{64})\\. "
        "Signed on (\\d{4}-\\d{2}-\\d{2})\\.\\z"));
    const QRegularExpressionMatch m = re.match(statement);
    if (!m.hasMatch())
        return false;
    title = m.captured(1);
    hashHex = m.captured(2);
    isoDate = m.captured(3);
    return QDate::fromString(isoDate, Qt::ISODate).isValid();
}

QJsonObject ManifestoUiBackend::checkReceipt(const QJsonObject& receipt, const QString& currentHash)
{
    QJsonObject out;
    out.insert(QStringLiteral("checked"), true);
    out.insert(QStringLiteral("valid"), false);
    out.insert(QStringLiteral("sameText"), false);

    auto fail = [&out](const QString& why) {
        out.insert(QStringLiteral("error"), why);
        return out;
    };

    if (receipt.value(QStringLiteral("type")).toString() != kReceiptType)
        return fail(QStringLiteral("This is not a manifesto signature receipt."));
    const QString statement = receipt.value(QStringLiteral("statement")).toString();
    const QString address = receipt.value(QStringLiteral("address")).toString();
    const QString signature = receipt.value(QStringLiteral("signature")).toString();
    if (statement.isEmpty() || address.isEmpty() || signature.isEmpty())
        return fail(QStringLiteral("The receipt is missing its statement, address or signature."));
    out.insert(QStringLiteral("statement"), statement);

    sig::Address claimed;
    if (!sig::parseAddress(address.toStdString(), claimed))
        return fail(QStringLiteral("The address in the receipt is not an Ethereum address."));
    out.insert(QStringLiteral("address"), std2q(claimed));

    QString title, hash, date;
    if (!parseStatement(statement, title, hash, date))
        return fail(QStringLiteral("The signed text is not a manifesto statement, so it says nothing about the manifesto."));
    out.insert(QStringLiteral("title"), title);
    out.insert(QStringLiteral("hash"), hash);
    out.insert(QStringLiteral("date"), date);

    sig::Address signer;
    std::string err;
    if (!sig::recoverSigner(statement.toStdString(), signature.toStdString(), signer, err))
        return fail(QStringLiteral("The signature could not be checked: %1.").arg(QString::fromStdString(err)));
    if (signer != claimed) {
        out.insert(QStringLiteral("recovered"), std2q(signer));
        return fail(QStringLiteral("The signature was not made by the address in the receipt."));
    }

    out.insert(QStringLiteral("valid"), true);
    out.insert(QStringLiteral("sameText"), hash == currentHash);
    return out;
}

// ---------------------------------------------------------------------------
// Lifecycle

void ManifestoUiBackend::onContextReady()
{
    setBuildId(QStringLiteral(MANIFESTO_BUILD_ID));
    loadManifesto();

    const std::string st = sig::selfTest();
    if (!st.empty()) {
        setSignerState(QStringLiteral("error"));
        setSignerInfo(QStringLiteral("The signature check failed its own test (%1). Signing and checking are off.")
                          .arg(QString::fromStdString(st)));
        return;
    }
    if (!initStorage()) {
        setSignerState(QStringLiteral("error"));
        return;
    }
    // A problem with the key file turns off own-key signing only. Receipts
    // already made stay visible, and Signer accounts stay usable.
    ensureAppKey();
    loadReceipts();
    publishAccounts(m_rawAccounts);     // the app's own key is usable at once
    refreshAccounts();                  // Signer accounts join the list if there are any
}

void ManifestoUiBackend::loadManifesto()
{
    const QByteArray bytes(reinterpret_cast<const char*>(kManifesto), static_cast<int>(sizeof(kManifesto)));
    const QString text = QString::fromUtf8(bytes);
    const int nl = text.indexOf(QLatin1Char('\n'));
    setManifestoTitle(nl > 0 ? text.left(nl).trimmed() : QStringLiteral("Logos manifesto"));
    setManifestoText(text);
    setManifestoHash(QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
    setStatement(todayStatement());
}

QString ManifestoUiBackend::todayStatement() const
{
    // MANIFESTO_DATE pins the date for tests.
    QString date = QProcessEnvironment::systemEnvironment().value(QStringLiteral("MANIFESTO_DATE"));
    if (!QDate::fromString(date, Qt::ISODate).isValid())
        date = QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
    return buildStatement(manifestoTitle(), manifestoHash(), date);
}

// ---------------------------------------------------------------------------
// Storage

bool ManifestoUiBackend::initStorage()
{
    const QString env = QProcessEnvironment::systemEnvironment().value(QStringLiteral("LOGOS_USER_DIR"));
    const QString base = env.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Logos/LogosBasecamp")
        : env;
    m_baseDir = base + QStringLiteral("/module_data/manifesto_ui");
    m_receiptsDir = m_baseDir + QStringLiteral("/receipts");
    if (!QDir().mkpath(m_receiptsDir)) {
        setSignerInfo(QStringLiteral("Could not create %1.").arg(m_receiptsDir));
        return false;
    }
#ifndef Q_OS_WIN
    ::chmod(m_baseDir.toLocal8Bit().constData(), 0700);
    ::chmod(m_receiptsDir.toLocal8Bit().constData(), 0700);
#endif
    setDataDir(m_baseDir);
    return true;
}

// The app's own signing key. It is made here on first run, kept in one file
// that only this user can read, and never leaves this process: no slot, PROP
// or log line carries it. It has no password, because a signature that needs
// no account was the point; what it protects is one statement about a public
// text, and the view says so.
bool ManifestoUiBackend::ensureAppKey()
{
    m_keyPath = m_baseDir + QStringLiteral("/signing-key");
    const QByteArray finalPath = m_keyPath.toLocal8Bit();
    sig::SecretKey key;

    // An existing file is the person's identity in every receipt already
    // made. It is never replaced: an unusable one stops own-key signing.
    auto load = [this, &key]() -> bool {
        QFile f(m_keyPath);
        std::vector<uint8_t> raw;
        if (!f.open(QIODevice::ReadOnly) || f.size() > 256
            || !sig::fromHex(QString::fromLatin1(f.readAll()).trimmed().toStdString(), raw) || raw.size() != key.size()) {
            m_keyError = QStringLiteral("The signing key file at %1 cannot be read, so this app's own key is off. The file was left untouched.").arg(m_keyPath);
            return false;
        }
        std::copy(raw.begin(), raw.end(), key.begin());
        std::fill(raw.begin(), raw.end(), 0);
        if (!sig::validKey(key)) {
            sig::wipe(key);
            m_keyError = QStringLiteral("The signing key file at %1 does not hold a usable key, so this app's own key is off. The file was left untouched.").arg(m_keyPath);
            return false;
        }
        return true;
    };

    bool have = false;
    if (QFile::exists(m_keyPath)) {
        have = load();
    } else {
        if (!sig::generateKey(key)) {
            m_keyError = QStringLiteral("This computer gave no randomness for a new key, so this app's own key is off.");
            return false;
        }
        const QByteArray hex = QByteArray::fromStdString(sig::toHex(key.data(), key.size())) + '\n';
        bool published = false, lostRace = false;
#ifndef Q_OS_WIN
        // Written in full under a temporary name (owner-only from its first
        // byte), then given its real name with link(), which fails if the
        // name is taken. So the real name never holds a partial key, and a
        // key another instance made a moment ago is never overwritten.
        const QByteArray tmpPath = finalPath + ".new." + QByteArray::number(static_cast<qlonglong>(::getpid()));
        ::unlink(tmpPath.constData());
        const int fd = ::open(tmpPath.constData(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            const ssize_t n = ::write(fd, hex.constData(), static_cast<size_t>(hex.size()));
            const bool synced = ::fsync(fd) == 0;
            const bool closed = ::close(fd) == 0;
            if (n == hex.size() && synced && closed) {
                if (::link(tmpPath.constData(), finalPath.constData()) == 0)
                    published = true;
                else if (errno == EEXIST)
                    lostRace = true;
            }
            ::unlink(tmpPath.constData());
        }
#else
        QFile nf(m_keyPath);
        if (nf.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            published = nf.write(hex) == hex.size() && nf.flush();
            nf.close();
            if (published)
                nf.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            else
                nf.remove();
        } else if (QFile::exists(m_keyPath)) {
            lostRace = true;
        }
#endif
        if (lostRace) {
            sig::wipe(key);
            have = load();      // use the key that is there, not the one just made
        } else if (!published) {
            sig::wipe(key);
            m_keyError = QStringLiteral("The new signing key could not be saved to %1, so this app's own key is off.").arg(m_keyPath);
            return false;
        } else {
            have = true;
        }
    }
    if (!have)
        return false;
#ifndef Q_OS_WIN
    ::chmod(finalPath.constData(), 0600);
#endif

    sig::Address a;
    if (!sig::addressOfKey(key, a)) {
        sig::wipe(key);
        m_keyError = QStringLiteral("The signing key at %1 could not be used.").arg(m_keyPath);
        return false;
    }
    m_appKey = key;
    m_haveAppKey = true;
    m_keyError.clear();
    sig::wipe(key);
    setAppKeyAddress(std2q(a));
    setAppKeyFile(m_keyPath);
    return true;
}

void ManifestoUiBackend::signWithAppKey()
{
    const QString statement = todayStatement();
    setStatement(statement);
    setUnsavedReceiptJson(QString());

    std::string sigHex, err;
    if (!m_haveAppKey || !sig::signMessage(statement.toStdString(), m_appKey, sigHex, err)) {
        setSign(QStringLiteral("error"), QStringLiteral("Signing failed: %1.").arg(QString::fromStdString(err)));
        return;
    }
    QJsonObject receipt;
    receipt.insert(QStringLiteral("type"), kReceiptType);
    receipt.insert(QStringLiteral("version"), kReceiptVersion);
    receipt.insert(QStringLiteral("scheme"), QStringLiteral("eip191"));
    receipt.insert(QStringLiteral("statement"), statement);
    receipt.insert(QStringLiteral("address"), appKeyAddress());
    receipt.insert(QStringLiteral("signature"), QString::fromStdString(sigHex));

    // The same check a stranger would run, before anything is kept.
    const QJsonObject res = checkReceipt(receipt, manifestoHash());
    if (!res.value(QStringLiteral("valid")).toBool() || !res.value(QStringLiteral("sameText")).toBool()) {
        setSign(QStringLiteral("error"), QStringLiteral("The new signature does not check out, so it was not kept: %1")
                                             .arg(res.value(QStringLiteral("error")).toString()));
        return;
    }
    QString path;
    if (!saveReceipt(receipt, path)) {
        setUnsavedReceiptJson(QString::fromUtf8(QJsonDocument(receipt).toJson(QJsonDocument::Indented)));
        setSign(QStringLiteral("error"),
                QStringLiteral("Signed and checked, but the receipt could not be saved to %1. Copy it now.").arg(path));
        return;
    }
    loadReceipts();
    setSign(QStringLiteral("signed"), QStringLiteral("Signed and checked."));
}

void ManifestoUiBackend::loadReceipts()
{
    m_receipts = QJsonArray();
    m_signedAddresses.clear();
    QDir dir(m_receiptsDir);
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (const QFileInfo& fi : files) {
        QFile f(fi.absoluteFilePath());
        if (fi.size() > kMaxReceiptBytes || !f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject receipt = QJsonDocument::fromJson(f.readAll()).object();
        // A saved receipt is shown only if it still checks out. A file edited
        // by hand, or left by another build, is never presented as a signature.
        const QJsonObject res = checkReceipt(receipt, manifestoHash());
        if (!res.value(QStringLiteral("valid")).toBool())
            continue;
        QJsonObject row;
        row.insert(QStringLiteral("address"), res.value(QStringLiteral("address")));
        row.insert(QStringLiteral("statement"), receipt.value(QStringLiteral("statement")));
        row.insert(QStringLiteral("signature"), receipt.value(QStringLiteral("signature")));
        row.insert(QStringLiteral("date"), res.value(QStringLiteral("date")));
        row.insert(QStringLiteral("sameText"), res.value(QStringLiteral("sameText")));
        row.insert(QStringLiteral("file"), fi.absoluteFilePath());
        row.insert(QStringLiteral("json"), QString::fromUtf8(QJsonDocument(receipt).toJson(QJsonDocument::Indented)));
        m_receipts.append(row);
        if (res.value(QStringLiteral("sameText")).toBool())
            m_signedAddresses.append(res.value(QStringLiteral("address")).toString().toLower());
    }
    setReceiptsJson(QString::fromUtf8(QJsonDocument(m_receipts).toJson(QJsonDocument::Compact)));
    if (m_started)
        publishAccounts(m_rawAccounts);   // refresh each row's "signed" mark
}

bool ManifestoUiBackend::saveReceipt(const QJsonObject& receipt, QString& path)
{
    // One receipt per account and text version: signing again on another day
    // replaces the earlier receipt for the same text.
    const QString name = QStringLiteral("%1-%2.json")
        .arg(receipt.value(QStringLiteral("address")).toString().toLower().mid(2), manifestoHash().left(12));
    path = m_receiptsDir + QLatin1Char('/') + name;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(receipt).toJson(QJsonDocument::Indented));
    return f.commit();
}

void ManifestoUiBackend::removeReceipt(QString file)
{
    for (const QJsonValue& v : m_receipts) {
        const QString known = v.toObject().value(QStringLiteral("file")).toString();
        // Only a receipt this module listed, and only inside its own folder.
        if (known == file && QFileInfo(known).absolutePath() == QFileInfo(m_receiptsDir).absoluteFilePath())
            QFile::remove(known);
    }
    loadReceipts();
    // "You signed" would now point at a receipt that may be gone.
    if (!m_pending.active && signState() == QLatin1String("signed"))
        setSign(QStringLiteral("idle"), QString());
}

// ---------------------------------------------------------------------------
// Signer manager

void ManifestoUiBackend::callSigner(const QString& method, const QStringList& args, Reply cb)
{
    auto done = [cb, method](const Raw& r) {
        if (!r.ok) {
            cb(false, QJsonObject(), r.error.isEmpty() ? QStringLiteral("%1 did not answer").arg(method) : r.error);
            return;
        }
        // ack_result and cancel_approval answer a bare bool.
        if (r.value.typeId() == QMetaType::Bool) {
            QJsonObject o;
            o.insert(QStringLiteral("ok"), r.value.toBool());
            cb(true, o, QString());
            return;
        }
        const QJsonObject o = objectOf(r.value);
        if (o.isEmpty()) {
            cb(false, o, QStringLiteral("%1 gave an unreadable answer").arg(method));
            return;
        }
        if (!o.value(QStringLiteral("ok")).toBool(false)) {
            QString why = o.value(QStringLiteral("error")).toString();
            if (why.isEmpty())
                why = o.value(QStringLiteral("code")).toString();
            cb(false, o, why.isEmpty() ? QStringLiteral("%1 was refused").arg(method) : why);
            return;
        }
        cb(true, o, QString());
    };

    auto& sm = modules().signer_manager_module;
    if (method == QLatin1String("accounts"))
        sm.accountsAsync([done](auto r) { done(rawOf(r)); });
    else if (method == QLatin1String("request_approval"))
        sm.request_approvalAsync(args.value(0), [done](auto r) { done(rawOf(r)); });
    else if (method == QLatin1String("approval_status"))
        sm.approval_statusAsync(args.value(0), args.value(1), [done](auto r) { done(rawOf(r)); });
    else if (method == QLatin1String("fetch_result"))
        sm.fetch_resultAsync(args.value(0), args.value(1), [done](auto r) { done(rawOf(r)); });
    else if (method == QLatin1String("ack_result"))
        sm.ack_resultAsync(args.value(0), args.value(1), [done](auto r) { done(rawOf(r)); });
    else if (method == QLatin1String("cancel_approval"))
        sm.cancel_approvalAsync(args.value(0), args.value(1), [done](auto r) { done(rawOf(r)); });
    else
        cb(false, QJsonObject(), QStringLiteral("unknown call %1").arg(method));
}

void ManifestoUiBackend::refreshAccounts()
{
    if (m_baseDir.isEmpty())
        return;     // storage never came up; signerInfo says why
    setStatement(todayStatement());
    callSigner(QStringLiteral("accounts"), {}, [this](bool ok, const QJsonObject& reply, const QString&) {
        // The Signer is optional: without it the app's own key is the only
        // choice, which is the normal case. A call that merely did not get
        // through (empty reply) keeps the list as it was, so an account the
        // person had chosen does not vanish under them.
        if (ok)
            m_rawAccounts = reply.value(QStringLiteral("accounts")).toArray();
        else if (!reply.isEmpty() || !m_started)
            m_rawAccounts = QJsonArray();
        m_started = true;
        publishAccounts(m_rawAccounts);
    });
}

void ManifestoUiBackend::publishAccounts(const QJsonArray& raw)
{
    QJsonArray out;
    QStringList seen;
    if (m_haveAppKey) {
        QJsonObject own;
        own.insert(QStringLiteral("address"), appKeyAddress());
        own.insert(QStringLiteral("label"), QStringLiteral("This app's key"));
        own.insert(QStringLiteral("signer"), QStringLiteral("app"));
        own.insert(QStringLiteral("own"), true);
        own.insert(QStringLiteral("available"), true);
        own.insert(QStringLiteral("signed"), m_signedAddresses.contains(appKeyAddress().toLower()));
        out.append(own);
        seen.append(appKeyAddress());
    }
    for (const QJsonValue& v : raw) {
        const QJsonObject a = v.toObject();
        sig::Address parsed;
        const QString address = a.value(QStringLiteral("address")).toString();
        // Only accounts that can make this kind of signature.
        if (!sig::parseAddress(address.toStdString(), parsed))
            continue;
        const QString shown = std2q(parsed);
        if (seen.contains(shown))
            continue;
        seen.append(shown);
        QJsonObject row;
        row.insert(QStringLiteral("address"), shown);
        row.insert(QStringLiteral("label"), a.value(QStringLiteral("label")).toString());
        row.insert(QStringLiteral("signer"), a.value(QStringLiteral("signer")).toString());
        row.insert(QStringLiteral("own"), false);
        row.insert(QStringLiteral("available"), a.value(QStringLiteral("available")).toBool(true));
        row.insert(QStringLiteral("signed"), m_signedAddresses.contains(shown.toLower()));
        out.append(row);
    }
    setAccountsJson(QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact)));
    m_started = true;
    if (out.isEmpty()) {
        // No own key and nothing from the Signer: nothing can sign.
        setSignerState(QStringLiteral("error"));
        setSignerInfo(m_keyError);
        return;
    }
    setSignerState(QStringLiteral("ready"));
    if (!m_haveAppKey)
        setSignerInfo(QStringLiteral("%1 Signer accounts can still sign.").arg(m_keyError));
    else
        setSignerInfo(out.size() == 1 ? QStringLiteral("Signing with this app's own key")
                                      : QStringLiteral("This app's key, or %1 from the Signer").arg(out.size() - 1));
}

void ManifestoUiBackend::sign(QString address)
{
    if (m_pending.active) {
        setSign(signState(), QStringLiteral("A request is already waiting in the Signer."));
        return;
    }
    sig::Address parsed;
    if (!sig::parseAddress(address.toStdString(), parsed)) {
        setSign(QStringLiteral("error"), QStringLiteral("Choose a key first."));
        return;
    }
    if (m_haveAppKey && std2q(parsed) == appKeyAddress()) {
        signWithAppKey();   // on this computer, at once, no Signer involved
        return;
    }
    // Any other address is an account the Signer listed, addressed by the
    // spelling it was listed under.
    QString raw = address;
    for (const QJsonValue& v : m_rawAccounts) {
        const QString a = v.toObject().value(QStringLiteral("address")).toString();
        if (a.compare(address, Qt::CaseInsensitive) == 0)
            raw = a;
    }

    const QString statement = todayStatement();
    setStatement(statement);
    setUnsavedReceiptJson(QString());

    QJsonObject leg;
    leg.insert(QStringLiteral("kind"), QStringLiteral("message"));
    leg.insert(QStringLiteral("text"), statement);
    QJsonObject intent;
    intent.insert(QStringLiteral("address"), raw);
    intent.insert(QStringLiteral("purpose"), QStringLiteral("Sign the Logos manifesto"));
    intent.insert(QStringLiteral("legs"), QJsonArray{leg});

    m_pending = Pending();
    m_pending.active = true;
    m_pending.address = std2q(parsed);
    m_pending.statement = statement;
    // Every callback below belongs to this one request. A reply that arrives
    // after the request was cancelled, finished or replaced must not act on
    // whatever request is current by then.
    const quint64 gen = ++m_generation;
    setPendingHandle(QString());
    setSign(QStringLiteral("requesting"), QStringLiteral("Asking the Signer…"));

    callSigner(QStringLiteral("request_approval"),
               {QString::fromUtf8(QJsonDocument(intent).toJson(QJsonDocument::Compact))},
               [this, gen](bool ok, const QJsonObject& reply, const QString& error) {
        const QString handle = reply.value(QStringLiteral("handle")).toString();
        const QString token = reply.value(QStringLiteral("receipt")).toString();
        if (gen != m_generation || !m_pending.active) {
            // Given up on before the Signer answered: withdraw it there too,
            // so nobody is asked to approve a request nothing will collect.
            if (ok && !handle.isEmpty() && !token.isEmpty())
                callSigner(QStringLiteral("cancel_approval"), {handle, token},
                           [](bool, const QJsonObject&, const QString&) {});
            return;
        }
        if (!ok) {
            finishPending(QStringLiteral("error"), QStringLiteral("The Signer refused the request: %1.").arg(error));
            return;
        }
        if (handle.isEmpty() || token.isEmpty()) {
            finishPending(QStringLiteral("error"), QStringLiteral("The Signer answered without a request handle."));
            return;
        }
        m_pending.handle = handle;
        m_pending.receipt = token;
        setSign(QStringLiteral("awaiting"), QStringLiteral("Waiting for you to approve it in the Signer."));
        setPendingHandle(handle);   // the view brings the Signer forward
        m_pollTimer.start();
    });
}

void ManifestoUiBackend::pollPending()
{
    if (!m_pending.active || m_pending.handle.isEmpty() || m_pending.fetching || m_pending.polling)
        return;
    m_pending.polling = true;
    const quint64 gen = m_generation;
    callSigner(QStringLiteral("approval_status"), {m_pending.handle, m_pending.receipt},
               [this, gen](bool ok, const QJsonObject& reply, const QString& error) {
        if (gen != m_generation || !m_pending.active)
            return;
        m_pending.polling = false;
        if (m_pending.fetching)
            return;
        if (!ok) {
            // An empty reply is a call that did not get through (the module
            // was busy or reloading), not news about the request: keep asking.
            // A refusal the Signer actually sent means the record is gone.
            if (reply.isEmpty() && ++m_pending.misses < kMaxPollMisses)
                return;
            abandonPending(QStringLiteral("rejected"),
                           reply.isEmpty()
                               ? QStringLiteral("The Signer stopped answering, so the request was withdrawn. Try again.")
                               : QStringLiteral("The request ended before it was approved (%1). Open the Signer and try again.").arg(error));
            return;
        }
        m_pending.misses = 0;
        const QString state = reply.value(QStringLiteral("state")).toString();
        if (state == QLatin1String("signing")) {
            setSign(QStringLiteral("signing"), QStringLiteral("Approved. The signer is working."));
            return;
        }
        if (state != QLatin1String("settled"))
            return;     // offered or rendered: still the person's turn
        const QString reason = reply.value(QStringLiteral("reason")).toString();
        if (reason == QLatin1String("rejected")) {
            finishPending(QStringLiteral("rejected"), QStringLiteral("You declined it in the Signer. Nothing was signed."));
            return;
        }
        if (reason == QLatin1String("expired_no_ack")) {
            finishPending(QStringLiteral("rejected"),
                          QStringLiteral("The Signer did not open in time. Open the Signer app, then try again."));
            return;
        }
        if (reason == QLatin1String("cancelled")) {
            finishPending(QStringLiteral("idle"), QString());
            return;
        }
        collectResult();
    });
}

void ManifestoUiBackend::collectResult()
{
    m_pending.fetching = true;
    const quint64 gen = m_generation;
    // The request's own handle and token, kept by value: the acknowledgement
    // below must name this request whatever m_pending holds by then.
    const QString handle = m_pending.handle, token = m_pending.receipt;
    const QString address = m_pending.address, statement = m_pending.statement;
    callSigner(QStringLiteral("fetch_result"), {handle, token},
               [this, gen, handle, token, address, statement](bool ok, const QJsonObject& reply, const QString& error) {
        if (gen != m_generation || !m_pending.active)
            return;
        if (!ok) {
            finishPending(QStringLiteral("rejected"),
                          QStringLiteral("Nothing was signed (%1).").arg(error));
            return;
        }
        auto ack = [this, handle, token]() {
            callSigner(QStringLiteral("ack_result"), {handle, token}, [](bool, const QJsonObject&, const QString&) {});
        };
        const QJsonArray signedLegs = reply.value(QStringLiteral("signed")).toArray();
        if (signedLegs.size() != 1) {
            ack();
            finishPending(QStringLiteral("error"),
                          QStringLiteral("The Signer returned %1 signatures for one request.").arg(signedLegs.size()));
            return;
        }
        QString signature = signedLegs.at(0).toString();
        if (signature.isEmpty())
            signature = signedLegs.at(0).toObject().value(QStringLiteral("signature")).toString();
        if (!signature.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            signature.prepend(QLatin1String("0x"));

        QJsonObject receipt;
        receipt.insert(QStringLiteral("type"), kReceiptType);
        receipt.insert(QStringLiteral("version"), kReceiptVersion);
        receipt.insert(QStringLiteral("scheme"), QStringLiteral("eip191"));
        receipt.insert(QStringLiteral("statement"), statement);
        receipt.insert(QStringLiteral("address"), address);
        receipt.insert(QStringLiteral("signature"), signature.toLower());

        // Nothing is taken on the Signer's word: the signature must recover to
        // the chosen account over the statement this module built.
        const QJsonObject res = checkReceipt(receipt, manifestoHash());
        ack();
        if (!res.value(QStringLiteral("valid")).toBool() || !res.value(QStringLiteral("sameText")).toBool()) {
            finishPending(QStringLiteral("error"),
                          QStringLiteral("The signature that came back does not check out, so it was not kept: %1")
                              .arg(res.value(QStringLiteral("error")).toString()));
            return;
        }
        QString path;
        if (!saveReceipt(receipt, path)) {
            // The signature is good and cannot be fetched twice: hand it to
            // the view so the person can copy it instead of signing again.
            setUnsavedReceiptJson(QString::fromUtf8(QJsonDocument(receipt).toJson(QJsonDocument::Indented)));
            finishPending(QStringLiteral("error"),
                          QStringLiteral("Signed and checked, but the receipt could not be saved to %1. Copy it now.").arg(path));
            return;
        }
        loadReceipts();
        finishPending(QStringLiteral("signed"), QStringLiteral("Signed and checked."));
    });
}

void ManifestoUiBackend::finishPending(const QString& state, const QString& info)
{
    m_pollTimer.stop();
    m_pending = Pending();
    ++m_generation;     // replies still in flight belong to nobody now
    setPendingHandle(QString());
    setSign(state, info);
}

void ManifestoUiBackend::abandonPending(const QString& state, const QString& info)
{
    // Giving up while the request may still be on screen in the Signer:
    // withdraw it there first.
    if (!m_pending.handle.isEmpty())
        callSigner(QStringLiteral("cancel_approval"), {m_pending.handle, m_pending.receipt},
                   [](bool, const QJsonObject&, const QString&) {});
    finishPending(state, info);
}

void ManifestoUiBackend::cancelSign()
{
    if (!m_pending.active)
        return;
    // Before the Signer has answered there is no handle yet; the late reply
    // is withdrawn in sign()'s callback.
    abandonPending(QStringLiteral("idle"), QString());
}

// ---------------------------------------------------------------------------
// Verify

// A receipt that travelled through a chat app or an email often arrives
// damaged in ways that have nothing to do with the signature: the backslashes
// before the quotes inside the statement are dropped (so it is no longer JSON),
// straight quotes become curly ones, long lines get wrapped or split, invisible
// characters are inserted. The three things that matter survive all of that:
// the statement line, the address and the signature. This finds them in any
// text.
//
// It cannot make a bad signature pass. It only ever returns an address that
// is written in the text, never one worked out from the signature (every
// signature "recovers" to some address, so that would prove nothing), and
// whatever it returns still goes through checkReceipt, where the signature
// must recover to that address over that exact statement.
//
// On failure the result has only "missing": statement | signature | both | address.
QJsonObject ManifestoUiBackend::receiptFromLooseText(const QString& textIn)
{
    QString t = textIn;
    // Invisible characters chat apps insert: soft hyphen, zero-width and
    // direction marks, word joiner, byte-order mark.
    static const QRegularExpression invisible(QStringLiteral(
        "[\\x{00AD}\\x{200B}-\\x{200F}\\x{202A}-\\x{202E}\\x{2060}\\x{FEFF}]"));
    t.remove(invisible);
    // Email quoting at the start of lines.
    static const QRegularExpression quoteMarks(QStringLiteral("(^|\\n)[ \\t]*(?:>[ \\t]*)+"));
    t.replace(quoteMarks, QStringLiteral("\\1"));
    // Undo what escaping and "smart" typography do to the quotes in the statement.
    static const QRegularExpression escapedQuote(QStringLiteral("\\\\+\""));
    t.replace(escapedQuote, QStringLiteral("\""));
    t.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    t.replace(QStringLiteral("&#34;"), QStringLiteral("\""));
    for (const QChar c : {QChar(0x201C), QChar(0x201D), QChar(0x201E), QChar(0x201F), QChar(0x00AB), QChar(0x00BB)})
        t.replace(c, QLatin1Char('"'));
    t.replace(QChar(0x00A0), QLatin1Char(' '));
    // Wrapped lines: the statement has single spaces only.
    t = t.simplified();

    static const QRegularExpression stRe(QStringLiteral(
        "I sign the Logos manifesto, \"[^\"]{1,200}\"\\. Text sha256: [0-9a-f]{64}\\. Signed on \\d{4}-\\d{2}-\\d{2}\\."));
    // A signature may have been split by a space or a line break.
    static const QRegularExpression sigRe(QStringLiteral(
        "(?<![0-9a-fA-F])0[xX](?:[0-9a-fA-F] ?){130}(?![0-9a-fA-F])"));
    static const QRegularExpression addrRe(QStringLiteral(
        "(?<![0-9a-fA-F])0[xX][0-9a-fA-F]{40}(?![0-9a-fA-F])"));

    const QRegularExpressionMatch st = stRe.match(t);
    const QRegularExpressionMatch sg = sigRe.match(t);
    QJsonObject out;
    if (!st.hasMatch() || !sg.hasMatch()) {
        out.insert(QStringLiteral("missing"),
                   !st.hasMatch() && !sg.hasMatch() ? QStringLiteral("both")
                       : (!st.hasMatch() ? QStringLiteral("statement") : QStringLiteral("signature")));
        return out;
    }
    const QString statement = st.captured(0);
    QString signature = sg.captured(0);
    signature.remove(QLatin1Char(' '));
    signature = QStringLiteral("0x") + signature.mid(2).toLower();

    // Addresses written in the text, outside the signature itself. If one of
    // them is the signer's, that is the receipt's address; otherwise the first
    // one is taken, and the check will say the signature is not from it.
    QString rest = t;
    rest.remove(sg.capturedStart(0), sg.capturedLength(0));
    sig::Address signer;
    std::string err;
    const bool recovered = sig::recoverSigner(statement.toStdString(), signature.toStdString(), signer, err);
    QString address;
    QRegularExpressionMatchIterator it = addrRe.globalMatch(rest);
    while (it.hasNext()) {
        const QString candidate = it.next().captured(0);
        sig::Address parsed;
        if (!sig::parseAddress(candidate.toStdString(), parsed))
            continue;
        if (address.isEmpty())
            address = candidate;
        if (recovered && parsed == signer) {
            address = candidate;
            break;
        }
    }
    if (address.isEmpty()) {
        out.insert(QStringLiteral("missing"), QStringLiteral("address"));
        return out;
    }

    out.insert(QStringLiteral("type"), kReceiptType);
    out.insert(QStringLiteral("version"), kReceiptVersion);
    out.insert(QStringLiteral("scheme"), QStringLiteral("eip191"));
    out.insert(QStringLiteral("statement"), statement);
    out.insert(QStringLiteral("address"), address);
    out.insert(QStringLiteral("signature"), signature);
    return out;
}

void ManifestoUiBackend::verifyReceipt(QString receiptJson)
{
    QJsonObject out;
    const QByteArray bytes = receiptJson.trimmed().toUtf8();
    if (bytes.isEmpty()) {
        setVerifyResultJson(QStringLiteral("{}"));
        return;
    }
    auto refuse = [&out](const QString& why) {
        out = QJsonObject();
        out.insert(QStringLiteral("checked"), true);
        out.insert(QStringLiteral("valid"), false);
        out.insert(QStringLiteral("sameText"), false);
        out.insert(QStringLiteral("error"), why);
    };
    if (bytes.size() > kMaxReceiptBytes) {
        refuse(QStringLiteral("That is too long to be a receipt."));
    } else {
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
        const bool strict = pe.error == QJsonParseError::NoError && doc.isObject();
        if (strict)
            out = checkReceipt(doc.object(), manifestoHash());

        // Not clean JSON, or clean JSON whose text was altered on the way
        // (curly quotes inside the statement, say): look for the statement,
        // address and signature in the text itself.
        if (!strict || !out.value(QStringLiteral("valid")).toBool()) {
            const QJsonObject loose = receiptFromLooseText(receiptJson);
            const QString missing = loose.value(QStringLiteral("missing")).toString();
            if (missing.isEmpty()) {
                QJsonObject res = checkReceipt(loose, manifestoHash());
                res.insert(QStringLiteral("repaired"), true);
                if (!strict) {
                    out = res;
                } else if (res.value(QStringLiteral("valid")).toBool()) {
                    // A well-formed receipt names its address in a field of
                    // its own. The repaired reading may replace its verdict
                    // only when it is about that same address.
                    sig::Address named, found;
                    if (sig::parseAddress(doc.object().value(QStringLiteral("address")).toString().toStdString(), named)
                        && sig::parseAddress(loose.value(QStringLiteral("address")).toString().toStdString(), found)
                        && named == found)
                        out = res;
                }
            } else if (!strict) {
                if (missing == QLatin1String("address"))
                    refuse(QStringLiteral("The signed line and a signature were found, but no address. A signature can only be checked against the address it claims to come from. Paste the whole receipt, including the address."));
                else if (missing == QLatin1String("signature"))
                    refuse(QStringLiteral("The signed line was found, but the signature is missing or cut short. It should be 0x followed by 130 letters and digits. Ask the sender to send the receipt again."));
                else if (missing == QLatin1String("statement"))
                    refuse(QStringLiteral("A signature was found, but the signed line is missing or was changed on the way. Ask the sender to send the receipt again."));
                else
                    refuse(QStringLiteral("No signed line and no signature were found in that text. Paste the whole receipt."));
            }
        }
    }
    setVerifyResultJson(QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact)));
}
