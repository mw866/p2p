#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <unistd.h>
#include <QDataStream>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>
#include <QHostInfo>
#include <QtGlobal>
#include <QTimer>


class NetSocket : public QUdpSocket
{
    Q_OBJECT

    public:
        NetSocket();

        // Bind this socket to a P2Papp-specific default port.
        bool bind();
        void restartTimer();
        QTimer * timer;

public:
        int myPortMin, myPortMax, myPort;

};

class ChatDialog : public QDialog
{
    Q_OBJECT

    public:
        ChatDialog();
        void sendDatagrams(QByteArray);
        NetSocket *mySocket;
        int SeqNo;
        QMap<QString, quint32> want_list;

        QMap<QString, QMap<quint32, QVariantMap> > messages_list;

        public slots:
                void gotReturnPressed();
                void readPendingDatagrams();

    private:
        QTextEdit *textview;
        QLineEdit *textline;
        QMap<QString, quint32>* m_messageStatus;
        void processIncomingDatagram(QVariantMap& messageMap);
        void processStatus(QVariantMap& wants);
        void processMessage(QVariantMap& wants);
        void sendStatus(QByteArray);
        void replyWithRumor();
        void rumorMongering();
        QByteArray serializeMessage(QString);
        QByteArray serializeStatus();
};

#endif // P2PAPP_MAIN_HH
