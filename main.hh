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

        public slots:
                void gotReturnPressed();
                void readPendingDatagrams();

    private:
        QTextEdit *textview;
        QLineEdit *textline;
        QMap<QString, quint32>* m_messageStatus;
        void processIncomingDatagram(QVariantMap& messageMap);
        void processStatus(QVariantMap& wants);
        void sendStatus(QByteArray);
        void sendRumor();
        QByteArray serializeMessage(QString);
        QByteArray serializeStatus();
};

#endif // P2PAPP_MAIN_HH
