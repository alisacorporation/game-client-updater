#include "updater.h"
#include "ui_updater.h"

#include <QDir>
#include <QFile>
#include <QGraphicsView>
#include <QImageReader>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QResource>
#include <QVBoxLayout>
#include <QtNetwork>

const QString SERVER_URL = "https://updater.domain.com/";
const QString PATCH_DIR = "patch";
const QString GAME_EXECUTABLE = "L2.exe";
const QString VERSION_FILE = "version.txt";
const QString MANIFEST_FILE = "manifest.txt";

Updater::Updater(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::Updater),
    networkManager(new QNetworkAccessManager(this)) {
    ui->setupUi(this);

    QPixmap background(":/neon.webp");

    QPalette palette;
    palette.setBrush(QPalette::Window,
                     background.scaled(this->size(),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation));
    this->setPalette(palette);
    this->setAutoFillBackground(true);

    ui->downloadProgressBar->setValue(0);
    ui->downloadProgressBar->setMaximum(100);

    ui->overallProgressBar->setValue(0);
    ui->overallProgressBar->setMaximum(100);

    checkForUpdates();
}

Updater::~Updater() { delete ui; }

void Updater::checkFileExists(const QString &url) {
    QUrl fileUrl(url);
    QNetworkRequest request(fileUrl);
    QNetworkReply *reply = networkManager->head(request);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug()
            << "File does not exist. HTTP Status:"
            << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString errorMessage = "Error: " + reply->errorString();
            showError(errorMessage);
            reply->deleteLater();
            return;
        }

        qDebug() << "File exists on server.";

        reply->deleteLater();
    });
}

void Updater::checkForUpdates() {
    qDebug() << "Updater::checkForUpdates";

    ui->statusLabel->setText("Checking for updates...");
    ui->launchButton->setEnabled(false);

    const QString versionUrl = SERVER_URL + VERSION_FILE;

    QUrl fileUrl(versionUrl);
    QNetworkRequest request(fileUrl);
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Failed to download version file.";
            showError("Failed to download version file.");
            reply->deleteLater();
            return;
        }

        const QString remoteVersion = QString(reply->readAll()).trimmed();
        reply->deleteLater();

        QRegularExpression versionRegex(R"(\d+\.\d+\.\d+)");

        if (!versionRegex.match(remoteVersion).hasMatch()) {
            showError(
                QString("Invalid version format in remote %1.").arg(VERSION_FILE));
            return;
        }

        qDebug() << "version [remote]: " << remoteVersion;
        bool patched = false;
        qDebug() << "patched: " << patched;

        if (!patched) {
            ui->statusLabel->setText("Update available. Downloading...");
            downloadUpdate();
        } else {
            qDebug() << "game is up to date";
            ui->statusLabel->setText("Game is up to date.");
            ui->launchButton->setEnabled(true);
        }
    });
}

void Updater::downloadUpdate() {
    qDebug() << "Updater::downloadUpdate";
    const QString manifestUrl = SERVER_URL + MANIFEST_FILE;
    qDebug() << "manifest url [remote]: " << manifestUrl;

    QUrl fileUrl(manifestUrl);
    QNetworkRequest request(fileUrl);
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            showError("Failed to download manifest file.");
            reply->deleteLater();
            return;
        }

        const QStringList filesToDownload =
            QString(reply->readAll()).split("\n", Qt::SkipEmptyParts);
        reply->deleteLater();

        qDebug() << "filesToDownload: " << filesToDownload.size();
        downloadFiles(filesToDownload);
    });
}

