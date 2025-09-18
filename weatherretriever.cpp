#include "weatherretriever.h"
#include <QStandardPaths>
#include <QCoreApplication>

WeatherRetriever::WeatherRetriever(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
    , timeoutTimer(new QTimer(this))
    , rateLimitTimer(new QTimer(this))
    , outputStream(nullptr)
    , outputFile(nullptr)
    , currentRecordIndex(0)
    , processedCount(0)
    , totalRecords(0)
{
    // Configuration des timers
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(30000); // 30 secondes timeout
    connect(timeoutTimer, &QTimer::timeout, this, &WeatherRetriever::onRequestTimeout);

    rateLimitTimer->setSingleShot(true);
    rateLimitTimer->setInterval(1100); // 1.1 seconde entre les requêtes pour respecter les limites
    connect(rateLimitTimer, &QTimer::timeout, this, &WeatherRetriever::processNextRecord);

    // Initialisation de la base de données SQLite
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("weather_data.db");

    if (!db.open()) {
        qDebug() << "Erreur d'ouverture de la base de données:" << db.lastError().text();
        return;
    }

    // Créer la table avec les nouvelles colonnes
    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS weather_data ("
           "sexe INTEGER, "
           "delta INTEGER, "
           "datedeces TEXT, "
           "latitude REAL, "
           "longitude REAL, "
           "wind_dir_10m REAL, "
           "wind_dir_100m REAL, "
           "wind_dir_10mMin REAL, "
           "wind_dir_100mMin REAL, "
           "wind_dir_10mMax REAL, "
           "wind_dir_100mMax REAL, "
           "temperature_2m REAL, "
           "temperature_2m_min REAL, "
           "temperature_2m_max REAL, "
           "relative_humidity_2m REAL, "
           "relative_humidity_2m_min REAL, "
           "relative_humidity_2m_max REAL, "
           "dewpoint_2m REAL, "
           "dewpoint_2m_min REAL, "
           "dewpoint_2m_max REAL, "
           "rain REAL, "
           "rain_min REAL, "
           "rain_max REAL, "
           "surface_pressure REAL, "
           "surface_pressure_min REAL, "
           "surface_pressure_max REAL, "
           "et0_fao_evapotranspiration REAL, "
           "et0_fao_evapotranspiration_min REAL, "
           "et0_fao_evapotranspiration_max REAL, "
           "vapour_pressure_deficit REAL, "
           "vapour_pressure_deficit_min REAL, "
           "vapour_pressure_deficit_max REAL, "
           "wind_speed_10m REAL, "
           "wind_speed_10m_min REAL, "
           "wind_speed_10m_max REAL, "
           "wind_speed_100m REAL, "
           "wind_speed_100m_min REAL, "
           "wind_speed_100m_max REAL, "
           "wind_gusts_10m REAL, "
           "wind_gusts_10m_min REAL, "
           "wind_gusts_10m_max REAL, "
           "cape REAL, "
           "cape_min REAL, "
           "cape_max REAL"
           ")");
}

void WeatherRetriever::processDeathFiles(const QString& directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists()) {
        qDebug() << "Répertoire non trouvé:" << directoryPath;
        return;
    }

    // Trouver tous les fichiers deces-*-clean.csv
    QStringList filters;
    filters << "deces-*-clean.csv";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        qDebug() << "Aucun fichier deces-*-clean.csv trouvé dans" << directoryPath;
        return;
    }

    setupOutputFile();

    // Charger tous les fichiers
    for (const QString& fileName : files) {
        QString fullPath = dir.absoluteFilePath(fileName);
        qDebug() << "Chargement du fichier:" << fileName;
        loadDeathRecords(fullPath);
    }

    totalRecords = deathRecords.size();
    qDebug() << "Total des enregistrements à traiter:" << totalRecords;

    if (totalRecords > 0) {
        currentRecordIndex = 0;
        processNextRecord();
    }
}

