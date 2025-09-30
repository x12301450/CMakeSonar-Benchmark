#include "shadowsocksvpnprotocol.h"

#include "logger.h"
#include "utilities.h"
#include "containers/containers_defs.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

ShadowSocksVpnProtocol::ShadowSocksVpnProtocol(const QJsonObject &configuration, QObject *parent):
    OpenVpnProtocol(configuration, parent)
{
    readShadowSocksConfiguration(configuration);
}

ShadowSocksVpnProtocol::~ShadowSocksVpnProtocol()
{
    qDebug() << "ShadowSocksVpnProtocol::~ShadowSocksVpnProtocol";
    ShadowSocksVpnProtocol::stop();
    QThread::msleep(200);
#ifndef Q_OS_IOS
    m_ssProcess.close();
#endif
}

ErrorCode ShadowSocksVpnProtocol::start()
{

    if (!QFileInfo::exists(shadowSocksExecPath())) {
        setLastError(ErrorCode::ShadowSocksExecutableMissing);
        return lastError();
    }


#ifndef Q_OS_IOS
    if (Utils::processIsRunning(Utils::executable("ss-local", false))) {
        Utils::killProcessByName(Utils::executable("ss-local", false));
    }

#ifdef QT_DEBUG
    m_shadowSocksCfgFile.setAutoRemove(false);
#endif
    m_shadowSocksCfgFile.open();
    m_shadowSocksCfgFile.write(QJsonDocument(m_shadowSocksConfig).toJson());
    m_shadowSocksCfgFile.close();

#ifdef Q_OS_LINUX
    QStringList args = QStringList() << "-c" << m_shadowSocksCfgFile.fileName();
#else
    QStringList args = QStringList() << "-c" << m_shadowSocksCfgFile.fileName()
                                     << "--no-delay";
#endif

    qDebug().noquote() << "ShadowSocksVpnProtocol::start()"
                       << shadowSocksExecPath() << args.join(" ");

    m_ssProcess.setProcessChannelMode(QProcess::MergedChannels);

    m_ssProcess.setProgram(shadowSocksExecPath());
    m_ssProcess.setArguments(args);

    connect(&m_ssProcess, &QProcess::readyReadStandardOutput, this, [this](){
        qDebug().noquote() << "ss-local:" << m_ssProcess.readAllStandardOutput();
    });

    connect(&m_ssProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        qDebug().noquote() << "ShadowSocksVpnProtocol finished, exitCode, exiStatus" << exitCode << exitStatus;
        setConnectionState(Vpn::ConnectionState::Disconnected);
        if (exitStatus != QProcess::NormalExit){
            emit protocolError(amnezia::ErrorCode::ShadowSocksExecutableCrashed);
            stop();
        }
        if (exitCode !=0 ){
            emit protocolError(amnezia::ErrorCode::InternalError);
            stop();
        }
    });

    m_ssProcess.start();
    m_ssProcess.waitForStarted();

    if (m_ssProcess.state() == QProcess::ProcessState::Running) {
        setConnectionState(Vpn::ConnectionState::Connecting);

        return OpenVpnProtocol::start();
    }
    else return ErrorCode::ShadowSocksExecutableMissing;
#else
    return ErrorCode::NotImplementedError;
#endif
}

void ShadowSocksVpnProtocol::stop()
{
    OpenVpnProtocol::stop();

    qDebug() << "ShadowSocksVpnProtocol::stop()";
#ifndef Q_OS_IOS
    m_ssProcess.terminate();
#endif

#ifdef Q_OS_WIN
    Utils::signalCtrl(m_ssProcess.processId(), CTRL_C_EVENT);
#endif
}

QString ShadowSocksVpnProtocol::shadowSocksExecPath()
{
#ifdef Q_OS_WIN
    return Utils::executable(QString("ss/ss-local"), true);
#else
    return Utils::executable(QString("/ss-local"), true);
#endif
}

void ShadowSocksVpnProtocol::readShadowSocksConfiguration(const QJsonObject &configuration)
{
    QJsonObject shadowSocksConfig = configuration.value(ProtocolProps::key_proto_config_data(Proto::ShadowSocks)).toObject();
    bool isLocalPortConvertOk = false;
    bool isServerPortConvertOk = false;
    int localPort = shadowSocksConfig.value("local_port").toString().toInt(&isLocalPortConvertOk);
    int serverPort = shadowSocksConfig.value("server_port").toString().toInt(&isServerPortConvertOk);
    if (!isLocalPortConvertOk) {
        qDebug() << "Error when converting local_port field in ShadowSocks config";
    } else if (!isServerPortConvertOk) {
        qDebug() << "Error when converting server_port field in ShadowSocks config";
    }
    shadowSocksConfig["local_port"] = localPort;
    shadowSocksConfig["server_port"] = serverPort;
    m_shadowSocksConfig = shadowSocksConfig;
}
