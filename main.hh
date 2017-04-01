#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>


class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	NetSocket();

	// Bind this socket to a P2Papp-specific default port.
	bool bind();

private:
	int myPortMin, myPortMax, myPort;
};

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();
	void sendDatagrams();
	NetSocket *mySocket;

public slots:
	void gotReturnPressed();

private:
	QTextEdit *textview;
	QLineEdit *textline;
};

#endif // P2PAPP_MAIN_HH
