#ifndef UPDATER_H
#define UPDATER_H

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QtNetwork>
#include <QFile>
#include <QUrl>

QT_BEGIN_NAMESPACE
namespace Ui {
class Updater;
}
QT_END_NAMESPACE

class Updater : public QMainWindow
{
    Q_OBJECT

public:
    Updater(QWidget *parent = nullptr);
    ~Updater();

private slots:
    void checkFileExists(const QString &url);
    void checkForUpdates();
    void checkCompletion(size_t files_checked, size_t files_to_download, size_t overall_file_size);
    void downloadUpdate();
    void downloadFiles(const QStringList &files);
    void launchGame();
    void setExecutable(const QString &filePath);
    QString calculateMd5(QFile &file);

    void on_checkButton_clicked();
    void on_optionButton_clicked();
    void on_launchButton_clicked();

private:
    Ui::Updater *ui;
    QNetworkAccessManager *networkManager;
    void showError(const QString &message);
    void showMessage(const QString &message);
};
#endif // UPDATER_H
