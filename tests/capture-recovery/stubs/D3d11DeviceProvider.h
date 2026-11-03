#pragma once
#include <QObject>
#include <d3d11.h>
class D3D11DeviceProvider : public QObject {
    Q_OBJECT
public:
    static D3D11DeviceProvider* instance() { static D3D11DeviceProvider value; return &value; }
    ID3D11Device* acquireDevice() { return nullptr; }
signals:
    void deviceReady();
    void deviceLost();
};