bool WeatherRetriever::loadDeathRecords(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Impossible d'ouvrir le fichier:" << filePath;
        return false;
    }

    QTextStream in(&file);
    QString line = in.readLine(); // Skip header

    while (!in.atEnd()) {
        line = in.readLine();
        if (line.isEmpty()) continue;

        // Parse CSV line: "sexe","delta","datedeces","latitude","longitude"
        QStringList parts = line.split(',');
        if (parts.size() < 5) continue;

        DeathRecord record;
        record.sexe = parts[0].toInt();
        record.delta = parts[1].toInt();

        // Parse date YYYYMMDD
        QString dateStr = parts[2];
        if (dateStr.length() == 8) {
            int year = dateStr.left(4).toInt();
            int month = dateStr.mid(4, 2).toInt();
            int day = dateStr.right(2).toInt();
            record.dateDeces = QDate(year, month, day);
        }

        record.latitude = parts[3].toDouble();
        record.longitude = parts[4].toDouble();

        // Vérifier si la date doit être exclue
        if (!isExcludedDate(record.dateDeces)) {
            deathRecords.append(record);
        }
    }

    return true;
}

bool WeatherRetriever::isHoliday(const QDate& date) const
{
    int year = date.year();
    QList<QDate> holidays = getHolidaysForYear(year);
    return holidays.contains(date);
}

bool WeatherRetriever::isExcludedDate(const QDate& date) const
{
    // Exclure les dimanches (7 = dimanche en Qt)
    if (date.dayOfWeek() == 7) {
        return true;
    }

    // Exclure les jours fériés
    return isHoliday(date);
}

QDate WeatherRetriever::calculateEaster(int year) const
{
    // Algorithme pour calculer Pâques
    int a = year % 19;
    int b = year / 100;
    int c = year % 100;
    int d = b / 4;
    int e = b % 4;
    int f = (b + 8) / 25;
    int g = (b - f + 1) / 3;
    int h = (19 * a + b - d - g + 15) % 30;
    int i = c / 4;
    int k = c % 4;
    int l = (32 + 2 * e + 2 * i - h - k) % 7;
    int m = (a + 11 * h + 22 * l) / 451;
    int month = (h + l - 7 * m + 114) / 31;
    int day = ((h + l - 7 * m + 114) % 31) + 1;

    return QDate(year, month, day);
}

QList<QDate> WeatherRetriever::getHolidaysForYear(int year) const
{
    QList<QDate> holidays;

    // Jours fériés fixes
    holidays << QDate(year, 1, 1);   // Nouvel An
    holidays << QDate(year, 5, 1);   // Fête du Travail
    holidays << QDate(year, 5, 8);   // Victoire 1945
    holidays << QDate(year, 7, 14);  // Fête Nationale
    holidays << QDate(year, 8, 15);  // Assomption
    holidays << QDate(year, 11, 1);  // Toussaint
    holidays << QDate(year, 11, 11); // Armistice
    holidays << QDate(year, 12, 25); // Noël

    // Jours fériés variables basés sur Pâques
    QDate easter = calculateEaster(year);
    holidays << easter.addDays(1);   // Lundi de Pâques
    holidays << easter.addDays(39);  // Ascension
    holidays << easter.addDays(50);  // Lundi de Pentecôte

    return holidays;
}

void WeatherRetriever::requestWeatherData(const DeathRecord& record)
{
    // Pour les données historiques anciennes, utiliser ERA5 via Open-Meteo
    // Les données d'archive sont disponibles depuis 1940
    QString baseUrl = "https://customer-archive-api.open-meteo.com/v1/archive";

    QUrlQuery query;
    query.addQueryItem("latitude", QString::number(record.latitude, 'f', 6));
    query.addQueryItem("longitude", QString::number(record.longitude, 'f', 6));
    query.addQueryItem("start_date", record.dateDeces.toString("yyyy-MM-dd"));
    query.addQueryItem("end_date", record.dateDeces.toString("yyyy-MM-dd"));

    // Ajouter toutes les variables météorologiques
    query.addQueryItem("hourly", "wind_direction_10m,wind_direction_100m,temperature_2m,relative_humidity_2m,dewpoint_2m,rain,surface_pressure,et0_fao_evapotranspiration,vapour_pressure_deficit,wind_speed_10m,wind_speed_100m,wind_gusts_10m,cape");

    query.addQueryItem("timezone", "Europe/Paris");
    query.addQueryItem("apikey", "");

    QUrl url(baseUrl);
    url.setQuery(query);

    qDebug() << "Requête URL:" << url.toString();
    qDebug() << "Date:" << record.dateDeces.toString("yyyy-MM-dd")
             << "Coords:" << record.latitude << "," << record.longitude;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "WeatherRetriever/1.0");
    request.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &WeatherRetriever::onWeatherDataReceived);

    timeoutTimer->start();
}

