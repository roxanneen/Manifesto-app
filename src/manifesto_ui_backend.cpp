#include "manifesto_ui_backend.h"

// Generated umbrella. This app depends on no other module, so LogosModules is
// empty; the include completes the type LogosUiPluginContext refers to.
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

// The manifesto as inscribed on Bitcoin (inscription 70,614,708, block
// 841,886, 3 May 2024): a JPEG of the typeset text. Every signature names
// this ID and the sha256 of that file. Anyone can fetch the file from an
// ordinals explorer, e.g. https://ordinals.com/content/<ID>, and hash it.
const char kInscriptionId[] = "1c3990078b6a4e68d2e5c87c88cff9782f261cb22b5375bfc1cdbe711cbf4686i0";
// The title every current statement names (the first line of the text).
const char kManifestoTitle[] = "Logos: A Declaration of Independence in Cyberspace";
const char kInscriptionSha256[] = "441e1c2ae342848fb6298e62cc427d22c2a954b73b6e1f2899da25072757aead";

const QString kReceiptType = QStringLiteral("logos-manifesto-signature");
constexpr int kReceiptVersion = 1;
constexpr int kMaxReceiptBytes = 16 * 1024;

QString std2q(const sig::Address& a) { return QString::fromStdString(sig::checksumAddress(a)); }

} // namespace

ManifestoUiBackend::ManifestoUiBackend() = default;

ManifestoUiBackend::~ManifestoUiBackend() { sig::wipe(m_appKey); }

void ManifestoUiBackend::setSign(const QString& state, const QString& info)
{
    setSignState(state);
    setSignInfo(info);
}

// ---------------------------------------------------------------------------
// Statement and receipt (pure)

QString ManifestoUiBackend::buildStatement(const QString& title, const QString& inscriptionId,
                                           const QString& hashHex, const QString& isoDate)
{
    // One line of plain printable text: it reads the same wherever it is
    // shown, and it survives being pasted into a chat better than several.
    // It names the copy of the manifesto inscribed on Bitcoin, by its
    // inscription ID and the sha256 of that file, so every signature points
    // at a version nobody can change.
    return QStringLiteral("I sign the Logos manifesto, \"%1\", inscribed on Bitcoin as %2. Image sha256: %3. Signed on %4.")
        .arg(title, inscriptionId, hashHex, isoDate);
}

bool ManifestoUiBackend::parseStatement(const QString& statement, QString& title, QString& inscriptionId,
                                        QString& hashHex, QString& isoDate)
{
    // The current form, naming the Bitcoin inscription.
    static const QRegularExpression current(QStringLiteral(
        "\\AI sign the Logos manifesto, \"([^\"]{1,200})\", inscribed on Bitcoin as ([0-9a-f]{64}i[0-9]{1,6})\\. "
        "Image sha256: ([0-9a-f]{64})\\. Signed on (\\d{4}-\\d{2}-\\d{2})\\.\\z"));
    // The form versions up to 0.1.5 signed: the sha256 of the app's text.
    // Still read, so those receipts check out, as a different version.
    static const QRegularExpression earlier(QStringLiteral(
        "\\AI sign the Logos manifesto, \"([^\"]{1,200})\"\\. Text sha256: ([0-9a-f]{64})\\. "
        "Signed on (\\d{4}-\\d{2}-\\d{2})\\.\\z"));
    QRegularExpressionMatch m = current.match(statement);
    if (m.hasMatch()) {
        title = m.captured(1);
        inscriptionId = m.captured(2);
        hashHex = m.captured(3);
        isoDate = m.captured(4);
    } else {
        m = earlier.match(statement);
        if (!m.hasMatch())
            return false;
        title = m.captured(1);
        inscriptionId.clear();
        hashHex = m.captured(2);
        isoDate = m.captured(3);
    }
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

    QString title, inscription, hash, date;
    if (!parseStatement(statement, title, inscription, hash, date))
        return fail(QStringLiteral("The signed text is not a manifesto statement, so it says nothing about the manifesto."));
    out.insert(QStringLiteral("title"), title);
    out.insert(QStringLiteral("hash"), hash);
    out.insert(QStringLiteral("inscription"), inscription);
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
    // The version this app signs: the Bitcoin inscription, by its file's
    // hash and its ID. A receipt in the earlier form names the app's text
    // instead and is valid, but for a different version.
    out.insert(QStringLiteral("sameText"), hash == currentHash && inscription == QLatin1String(kInscriptionId)
                                           && title == QLatin1String(kManifestoTitle));
    return out;
}

