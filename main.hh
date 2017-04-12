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
#include <ctime>
#include <cstdlib>


class NetSocket : public QUdpSocket
{
    Q_OBJECT

    public:
        NetSocket();

        // Bind this socket to a P2Papp-specific default port.
        bool bind();

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
        int remotePort;
        QMap<QString, quint32> want_list;
        QVariantMap last_message;

        QMap<QString, QMap<quint32, QVariantMap> > messages_list;
        QTimer * timtoutTimer;
        QTimer * antientropyTimer;


    public slots:
        void gotReturnPressed();
        void readPendingDatagrams();
        void timeoutHandler();
        void antiEntropyHandler();

    private:
        QTextEdit *textview;
        QLineEdit *textline;
        QMap<QString, quint32>* m_messageStatus;
        void processIncomingDatagram(QByteArray datagram);
        void processStatus(QMap<QString, QMap<QString, quint32> > wants);
        void processMessage(QVariantMap wants);
        void sendStatus(QByteArray);
        void replyWithRumor();
        void rumorMongering(QVariantMap messageMap);
        void addToMessageList(QVariantMap messageMap, quint32 origin, quint32 seqNo);
        QByteArray serializeMessage(QString);
        QByteArray serializeStatus();

};

#endif // P2PAPP_MAIN_HH
