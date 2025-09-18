#ifndef WEATHER_H
#define WEATHER_H


#include "villecritique.h"

#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>


#include <algorithm>
#include <climits>

#include <cmath>


#define DB_WEATHER "/run/media/gael/GaelUSB/weather_data.db"


class Weather {
public:
    static const int deltaDayDeath;

    // Méthode pour itérer sur les lignes où deltadaydeath = deltaDayDeath

    // Méthode pour itérer sur les lignes où deltadaydeath = deltaDayDeath

    static int calculerDistance(const char* tableau, double wind_direction) {
        wind_direction--;
        int start_index = static_cast<int>(std::round(wind_direction)) % 360;
        int index_forward = start_index;
        int index_backward = start_index;
        int distance_forward = 0;
        int distance_backward = 0;

        // Parcours dans le sens des aiguilles d'une montre
        while (true) {
            if (tableau[index_forward] != 0) {
                break;
            }
            index_forward = (index_forward + 1) % 360;
            distance_forward++;
            if (index_forward == start_index) {
                distance_forward = INT_MAX; // Aucun élément non nul
                break;
            }
        }

        // Parcours dans le sens inverse
        while (true) {
            if (tableau[index_backward] != 0) {
                break;
            }
            index_backward = (index_backward - 1 + 360) % 360;
            distance_backward++;
            if (index_backward == start_index) {
                distance_backward = INT_MAX; // Aucun élément non nul
                break;
            }
        }

        // Retourne la distance signée
        if (distance_forward < distance_backward) {
            return distance_forward;
        } else if (distance_backward < distance_forward) {
            return -distance_backward;
        } else {
            return 0; // Les deux distances sont égales (ou INT_MAX)
        }
    }



    QString toString(const char* table_char)
    {
        QString result = "[";
        for (int i = 0; i < 360; ++i) {
            result += QString::number(static_cast<int>(table_char[i]));
            if (i < 359) {
                result += ",";
            }
        }
        result += "]";
        return result;
    }
    void iterateDead() {
        qDebug() << "w" << __LINE__;

        // Use a unique connection name
        static int connectionCounter = 0;
        connectionCounter++;
        QString connectionName = QString("weather_connection_%1").arg(connectionCounter);

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(DB_WEATHER);

        if (!db.open()) {
            qDebug() << "Erreur: impossible d'ouvrir la base de données.";
            QSqlDatabase::removeDatabase(connectionName);
            return;
        }

        qDebug() << "w" << __LINE__;

        // Scope the query
        {
            QSqlQuery query(db);
            query.prepare("SELECT latitude, longitude, wind_dir_10m, wind_dir_100m,* FROM weather_data WHERE deltadaydeath = :deltadaydeath");
            query.bindValue(":deltadaydeath", deltaDayDeath);

            if (!query.exec()) {
                qDebug() << "Erreur lors de l'exécution de la requête:" << query.lastError().text();
                db.close();
                QSqlDatabase::removeDatabase(connectionName);
                return;
            }

            while (query.next()) {
                double lat = query.value("latitude").toDouble();
                double lon = query.value("longitude").toDouble();
                char* stats = VilleCritique::getWindCoeffs(lat, lon);
                //qDebug() << toString(stats)<< calculerDistance(stats, query.value("wind_dir_10m").toDouble()) << calculerDistance(stats, query.value("wind_dir_10m").toDouble())  << query.value("wind_dir_100m").toString();
                //qDebug() << calculerDistance(stats, query.value("wind_dir_10m").toDouble()) << calculerDistance(stats, query.value("wind_dir_100m").toDouble());
                qDebug() << calculerDistance(stats, query.value("wind_dir_10mMin").toDouble())<<  calculerDistance(stats, query.value("wind_dir_10m").toDouble()) << calculerDistance(stats, query.value("wind_dir_10mMax").toDouble());
            }
            // query destructor called here
        }

        db.close();
        QSqlDatabase::removeDatabase(connectionName);
    }
};


// Définition de la constante statique

#endif // WEATHER_H
