#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QProcess>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Roku Smart Watchdog");
    window.resize(400, 250);

    QVBoxLayout *layout = new QVBoxLayout(&window);
    QNetworkAccessManager *networkManager = new QNetworkAccessManager(&window);

    layout->addWidget(new QLabel("Roku IP Address:"));
    QLineEdit *ipInput = new QLineEdit(&window);
    ipInput->setPlaceholderText("192.168.0.178");
    layout->addWidget(ipInput);

    QLabel *statusLabel = new QLabel("Status: Waiting for IP...", &window);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    QPushButton *button = new QPushButton("Start Monitoring", &window);
    button->setMinimumHeight(60);
    layout->addWidget(button);

    // This timer drives the logic
    QTimer *logicTimer = new QTimer(&window);
    logicTimer->setSingleShot(true); // We will manually restart it based on state

    // Logic Constants
    const int checkIntervalMs = 120 * 1000;         // Check status every 10 seconds
    const int thirtyMinutesMs = 30 * 60 * 1000;    // The actual sleep timer duration
    
    // State variable: true if we are currently waiting for the 30min to expire
    static bool isCountingDown = false;

    // --- The Core Logic Function ---
    auto runLogicCheck = [=]() {
        static bool countingDown = false; // Persistent state within the logic
        QString ip = ipInput->text().trimmed();
        
        if (ip.isEmpty()) return;

        QUrl infoUrl("http://" + ip + ":8060/query/device-info");
        QNetworkReply *reply = networkManager->get(QNetworkRequest(infoUrl));

        QObject::connect(reply, &QNetworkReply::finished, [=]() mutable {
            if (reply->error() == QNetworkReply::NoError) {
                QString xmlData = reply->readAll();
                bool isCurrentlyOn = xmlData.contains("<power-mode>PowerOn</power-mode>");

                if (isCurrentlyOn) {
                    if (!isCountingDown) {
                        // TV JUST TURNED ON: Start the 30-minute wait
                        isCountingDown = true;
                        statusLabel->setText("Roku TV Turned ON! Shutdown in 30 minutes at: " + 
                                            QDateTime::currentDateTime().addMSecs(thirtyMinutesMs).toString("hh:mm:ss AP"));
                        logicTimer->start(thirtyMinutesMs);
                    } else {
                        // TIMER EXPIRED: Turn it off now
                        qDebug() << "30 minutes up. Sending PowerOff.";
                        QProcess::startDetached("curl", QStringList() << "-d" << "" << "http://" + ip + ":8060/keypress/poweroff");
                        statusLabel->setText("Shutdown sent. Returning to monitor mode...");
                        isCountingDown = false;
                        logicTimer->start(checkIntervalMs);
                    }
                } else {
                    // TV IS OFF: Keep monitoring
                    isCountingDown = false;
                    statusLabel->setText("Roku TV is OFF. Monitoring every 60s... (Checked: " + 
                                        QDateTime::currentDateTime().toString("hh:mm:ss AP") + ")");
                    logicTimer->start(checkIntervalMs);
                }
            } else {
                statusLabel->setText("Error: Cannot reach Roku. Retrying in 60s...");
                logicTimer->start(checkIntervalMs);
            }
            reply->deleteLater();
        });
    };

    // Connect timer to the logic check
    QObject::connect(logicTimer, &QTimer::timeout, runLogicCheck);

    // Start Button
    QObject::connect(button, &QPushButton::clicked, [=]() {
        if (ipInput->text().trimmed().isEmpty()) {
            statusLabel->setText("Error: Enter IP first!");
            return;
        }
        button->setEnabled(false);
        ipInput->setEnabled(false);
        statusLabel->setText("Monitoring started...");
        runLogicCheck(); // Trigger first check immediately
    });

    window.show();
    return app.exec();
}
