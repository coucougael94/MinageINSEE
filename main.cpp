#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include "weatherretriever.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Configuration de l'application
    app.setApplicationName("WeatherRetriever");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("DeathDataAnalysis");

    qDebug() << "=== Récupérateur de données météorologiques ===";
    qDebug() << "Version:" << app.applicationVersion();

    // Déterminer le répertoire des téléchargements
    QString downloadsPath="/home/pi/Downloads/BDALTIV2/";

    qDebug() << "Répertoire de recherche:" << downloadsPath;

    // Créer et démarrer le récupérateur
    WeatherRetriever* retriever = new WeatherRetriever(&app);
    retriever->processDeathFiles(downloadsPath);
    qDebug()<<"end";

    return app.exec();
}

///Necesiite BDATLIV2 clean


