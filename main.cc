#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("P2Papp");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);
	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally, leaving extra vertical space for the textview widget.
	// You might change this into a read/write QTextEdit, so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:  http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	mySocket = new NetSocket();
    if (!mySocket->bind())
        exit(1);

    // tracking the receipts of messages status
    m_messageStatus = new QMap<QString, quint32>;

    // Initialize SeqNo
    SeqNo = 1;

	// Register a callback on the textline's returnPressed signal so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));

    connect(mySocket, SIGNAL(readyRead()),
            this, SLOT(readPendingDatagrams()));


}

void ChatDialog::sendDatagrams()
{
    // msg: the Rumor message, the key-value pairs of a user message to be gossipped.
    QVariantMap msg;
    //ChatText:  a QString containing user-entered text;
    msg.insert("ChatText",textline->text());

    // Origin : identifies the message’s original sender as a QString value;
    msg.insert("Origin", QHostInfo::localHostName());

    // msg.insert("Dest", destOrigin);
    // msg.insert("HopLimit",  hopLimit);


    // SeqNo: the monotonically increasing sequence number assigned by the original sender, as a quint32 value.
    SeqNo += 1;
    msg.insert("SeqNo",  SeqNo);


    QVariantMap statusMap;
            foreach (const QString &hostname, m_messageStatus->keys()) {
            statusMap.insert(hostname, m_messageStatus->value(hostname));
        }

    msg.insert("Want", statusMap);


    // Sending the Rumor message

    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << msg;

    //    sendDatagram(destination, messageMap);
    // TODO Change hardcoded destination portnumber
    mySocket->writeDatagram(datagram.data(), datagram.size(), QHostAddress("127.0.0.1"), 36770);

    textview->append(textline->text());
}



void ChatDialog::readPendingDatagrams()
{
    while (mySocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(mySocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        // TODO Change hardcoded destination portnumber
        mySocket->readDatagram(datagram.data(), datagram.size(), &senderAddress,  &senderPort);

        //QPeer sender(senderAddress, senderPort);
        // FIXME: Adding self as a peer accidentally
        //        addPeer(sender);

        QVariantMap messageMap;
        QDataStream serializer(&datagram, QIODevice::ReadOnly);
        serializer >> messageMap;
        if (serializer.status() != QDataStream::Ok) {
			qDebug() << "ERROR: Failed to deserialize datagram into QVariantMap";
            return;
        }

        // Handle rumor-mongoring and display if it's a new message processIncomingDatagram(sender, messageMap);

        textview->append(messageMap.value("ChatText").toString());
        qDebug() << "SUCCESS: Deserialized datagram to " << messageMap.value("ChatText");
    }
}



void ChatDialog::gotReturnPressed()
{
	// Initially, just echo the string locally.
	qDebug() << "FIX: send message to other peers: " << textline->text();
	sendDatagrams();
	// Clear the textline to get ready for the next input message.
	textline->clear();
}

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default, computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user to find each other on the same host, barring UDP port conflicts with other applications (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
			myPort = p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
	NetSocket sock;
	if (!sock.bind())
		exit(1);

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}