// ---------------------------------------------------------------------------
// Lifecycle

void ManifestoUiBackend::onContextReady()
{
    setBuildId(QStringLiteral(MANIFESTO_BUILD_ID));
    loadManifesto();
    // The date in the statement is today's (UTC). Keep the shown line in step
    // with what Sign would sign when the app stays open past midnight.
    m_dateTimer.setInterval(60 * 1000);
    QObject::connect(&m_dateTimer, &QTimer::timeout, [this]() {
        const QString now = todayStatement();
        if (now != statement())
            setStatement(now);
    });
    m_dateTimer.start();

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
    // A problem with the key file turns off signing only. Receipts already
    // made stay visible, and checking receipts still works.
    ensureAppKey();
    m_started = true;
    loadReceipts();
}

void ManifestoUiBackend::loadManifesto()
{
    const QByteArray bytes(reinterpret_cast<const char*>(kManifesto), static_cast<int>(sizeof(kManifesto)));
    const QString text = QString::fromUtf8(bytes);
    const int nl = text.indexOf(QLatin1Char('\n'));
    setManifestoTitle(nl > 0 ? text.left(nl).trimmed() : QStringLiteral("Logos manifesto"));
    setManifestoText(text);
    setTextSha256(QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
    // What every signature names: the manifesto as inscribed on Bitcoin.
    setInscriptionId(QLatin1String(kInscriptionId));
    setManifestoHash(QLatin1String(kInscriptionSha256));
    setStatement(todayStatement());
}

QString ManifestoUiBackend::todayStatement() const
{
    // MANIFESTO_DATE pins the date for tests.
    QString date = QProcessEnvironment::systemEnvironment().value(QStringLiteral("MANIFESTO_DATE"));
    if (!QDate::fromString(date, Qt::ISODate).isValid())
        date = QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
    return buildStatement(manifestoTitle(), inscriptionId(), manifestoHash(), date);
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
            m_keyError = QStringLiteral("The signing key file at %1 cannot be read, so signing is off. The file was left untouched.").arg(m_keyPath);
            return false;
        }
        std::copy(raw.begin(), raw.end(), key.begin());
        std::fill(raw.begin(), raw.end(), 0);
        if (!sig::validKey(key)) {
            sig::wipe(key);
            m_keyError = QStringLiteral("The signing key file at %1 does not hold a usable key, so signing is off. The file was left untouched.").arg(m_keyPath);
            return false;
        }
        return true;
    };

    bool have = false;
    if (QFile::exists(m_keyPath)) {
        have = load();
    } else {
        if (!sig::generateKey(key)) {
            m_keyError = QStringLiteral("This computer gave no randomness for a new key, so signing is off.");
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
            m_keyError = QStringLiteral("The new signing key could not be saved to %1, so signing is off.").arg(m_keyPath);
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
        publishState();
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
    if (signState() == QLatin1String("signed"))
        setSign(QStringLiteral("idle"), QString());
}

// ---------------------------------------------------------------------------
// State for the view, and the Sign slot

void ManifestoUiBackend::publishState()
{
    if (!m_haveAppKey) {
        setSignerState(QStringLiteral("error"));
        setSignerInfo(m_keyError);
        setOwnSigned(false);
        return;
    }
    setSignerState(QStringLiteral("ready"));
    setSignerInfo(QString());
    setOwnSigned(m_signedAddresses.contains(appKeyAddress().toLower()));
}

void ManifestoUiBackend::sign()
{
    if (!m_haveAppKey) {
        setSign(QStringLiteral("error"), m_keyError);
        return;
    }
    signWithAppKey();
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
        "I sign the Logos manifesto, \"[^\"]{1,200}\"(?:, inscribed on Bitcoin as [0-9a-f]{64}i[0-9]{1,6}\\. Image sha256: |\\. Text sha256: )"
        "[0-9a-f]{64}\\. Signed on \\d{4}-\\d{2}-\\d{2}\\."));
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