void WeatherRetriever::onWeatherDataReceived()
{
    timeoutTimer->stop();

    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    WeatherData weather;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        qDebug() << "Réponse reçue, taille:" << data.size();

        // Afficher les premiers caractères pour le débogage
        if (!data.isEmpty()) {
            QString preview = QString::fromUtf8(data.left(200));
            qDebug() << "Preview de la réponse:" << preview;
        }

        weather = parseWeatherResponse(data);

        if (weather.isValid) {
            qDebug() << "Données météo valides - Wind 10m:" << weather.windDir10m
                     << "Temperature:" << weather.temperature2m;
        } else {
            qDebug() << "Données météo invalides ou manquantes";
        }
    } else {
        qDebug() << "Erreur réseau:" << reply->errorString();
        qDebug() << "Code d'erreur:" << reply->error();
        qDebug() << "Status HTTP:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }

    // Écrire les données dans la base de données
    if (currentRecordIndex < deathRecords.size()) {
        writeRecordToDatabase(deathRecords[currentRecordIndex], weather);
        processedCount++;

        if (processedCount % 10 == 0) {
            qDebug() << "Traité:" << processedCount << "/" << totalRecords;
        }
    }

    reply->deleteLater();

    // Passer au prochain enregistrement
    currentRecordIndex++;
    if (currentRecordIndex < deathRecords.size()) {
        rateLimitTimer->start();
    } else {
        qDebug() << "Traitement terminé. Total traité:" << processedCount;
        if (outputFile) {
            outputFile->close();
            delete outputFile;
            outputFile = nullptr;
        }
        QCoreApplication::quit();
    }
}

void WeatherRetriever::onRequestTimeout()
{
    qDebug() << "Timeout pour l'enregistrement" << currentRecordIndex;

    // Écrire un enregistrement avec des données météo invalides
    if (currentRecordIndex < deathRecords.size()) {
        WeatherData invalidWeather;
        writeRecordToDatabase(deathRecords[currentRecordIndex], invalidWeather);
        processedCount++;
    }

    // Continuer avec le prochain enregistrement
    currentRecordIndex++;
    if (currentRecordIndex < deathRecords.size()) {
        rateLimitTimer->start();
    } else {
        qDebug() << "Traitement terminé. Total traité:" << processedCount;
        if (outputFile) {
            outputFile->close();
            delete outputFile;
            outputFile = nullptr;
        }
        QCoreApplication::quit();
    }
}

void WeatherRetriever::processNextRecord()
{
    if (currentRecordIndex < deathRecords.size()) {
        requestWeatherData(deathRecords[currentRecordIndex]);
    }
}

// Fonction utilitaire pour calculer min, max et moyenne
void WeatherRetriever::calculateStats(const QJsonArray& data, double& mean, double& min, double& max, int startHour, int endHour)
{
    mean = 0.0;
    min = 0.0;
    max = 0.0;
    double sum = 0.0;
    int count = 0;
    bool firstValue = true;

    int actualEnd = qMin(endHour, data.size());
    for (int i = startHour; i < actualEnd; ++i) {
        QJsonValue val = data[i];
        if (!val.isNull()) {
            double value = val.toDouble();
            sum += value;
            count++;

            if (firstValue) {
                min = value;
                max = value;
                firstValue = false;
            } else {
                if (value < min) min = value;
                if (value > max) max = value;
            }
        }
    }

    if (count > 0) {
        mean = sum / count;
    }
}

