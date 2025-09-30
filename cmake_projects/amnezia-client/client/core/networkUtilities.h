#ifndef NETWORKUTILITIES_H
#define NETWORKUTILITIES_H

#include <QRegularExpression>
#include <QRegExp>
#include <QString>
#include <QHostAddress>
#include <QNetworkReply>


class NetworkUtilities : public QObject
{
    Q_OBJECT
public:
    static QString getIPAddress(const QString &host);
    static QString getStringBetween(const QString &s, const QString &a, const QString &b);
    static bool checkIPv4Format(const QString &ip);
    static bool checkIpSubnetFormat(const QString &ip);
    static bool checkIpv6Enabled();
    static QString getGatewayAndIface();
    // Returns the Interface Index that could Route to dst
    static int AdapterIndexTo(const QHostAddress& dst);

    static QRegularExpression ipAddressRegExp();
    static QRegularExpression ipAddressPortRegExp();
    static QRegExp ipAddressWithSubnetRegExp();
    static QRegExp ipNetwork24RegExp();
    static QRegExp ipPortRegExp();
    static QRegExp domainRegExp();

    static QString netMaskFromIpWithSubnet(const QString ip);
    static QString ipAddressFromIpWithSubnet(const QString ip);
    static QStringList summarizeRoutes(const QStringList &ips, const QString cidr);
};

#endif // NETWORKUTILITIES_H