void Updater::downloadFiles(const QStringList &files) {
    auto overall_file_size = std::make_shared<size_t>(0);
    auto downloaded_size = std::make_shared<size_t>(0);
    auto files_checked = std::make_shared<size_t>(0);
    size_t files_to_download = files.size();

    for (const QString &file : files) {
        QStringList fileDetails = file.split(":");
        if (fileDetails.size() == 3) {
            *overall_file_size += fileDetails.at(2).toInt();
        }
    }

    double scaling_factor_overall_progress =
        static_cast<double>(100) / *overall_file_size;

    for (const QString &file : files) {
        QStringList fileDetails = file.split(":");
        if (fileDetails.size() != 3) {
            qDebug() << "Invalid file entry in manifest:" << file;
            continue;
        }

        QString remoteFile = SERVER_URL + PATCH_DIR + "/" + fileDetails.at(0);
        const QString localFile = QDir::currentPath() + "/" + fileDetails.at(0);
        QString remote_md5 = fileDetails.at(1);
        size_t remote_size = fileDetails.at(2).toInt();

        QFile localFileObj(localFile);
        bool fileMatches = false;

        if (localFileObj.exists() && localFileObj.size() == remote_size) {
            QString localMd5 = calculateMd5(localFileObj);
            if (localMd5 == remote_md5) {
                fileMatches = true;
            }
        }

        if (fileMatches) {
            // Increment progress for matched files
            (*files_checked)++;
            (*downloaded_size) += remote_size;

            qDebug() << "File checked [fileMatches]: " << *files_checked << " / "
                     << files_to_download << " | Overall: " << *downloaded_size
                     << " / " << *overall_file_size;
            double scaling_factor_download_progress =
                static_cast<double>(100) / remote_size;
            int scaled_value_overall_progress = static_cast<int>(
                (*downloaded_size) * scaling_factor_overall_progress);
            int scaled_value_download_progress =
                static_cast<int>(remote_size * scaling_factor_download_progress);
            ui->downloadProgressBar->setValue(scaled_value_download_progress);
            ui->overallProgressBar->setValue(scaled_value_overall_progress);

            // QApplication::processEvents();

            const QString labelCheckedFiles =
                "Check file: " + QString::number(*files_checked) + " / " +
                QString::number(files_to_download) + " (" + fileDetails.at(0) + ")";
            ui->statusLabel->setText(labelCheckedFiles);
            checkCompletion(*files_checked, files_to_download, *overall_file_size);
            continue;
        }

        // File requires download
        ui->statusLabel->setText("Downloading: " + fileDetails.at(0));

        QNetworkReply *reply =
            networkManager->get(QNetworkRequest(QUrl(remoteFile)));

        connect(
            reply, &QNetworkReply::downloadProgress,
            [this, downloaded_size, scaling_factor_overall_progress, remote_size,
             remoteFile, fileDetails](qint64 bytesReceived, qint64 bytesTotal) {
                static qint64 lastBytesReceived = 0; // Track previous progress

                // Calculate the incremental bytes received
                qint64 increment = bytesReceived - lastBytesReceived;
                lastBytesReceived = bytesReceived;

                double scaling_factor_download_progress =
                    static_cast<double>(100) / remote_size;

                int scaled_value_download_progress = static_cast<int>(
                    bytesReceived * scaling_factor_download_progress);
                ui->downloadProgressBar->setValue(scaled_value_download_progress);

                *downloaded_size += increment;
                int scaled_value_overall_progress = static_cast<int>(
                    (*downloaded_size) * scaling_factor_overall_progress);
                ui->overallProgressBar->setValue(scaled_value_overall_progress);

                // qDebug() << "[downloadProgress][remoteFile] " << remoteFile
                //          << " [Received/Total] " << bytesReceived << " / "
                //          << bytesTotal;

                // int scaled_value_overall_progress = static_cast<int>(
                //     (*downloaded_size) * scaling_factor_overall_progress);
                // ui->overallProgressBar->setValue(scaled_value_overall_progress);

                // Calculate percentages for logging
                double progressPercent =
                    bytesTotal > 0
                        ? (static_cast<double>(bytesReceived) / bytesTotal) * 100.0
                        : 0.0;
                double overallPercent =
                    ui->overallProgressBar->maximum() > 0
                        ? (static_cast<double>(ui->overallProgressBar->value()) /
                           ui->overallProgressBar->maximum()) *
                              100.0
                        : 0.0;

                qDebug() << "[downloadProgress][remoteFile] " << remoteFile << " : "
                         << bytesReceived << " / " << bytesTotal << " ("
                         << progressPercent << "%)";
                qDebug() << "Overall Progress: " << ui->overallProgressBar->value()
                         << "/" << ui->overallProgressBar->maximum() << " ("
                         << overallPercent << "%)";

                ui->statusLabel->setText("Downloading: " + fileDetails.at(0));
                QApplication::processEvents();
            });

        connect(reply, &QNetworkReply::finished,
                [this, reply, localFile, files_checked, files_to_download,
                 downloaded_size, overall_file_size]() {
                    if (reply->error() != QNetworkReply::NoError) {
                        showError("Failed to download " + localFile);
                        reply->deleteLater();
                        return;
                    }

                    QFile file(localFile);
                    QDir dir = QFileInfo(localFile).absoluteDir();
                    if (!dir.exists() && !dir.mkpath(".")) {
                        qDebug() << "Failed to create directory:" << dir.absolutePath();
                        reply->deleteLater();
                        return;
                    }

                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(reply->readAll());
                        file.close();
                        qDebug() << "File saved successfully to" << localFile;

                        (*files_checked)++;
                    } else {
                        qDebug() << "Failed to save" << localFile;
                        reply->deleteLater();
                        return;
                    }

                    qDebug() << "File checked [finished]: " << *files_checked << "/"
                             << files_to_download
                             << " | Overall: " << *downloaded_size << " / "
                             << *overall_file_size;

                    const QString labelCheckedFiles =
                        "Check file: " + QString::number(*files_checked) + " / " +
                        QString::number(files_to_download);
                    ui->statusLabel->setText(labelCheckedFiles);
                    checkCompletion(*files_checked, files_to_download,
                                    *overall_file_size);

                    // Calculate percentages for logging
                    double progressPercent =
                        ui->downloadProgressBar->maximum() > 0
                            ? (static_cast<double>(ui->downloadProgressBar->value()) /
                               ui->downloadProgressBar->maximum()) *
                                  100.0
                            : 0.0;
                    double overallPercent =
                        ui->overallProgressBar->maximum() > 0
                            ? (static_cast<double>(ui->overallProgressBar->value()) /
                               ui->overallProgressBar->maximum()) *
                                  100.0
                            : 0.0;

                    reply->deleteLater();
                });
    }
}

