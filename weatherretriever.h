#ifndef WEATHERRETRIEVER_H
#define WEATHERRETRIEVER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDate>
#include <QDebug>
#include <QUrlQuery>
#include <QDir>


#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>


struct DeathRecord {
    int sexe;
    int delta;
    QDate dateDeces;
    double latitude;
    double longitude;
};






struct WeatherData {
    double windDir10m;
    double windDir100m;

    double windDir10mMin;
    double windDir100mMin;

    double windDir10mMax;
    double windDir100mMax;



    double temperature2m;
    double temperature2mMin;
    double temperature2mMax;
    double relativeHumidity2m;
    double relativeHumidity2mMin;
    double relativeHumidity2mMax;

        double dewpoint2m;
    double dewpoint2mMin;
    double dewpoint2mMax;

        double rain;
    double rainMin;
    double rainMax;

    double surfacePressure;
    double surfacePressureMin;
    double surfacePressureMax;

    double et0FaoEvapotranspiration;
    double et0FaoEvapotranspirationMin;
    double et0FaoEvapotranspirationMax;

    double vapourPressureDeficit;
    double vapourPressureDeficitMin;
    double vapourPressureDeficitMax;

    double windSpeed10m;
    double windSpeed10mMin;
    double windSpeed10mMax;

    double windSpeed100m;
    double windSpeed100mMin;
    double windSpeed100mMax;

        double windGusts10m;
    double windGusts10mMin;
    double windGusts10mMax;



    
    double cape;
    double capeMin;
    double capeMax;




    bool isValid;

    WeatherData() : windDir10m(0), windDir100m(0), windDir10mMin(0), windDir100mMin(0), windDir10mMax(0), windDir100mMax(0), isValid(false) {}
};

class WeatherRetriever : public QObject
{
    Q_OBJECT

public:
    explicit WeatherRetriever(QObject *parent = nullptr);
    void processDeathFiles(const QString& directoryPath);

private slots:
    void onWeatherDataReceived();
    void onRequestTimeout();
    void processNextRecord();

private:
    QNetworkAccessManager* networkManager;
    QTimer* timeoutTimer;
    QTimer* rateLimitTimer;

    QList<DeathRecord> deathRecords;
    QTextStream* outputStream;
    QFile* outputFile;

    int currentRecordIndex;
    int processedCount;
    int totalRecords;

    // Jours fériés français fixes
    QList<QDate> fixedHolidays;

    bool loadDeathRecords(const QString& filePath);
    bool isHoliday(const QDate& date) const;
    bool isExcludedDate(const QDate& date) const;
    QDate calculateEaster(int year) const;
    void calculateStats(const QJsonArray& data, double& mean, double& min, double& max, int startHour, int endHour);
    QList<QDate> getHolidaysForYear(int year) const;
    void requestWeatherData(const DeathRecord& record);
    WeatherData parseWeatherResponse(const QByteArray& response);
    void writeRecordToDatabase(const DeathRecord& record, const WeatherData& weather);
    void setupOutputFile();
    void processAllFiles(const QString& directoryPath);

    void setupDatabase();


    QSqlDatabase db;  // Base de données SQLite

};

#endif // WEATHERRETRIEVER_H