WeatherData WeatherRetriever::parseWeatherResponse(const QByteArray& response)
{
    WeatherData weather;

    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isNull()) {
        qDebug() << "Erreur: JSON invalide";
        return weather;
    }

    QJsonObject obj = doc.object();

    // Vérifier s'il y a une erreur dans la réponse
    if (obj.contains("error")) {
        qDebug() << "Erreur API:" << obj["error"].toString();
        return weather;
    }

    QJsonObject hourly = obj["hourly"].toObject();

    if (hourly.isEmpty()) {
        qDebug() << "Pas de données horaires dans la réponse";
        qDebug() << "Clés disponibles:" << obj.keys();
        return weather;
    }

    // Récupérer toutes les données horaires
    QJsonArray windDir10m = hourly["wind_direction_10m"].toArray();
    QJsonArray windDir100m = hourly["wind_direction_100m"].toArray();
    QJsonArray temperature2m = hourly["temperature_2m"].toArray();
    QJsonArray relativeHumidity2m = hourly["relative_humidity_2m"].toArray();
    QJsonArray dewpoint2m = hourly["dewpoint_2m"].toArray();
    QJsonArray rain = hourly["rain"].toArray();
    QJsonArray surfacePressure = hourly["surface_pressure"].toArray();
    QJsonArray et0FaoEvapotranspiration = hourly["et0_fao_evapotranspiration"].toArray();
    QJsonArray vapourPressureDeficit = hourly["vapour_pressure_deficit"].toArray();
    QJsonArray windSpeed10m = hourly["wind_speed_10m"].toArray();
    QJsonArray windSpeed100m = hourly["wind_speed_100m"].toArray();
    QJsonArray windGusts10m = hourly["wind_gusts_10m"].toArray();

    qDebug() << "Données récupérées - Taille arrays:"
             << "windDir10m:" << windDir10m.size()
             << "temperature2m:" << temperature2m.size()
             << "windSpeed10m:" << windSpeed10m.size();

    // Calculer les statistiques pour chaque variable (8h-18h)
    int startHour = 8;
    int endHour = 18;

    // Direction du vent - traitement spécial pour les angles
    if (!windDir10m.isEmpty()) {
        calculateStats(windDir10m, weather.windDir10m, weather.windDir10mMin, weather.windDir10mMax, startHour, endHour);
    }
    if (!windDir100m.isEmpty()) {
        calculateStats(windDir100m, weather.windDir100m, weather.windDir100mMin, weather.windDir100mMax, startHour, endHour);
    }

    // Température
    if (!temperature2m.isEmpty()) {
        calculateStats(temperature2m, weather.temperature2m, weather.temperature2mMin, weather.temperature2mMax, startHour, endHour);
    }

    // Humidité relative
    if (!relativeHumidity2m.isEmpty()) {
        calculateStats(relativeHumidity2m, weather.relativeHumidity2m, weather.relativeHumidity2mMin, weather.relativeHumidity2mMax, startHour, endHour);
    }

    // Point de rosée
    if (!dewpoint2m.isEmpty()) {
        calculateStats(dewpoint2m, weather.dewpoint2m, weather.dewpoint2mMin, weather.dewpoint2mMax, startHour, endHour);
    }

    // Pluie
    if (!rain.isEmpty()) {
        calculateStats(rain, weather.rain, weather.rainMin, weather.rainMax, startHour, endHour);
    }

    // Pression de surface
    if (!surfacePressure.isEmpty()) {
        calculateStats(surfacePressure, weather.surfacePressure, weather.surfacePressureMin, weather.surfacePressureMax, startHour, endHour);
    }

    // Évapotranspiration
    if (!et0FaoEvapotranspiration.isEmpty()) {
        calculateStats(et0FaoEvapotranspiration, weather.et0FaoEvapotranspiration, weather.et0FaoEvapotranspirationMin, weather.et0FaoEvapotranspirationMax, startHour, endHour);
    }

    // Déficit de pression de vapeur
    if (!vapourPressureDeficit.isEmpty()) {
        calculateStats(vapourPressureDeficit, weather.vapourPressureDeficit, weather.vapourPressureDeficitMin, weather.vapourPressureDeficitMax, startHour, endHour);
    }

    // Vitesse du vent 10m
    if (!windSpeed10m.isEmpty()) {
        calculateStats(windSpeed10m, weather.windSpeed10m, weather.windSpeed10mMin, weather.windSpeed10mMax, startHour, endHour);
    }

    // Vitesse du vent 100m
    if (!windSpeed100m.isEmpty()) {
        calculateStats(windSpeed100m, weather.windSpeed100m, weather.windSpeed100mMin, weather.windSpeed100mMax, startHour, endHour);
    }

    // Rafales de vent 10m
    if (!windGusts10m.isEmpty()) {
        calculateStats(windGusts10m, weather.windGusts10m, weather.windGusts10mMin, weather.windGusts10mMax, startHour, endHour);
    }
    QJsonArray cape = hourly["cape"].toArray();
	if (!cape.isEmpty()) {
	    calculateStats(cape, weather.cape, weather.capeMin, weather.capeMax, startHour, endHour);
	}


    // Marquer comme valide si au moins une mesure a été récupérée
    if (!windDir10m.isEmpty() || !temperature2m.isEmpty() || !windSpeed10m.isEmpty()) {
        weather.isValid = true;
        qDebug() << "Données calculées - Temp:" << weather.temperature2m
                 << "Humidité:" << weather.relativeHumidity2m
                 << "Vitesse vent 10m:" << weather.windSpeed10m;
    } else {
        qDebug() << "Aucune donnée valide trouvée";
    }

    return weather;
}

