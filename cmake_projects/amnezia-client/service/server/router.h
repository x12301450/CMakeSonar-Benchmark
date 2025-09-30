#ifndef ROUTER_H
#define ROUTER_H

#include <QTimer>
#include <QString>
#include <QSettings>
#include <QHash>
#include <QDebug>
#include <QObject>
#include <QHostAddress>

/**
 * @brief The Router class - General class for handling ip routing
 */
class Router : public QObject
{
    Q_OBJECT
public:
    static int routeAddList(const QString &gw, const QStringList &ips);
    static bool clearSavedRoutes();
    static int routeDeleteList(const QString &gw, const QStringList &ips);
    static void flushDns();
    static void resetIpStack();
    static bool createTun(const QString &dev, const QString &subnet);
    static bool deleteTun(const QString &dev);
    static void StartRoutingIpv6();
    static void StopRoutingIpv6();
    static bool updateResolvers(const QString& ifname, const QList<QHostAddress>& resolvers);
};

#endif // ROUTER_H

