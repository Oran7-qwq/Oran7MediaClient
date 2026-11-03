#pragma once
#include <QDebug>
#include <QString>
inline QString fixtureRoot;
class Oran7GlobalHelper { public: static QString getConfigDir() { return fixtureRoot; } };
#define WARNING_LOG qWarning()
#define CONFIG_LOG qDebug()
#define CLIENT_LOG qDebug()
#define NETWORK_LOG qDebug()
