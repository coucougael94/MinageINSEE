#ifndef VILLECRITIQUE_H
#define VILLECRITIQUE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QMap>

class VilleCritique : public QObject
{
    Q_OBJECT

public:
    explicit VilleCritique(QObject *parent = nullptr);
    static char* getWindCoeffs(double latVille, double lonVille);

private:
    static double haversine(double lat1, double lon1, double lat2, double lon2);
    static double calculateAzimuth(double lat1, double lon1, double lat2, double lon2);
    //static QMap<QString, char[360]> cacheCity;
};


#endif // VILLECRITIQUE_H