void Updater::checkCompletion(size_t files_checked, size_t files_to_download,
                              size_t overall_file_size) {
    if (files_checked == files_to_download) {
        ui->statusLabel->setText("Update complete. Ready to launch.");
        ui->launchButton->setEnabled(true);
        ui->overallProgressBar->setValue(ui->overallProgressBar->maximum());
        ui->downloadProgressBar->setValue(ui->downloadProgressBar->maximum());
    }
}

void Updater::setExecutable(const QString &filePath) {
    QFileInfo exeFileInfo(filePath);
    if (exeFileInfo.suffix() == "exe") {
        QFile exeFile(filePath);

        // Check if the file is executable
        if (!exeFile.permissions().testFlag(QFileDevice::ExeUser)) {
            qDebug() << "Debug: File is not executable, fixing permissions.";

            // Set the file as executable
            if (!exeFile.setPermissions(exeFile.permissions() |
                                        QFileDevice::ExeUser)) {
                qDebug() << "Error: Unable to set executable permissions for"
                         << filePath;
            } else {
                qDebug() << "Debug: Permissions set to executable for" << filePath;
            }
        } else {
            qDebug() << "Debug: File is already executable.";
        }
    }
}

QString Updater::calculateMd5(QFile &file) {
    QCryptographicHash hash(QCryptographicHash::Md5);
    if (file.open(QIODevice::ReadOnly)) {
        if (hash.addData(&file)) {
            return hash.result().toHex();
        }
    }
    return QString();
}

void Updater::launchGame() {
    if (!ui->launchButton->isEnabled())
        return;

    QString gamePath = QDir::currentPath() + "/system/" + GAME_EXECUTABLE;
    qDebug() << "gamePath" << gamePath;

    if (!QFile::exists(gamePath)) {
        showError("Game executable not found.");
        return;
    }

    setExecutable(gamePath);

    QProcess *process = new QProcess(this);
    process->setProgram(gamePath);
    process->setStandardOutputFile("game_output.log");
    process->setStandardErrorFile("game_error.log");
    process->start();

    // Check if the process was started successfully
    if (process->state() == QProcess::Running) {
        // Get the PID
        qint64 pid = process->processId();
        qDebug() << "Game launched with PID:" << pid;
    } else {
        qDebug() << "Something goes wrong when launching l2.exe";
    }
}

void Updater::showError(const QString &message) {
    QMessageBox::critical(this, "Error", message);
}

void Updater::showMessage(const QString &message) {
    QMessageBox::information(this, "Information", message);
}

void Updater::on_checkButton_clicked() { checkForUpdates(); }

void Updater::on_launchButton_clicked() { launchGame(); }

void Updater::on_optionButton_clicked() {}
