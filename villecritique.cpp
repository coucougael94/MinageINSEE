#include "villecritique.h"
#include <QSqlError>
#include <array>
#include <QMap>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <algorithm>


#define DB_EMISSAIRE "/home/gael/Documents/cpp/cre/ClairVent/PyClairVent/Emissaire.db"
#include <QSqlError>

// Initialisation du cache statique

VilleCritique::VilleCritique(QObject *parent) : QObject(parent)
{
}



char* VilleCritique::getWindCoeffs(double latVille, double lonVille)
{
    static QMap<QString, std::array<char, 360>> cacheCity;
    QString key = QString("%1,%2").arg(latVille, 0, 'f', 6).arg(lonVille, 0, 'f', 6);

    if (cacheCity.contains(key)) {
        return cacheCity[key].data();
    }

    static std::array<char, 360> table_char = {0};

    // Use default connection - much simpler
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid()) {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(DB_EMISSAIRE);
    }

    if (!db.isOpen() && !db.open()) {
        qDebug() << "Erreur d'ouverture de la base de données VilleC:" << db.lastError().text();
        return table_char.data();
    }

    QSqlQuery query(db);
    query.prepare("SELECT lat, lon FROM Emissaire WHERE type_ LIKE '%cre%'");

    if (!query.exec()) {
        qDebug() << "Erreur lors de l'exécution de la requête:" << query.lastError().text();
        return table_char.data();
    }

    // Reset the array for this calculation
    table_char.fill(0);

    // Process the query results
    while (query.next()) {
        double latEmissaire = query.value("lat").toDouble();
        double lonEmissaire = query.value("lon").toDouble();
        double distance = haversine(latVille, lonVille, latEmissaire, lonEmissaire);

        if (distance <= 100.0) {
            int azimuth = static_cast<int>(std::round(calculateAzimuth(latVille, lonVille, latEmissaire, lonEmissaire)));
            if (azimuth >= 0 && azimuth < 360) {
                table_char[azimuth]++;
            }
        }
    }

    // Insert the calculated array into the cache
    cacheCity.insert(key, table_char);

    // Return a pointer to the cached array
    return cacheCity[key].data();
}



double VilleCritique::haversine(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double R = 6371.0; // Rayon de la Terre en km
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    double a = sin(dLat / 2) * sin(dLat / 2) +
               sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

double VilleCritique::calculateAzimuth(double lat1, double lon1, double lat2, double lon2)
{
    lat1 = lat1 * M_PI / 180.0;
    lon1 = lon1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;
    lon2 = lon2 * M_PI / 180.0;

    double dLon = lon2 - lon1;
    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
    double azimuth = atan2(y, x) * 180.0 / M_PI;
    return fmod(azimuth + 360.0, 360.0); // Normalisation entre 0 et 360
}