void WeatherRetriever::writeRecordToDatabase(const DeathRecord& record, const WeatherData& weather)
{
    // Vérifier si la connexion à la base de données est valide
    if (!db.isOpen()) {
        qDebug() << "La base de données n'est pas ouverte!";
        return;
    }

    QSqlQuery query;
    // Préparer la requête d'insertion avec toutes les nouvelles colonnes
    query.prepare("INSERT INTO weather_data (sexe, delta, datedeces, latitude, longitude, "
              "wind_dir_10m, wind_dir_100m, wind_dir_10mMin, wind_dir_100mMin, wind_dir_10mMax, wind_dir_100mMax, "
              "temperature_2m, temperature_2m_min, temperature_2m_max, "
              "relative_humidity_2m, relative_humidity_2m_min, relative_humidity_2m_max, "
              "dewpoint_2m, dewpoint_2m_min, dewpoint_2m_max, "
              "rain, rain_min, rain_max, "
              "surface_pressure, surface_pressure_min, surface_pressure_max, "
              "et0_fao_evapotranspiration, et0_fao_evapotranspiration_min, et0_fao_evapotranspiration_max, "
              "vapour_pressure_deficit, vapour_pressure_deficit_min, vapour_pressure_deficit_max, "
              "wind_speed_10m, wind_speed_10m_min, wind_speed_10m_max, "
              "wind_speed_100m, wind_speed_100m_min, wind_speed_100m_max, "
              "wind_gusts_10m, wind_gusts_10m_min, wind_gusts_10m_max, "
              "cape, cape_min, cape_max) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    // Lier les paramètres de base
    query.addBindValue(record.sexe);  // 1
    query.addBindValue(record.delta);  // 2
    query.addBindValue(record.dateDeces.toString("yyyyMMdd"));  // 3
    query.addBindValue(record.latitude);  // 4
    query.addBindValue(record.longitude);  // 5

    if (weather.isValid) {
        // Ajouter les valeurs météo (36 paramètres supplémentaires)
        query.addBindValue(weather.windDir10m);  // 6
        query.addBindValue(weather.windDir100m);  // 7
        query.addBindValue(weather.windDir10mMin);  // 8
        query.addBindValue(weather.windDir100mMin);  // 9
        query.addBindValue(weather.windDir10mMax);  // 10
        query.addBindValue(weather.windDir100mMax);  // 11

        query.addBindValue(weather.temperature2m);  // 12
        query.addBindValue(weather.temperature2mMin);  // 13
        query.addBindValue(weather.temperature2mMax);  // 14

        query.addBindValue(weather.relativeHumidity2m);  // 15
        query.addBindValue(weather.relativeHumidity2mMin);  // 16
        query.addBindValue(weather.relativeHumidity2mMax);  // 17

        query.addBindValue(weather.dewpoint2m);  // 18
        query.addBindValue(weather.dewpoint2mMin);  // 19
        query.addBindValue(weather.dewpoint2mMax);  // 20

        query.addBindValue(weather.rain);  // 21
        query.addBindValue(weather.rainMin);  // 22
        query.addBindValue(weather.rainMax);  // 23

        query.addBindValue(weather.surfacePressure);  // 24
        query.addBindValue(weather.surfacePressureMin);  // 25
        query.addBindValue(weather.surfacePressureMax);  // 26

        query.addBindValue(weather.et0FaoEvapotranspiration);  // 27
        query.addBindValue(weather.et0FaoEvapotranspirationMin);  // 28
        query.addBindValue(weather.et0FaoEvapotranspirationMax);  // 29

        query.addBindValue(weather.vapourPressureDeficit);  // 30
        query.addBindValue(weather.vapourPressureDeficitMin);  // 31
        query.addBindValue(weather.vapourPressureDeficitMax);  // 32

        query.addBindValue(weather.windSpeed10m);  // 33
        query.addBindValue(weather.windSpeed10mMin);  // 34
        query.addBindValue(weather.windSpeed10mMax);  // 35

        query.addBindValue(weather.windSpeed100m);  // 36
        query.addBindValue(weather.windSpeed100mMin);  // 37
        query.addBindValue(weather.windSpeed100mMax);  // 38

        query.addBindValue(weather.windGusts10m);  // 39
        query.addBindValue(weather.windGusts10mMin);  // 40
        query.addBindValue(weather.windGusts10mMax);  // 41
						      //
						      //
	
	    query.addBindValue(weather.cape);
	    query.addBindValue(weather.capeMin);
	    query.addBindValue(weather.capeMax);
    } else {
        // Ajouter des valeurs NULL pour toutes les colonnes météo (36 valeurs NULL)
        for (int i = 0; i < 39; ++i) {
            query.addBindValue(QVariant());  // 6 à 41 : 36 NULL
        }
    }

    // Vérification du nombre de paramètres bindés
    qDebug() << "Number of bound values: " << query.boundValues().count();


    // Exécuter la requête
    if (!query.exec()) {
        qDebug() << "Erreur lors de l'insertion dans la base de données:" << query.lastError().text();
    } else {
        qDebug() << "Données insérées avec succès.";
    }
}

void WeatherRetriever::setupDatabase()
{
}

void WeatherRetriever::setupOutputFile()
{
    QString outputPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/deces_with_weather_extended.csv";

    outputFile = new QFile(outputPath);
    if (!outputFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Impossible de créer le fichier de sortie:" << outputPath;
        return;
    }

    outputStream = new QTextStream(outputFile);

    // Écrire l'en-tête avec toutes les nouvelles colonnes
    *outputStream << "sexe,delta,datedeces,latitude,longitude,"
                  << "wind_dir_10m,wind_dir_100m,wind_dir_10mMin,wind_dir_100mMin,wind_dir_10mMax,wind_dir_100mMax,"
                  << "temperature_2m,temperature_2m_min,temperature_2m_max,"
                  << "relative_humidity_2m,relative_humidity_2m_min,relative_humidity_2m_max,"
                  << "dewpoint_2m,dewpoint_2m_min,dewpoint_2m_max,"
                  << "rain,rain_min,rain_max,"
                  << "surface_pressure,surface_pressure_min,surface_pressure_max,"
                  << "et0_fao_evapotranspiration,et0_fao_evapotranspiration_min,et0_fao_evapotranspiration_max,"
                  << "vapour_pressure_deficit,vapour_pressure_deficit_min,vapour_pressure_deficit_max,"
                  << "wind_speed_10m,wind_speed_10m_min,wind_speed_10m_max,"
                  << "wind_speed_100m,wind_speed_100m_min,wind_speed_100m_max,"
                  << "wind_gusts_10m,wind_gusts_10m_min,wind_gusts_10m_max,"
		      << "cape,cape_min,cape_max\n";




    qDebug() << "Fichier de sortie créé:" << outputPath;
}

