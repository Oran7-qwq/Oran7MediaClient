#include "Oran7RadiusGenerator.h"

Oran7RadiusGenerator::Oran7RadiusGenerator(QObject *parent)
    : QObject{parent}
{

}

Oran7RadiusGenerator::~Oran7RadiusGenerator()
{

}

QList<int> Oran7RadiusGenerator::generateRadius(int radiusBase)
{
    int radiusLG = radiusBase;
    int radiusSM = radiusBase;
    int radiusXS = radiusBase;
    int radiusOuter = radiusBase;

    if (radiusBase >= 16) {
        radiusLG = 16;
    } else if (radiusBase >= 6) {
        radiusLG = radiusBase + 2;
    } else if (radiusBase >= 5) {
        radiusLG = radiusBase + 1;
    }

    if (radiusBase >= 16) {
        radiusSM = 8;
    } else if (radiusBase >= 14) {
        radiusSM = 7;
    } else if (radiusBase >= 8) {
        radiusSM = 6;
    } else if (radiusBase >= 7) {
        radiusSM = 5;
    } else if (radiusBase >= 5) {
        radiusSM = 4;
    }

    if (radiusBase >= 6) {
        radiusXS = 2;
    } else if (radiusBase >= 2) {
        radiusXS = 1;
    }

    if (radiusBase >= 8) {
        radiusOuter = 6;
    } else if (radiusBase > 4) {
        radiusOuter = 4;
    }

    return {
        radiusBase,
        radiusLG,
        radiusSM,
        radiusXS,
        radiusOuter
    };
}
