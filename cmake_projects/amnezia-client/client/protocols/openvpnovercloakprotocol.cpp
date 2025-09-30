#include "openvpnovercloakprotocol.h"

#include "utilities.h"
#include "containers/containers_defs.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

OpenVpnOverCloakProtocol::OpenVpnOverCloakProtocol(const QJsonObject &configuration, QObject *parent):
    OpenVpnProtocol(configuration, parent)
{
    readCloakConfiguration(configuration);
}

OpenVpnOverCloakProtocol::~OpenVpnOverCloakProtocol()
{
    OpenVpnOverCloakProtocol::stop();
    m_ckProcess.close();
}

ErrorCode OpenVpnOverCloakProtocol::start()
{
    if (!QFileInfo::exists(cloakExecPath())) {
        setLastError(ErrorCode::CloakExecutableMissing);
        return lastError();
    }

    if (Utils::processIsRunning(Utils::executable("ck-client", false))) {
        Utils::killProcessByName(Utils::executable("ck-client", false));
    }

    // workaround for desktop releases >= 3.0.7
    if (!m_cloakConfig.contains("RemoteHost") && m_cloakConfig.contains(config_key::remote)) {
        m_cloakConfig["RemoteHost"] = m_cloakConfig.value(config_key::remote);
        m_cloakConfig["RemotePort"] = m_cloakConfig.value(config_key::port);
    }

#ifdef QT_DEBUG
    m_cloakCfgFile.setAutoRemove(false);
#endif
    m_cloakCfgFile.open();
    m_cloakCfgFile.write(QJsonDocument(m_cloakConfig).toJson());
    m_cloakCfgFile.close();

    QStringList args = QStringList() << "-c" << m_cloakCfgFile.fileName()
                                     << "-l" << amnezia::protocols::openvpn::defaultPort;

    qDebug().noquote() << "OpenVpnOverCloakProtocol::start()"
                       << cloakExecPath() << args.join(" ");

    m_ckProcess.setProcessChannelMode(QProcess::MergedChannels);

    m_ckProcess.setProgram(cloakExecPath());
    m_ckProcess.setArguments(args);

    connect(&m_ckProcess, &QProcess::readyReadStandardOutput, this, [this](){
        qDebug().noquote() << "ck-client:" << m_ckProcess.readAllStandardOutput();
    });

    m_errorHandlerConnection = connect(&m_ckProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus){
        qDebug().noquote() << "OpenVpnOverCloakProtocol finished, exitCode, exiStatus" << exitCode << exitStatus;
        setConnectionState(Vpn::ConnectionState::Disconnected);
        if (exitStatus != QProcess::NormalExit){
            emit protocolError(amnezia::ErrorCode::CloakExecutableCrashed);
            stop();
        }
        if (exitCode !=0 ) {
            emit protocolError(amnezia::ErrorCode::InternalError);
            stop();
        }
    });

    m_ckProcess.start();
    m_ckProcess.waitForStarted();

    if (m_ckProcess.state() == QProcess::ProcessState::Running) {
        setConnectionState(Vpn::ConnectionState::Connecting);

        return OpenVpnProtocol::start();
    }
    else return ErrorCode::CloakExecutableMissing;
}

void OpenVpnOverCloakProtocol::stop()
{
    disconnect(m_errorHandlerConnection);
    OpenVpnProtocol::stop();

    qDebug() << "OpenVpnOverCloakProtocol::stop()";

#ifdef Q_OS_WIN
    Utils::signalCtrl(m_ckProcess.processId(), CTRL_C_EVENT);
#endif

    m_ckProcess.terminate();

    if (Utils::processIsRunning(Utils::executable("ck-client", false))) {
        QThread::msleep(1000);
        Utils::killProcessByName(Utils::executable("ck-client", false));
    }
}

QString OpenVpnOverCloakProtocol::cloakExecPath()
{
#ifdef Q_OS_WIN
    return Utils::executable(QString("cloak/ck-client"), true);
#else
    return Utils::executable(QString("/ck-client"), true);
#endif
}

void OpenVpnOverCloakProtocol::readCloakConfiguration(const QJsonObject &configuration)
{
    m_cloakConfig = configuration.value(ProtocolProps::key_proto_config_data(Proto::Cloak)).toObject();
}
