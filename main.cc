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

QByteArray ChatDialog::serializeMessage(QString message_text) {

    QVariantMap msg;
    //ChatText:  a QString containing user-entered text;
    msg.insert("ChatText",message_text);

    // Origin : identifies the message’s original sender as a QString value;
    msg.insert("Origin", QString::number(mySocket->myPort));

    // msg.insert("Dest", destOrigin);
    // msg.insert("HopLimit",  hopLimit);

    // SeqNo: the monotonically increasing sequence number assigned by the original sender, as a quint32 value.
    msg.insert("SeqNo",  SeqNo);
    SeqNo += 1;

    //serialize the message
    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << msg; 

    return datagram;
}

QByteArray ChatDialog::serializeStatus() {
    QVariantMap statusMap;
            foreach (const QString &hostname, m_messageStatus->keys()) {
            statusMap.insert(hostname, m_messageStatus->value(hostname));
        }

    QVariantMap statusMapMsgMap;
    statusMapMsgMap.insert("Want", statusMap);

    //serialize status
    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << statusMapMsgMap;

    return datagram;
}

// Send Chat Message
void ChatDialog::sendDatagrams(QByteArray datagram) {
   
    int neighbor;
    if (mySocket->myPort == mySocket->myPortMin) {
        // TODO To check why +1 is not working
        neighbor = mySocket->myPort + 1;

    } else if (mySocket->myPort == mySocket->myPortMax) {
        neighbor = mySocket->myPort - 1;
    } else {
        (rand() % 2 == 0) ?  neighbor = mySocket->myPort + 1: neighbor = mySocket->myPort - 1;
    }

    mySocket->writeDatagram(datagram, datagram.size(), QHostAddress("127.0.0.1"), neighbor);
    //textview->append(textline->text());
}

// Send Status Message
void ChatDialog::sendStatus(QByteArray datagram)
{
    // TODO Change hardcoded destination portnumber
    mySocket->writeDatagram(datagram.data(), datagram.size(), QHostAddress("127.0.0.1"), 36770);
    textview->append(textline->text());
}


void ChatDialog::sendRumor(){
// TODO
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

        // TODO Handle rumor-mongoring and display if it's a new message processIncomingDatagram(sender, messageMap);
        processIncomingDatagram(messageMap);
        textview->append(messageMap.value("ChatText").toString());
        qDebug() << "SUCCESS: Deserialized datagram to " << messageMap.value("ChatText");
    }
}



void ChatDialog::processIncomingDatagram(QVariantMap& messageMap)
// Identify and triage incoming datagram
{
    if (messageMap.contains("Want")) {
        qDebug() << "INFO: Received  a Status Message";
        QMap<QString, QVariant> wants = messageMap.value("Want").toMap();
        if (wants.isEmpty()) { // Also handles when "Want" key doesn't exist,
            // b/c nil.toMap() is empty;
            qDebug() << "ERROR: Received invalid or empty status map";
            return;
        }

        processStatus(wants);
    }  else if(messageMap.contains("ChatText")){
         qDebug() << "INFO: Received a Chat message";
        // TODO Handle Chat message
    }
        else {
        qDebug() << "ERROR: Improperly formatted message";
        return;
    }
}



void ChatDialog::processStatus(QVariantMap& wants)
{

//     // Since message is well-formed, mark peer as having sent a message
//    quint16 oldVal = m_peerStatus->value(sender);
//    m_peerStatus->insert(sender, oldVal - 1);

//     QString hostname;
//     quint32 firstSelfUnreceived, firstSenderUnreceived;

//     int sentMessage = 0;
//     // LOG("Checking if I can gossip a message");
//     QMap<QString, quint32>::const_iterator i;
//     // See if server knows about an origin unknown to the peer
//     for (i = m_messageStatus->constBegin(); i != m_messageStatus->constEnd(); i++) {
//         hostname = i.key();
//         // NOTE: QVariantMap == QMap<QString, QVariant>, so don't need to convert
//         // keys to make comparison
//         if (!wants.contains(hostname) && !sentMessage) {
//             // qDebug() << "Sending message - hostname: " << hostname <<
//             //             ", seqNo: " << 1;
//             //sendChatMessage(sender, qMakePair(hostname, (quint32) 1));
//             qDebug() << "TODO: To resend message.",
//             sentMessage = 1;
//             break; // See if the want message contains hostnames I'm missing
//         }
//     }

//     // See if server has a more recent message for any mutually known origins
//     if (!sentMessage) {
//         for (i = m_messageStatus->constBegin(); i != m_messageStatus->constEnd(); i++) {
//             hostname = i.key();
//             firstSelfUnreceived = i.value();
//             firstSenderUnreceived = (quint32) wants.value(hostname).toUInt();
//             if (firstSenderUnreceived == 0) {
//                 qDebug() << "Malformed status from peer";
//                 return;
//             }
//             if (firstSelfUnreceived > firstSenderUnreceived) {
//                 // qDebug() << "Sending message - hostname: " << hostname <<
//                 //             ", seqNo: " << firstSenderUnreceived;
// //                sendChatMessage(sender, qMakePair(hostname, firstSenderUnreceived));
//                 qDebug() << "TODO: To resend message.";
//                 sentMessage = 1;
//                 break;
//             }
//         }
//    }

//     // LOG("Checking if I need a message");
//     // See if peer knows about an origin unknown to the server
//     // NOTE: Iterating over 'wants' map
//     QVariantMap::const_iterator j;
//     for (j = wants.constBegin(); j != wants.constEnd(); j++) {
//         hostname = j.key();
//         if (!m_messageStatus->contains(hostname)) {
//             if (!sentMessage) {
//                 sendStatus();
//                 sentMessage = 1;
//             }
//             // Also add that origin to my status so another node could potentially
//             // send me the message
//             m_messageStatus->insert(hostname, 1);
//         }
//     }
//     // See if peer has a more recent message for any mutually known origins
//     // NOTE: Iterating over 'status,' as above
//     if (!sentMessage) {
//         for (i = m_messageStatus->constBegin(); i != m_messageStatus->constEnd(); i++) {
//             hostname = i.key();
//             firstSelfUnreceived = i.value();
//             firstSenderUnreceived = (quint32) wants.value(hostname).toUInt();
//             if (firstSenderUnreceived == 0) {qDebug() << "Malformed status from peer"; return; };
//             if (firstSelfUnreceived < firstSenderUnreceived) {sendStatus(); break;
//             }
//         }
//     }

//     // Given both server and peer are in sync, randomly decide whether to
//     // rumor-monger with another peer
//     if (rand() % 2 == 0) { sendStatus(/*randomPeer()*/); return; };

}



void ChatDialog::gotReturnPressed()
{
    // Initially, just echo the string locally.
    qDebug() << "Message Sending: " << textline->text();
    textview->append(QString::number(mySocket->myPort) + ": " + textline->text());

    QString input_message = textline->text(); // get message text
    QByteArray message = serializeMessage(input_message); // serialize into bytestream

    sendDatagrams(message);
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
    // NetSocket sock;
    // if (!sock.bind())
    //     exit(1);

    // Enter the Qt main loop; everything else is event driven
    return app.exec();
}

