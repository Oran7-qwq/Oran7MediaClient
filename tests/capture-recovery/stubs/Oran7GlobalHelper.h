#pragma once
#include <QDebug>
#include <functional>
#include <string>
using ErrorCallback = std::function<void(const std::string&)>;
#define INFO_LOG qDebug()
#define WARNING_LOG qWarning()
