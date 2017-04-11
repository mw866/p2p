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
    msg.insert("ChatText", message_text);

    // Origin : identifies the message’s original sender as a QString value;
    msg.insert("Origin", QString::number(mySocket->myPort));

    // msg.insert("Dest", destOrigin);
    // msg.insert("HopLimit",  hopLimit);

    // SeqNo: the monotonically increasing sequence number assigned by the original sender, as a quint32 value.
    msg.insert("SeqNo",  SeqNo);
    SeqNo += 1;

    // add messages to the message list
    if(messages_list.contains(QString::number(mySocket->myPort))) { // if this is not the first message
        messages_list[QString::number(mySocket->myPort)].insert(msg.value("SeqNo").toUInt(), msg);
    }
    else {
        QMap<quint32, QVariantMap> qvariantmap;
        messages_list.insert(QString::number(mySocket->myPort), qvariantmap);
        messages_list[QString::number(mySocket->myPort)].insert(msg.value("SeqNo").toUInt(), msg);
    }

    //serialize the message
    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << msg; 

    return datagram;
}

QByteArray ChatDialog::serializeStatus() {
    // QVariantMap statusMap;
    //         foreach (const QString &hostname, m_messageStatus->keys()) {
    //         statusMap.insert(hostname, m_messageStatus->value(hostname));
    //     }

    QMap<QString, QMap<QString, quint32> > statusMap;
    statusMap.insert("Want", want_list);

    //serialize status
    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << statusMap;

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
        qDebug () << "I am choosing random neighbor";
        srand(time(NULL));
        (rand() % 2 == 0) ?  neighbor = mySocket->myPort + 1: neighbor = mySocket->myPort - 1;
    }

    qDebug() <<  "Sending message to port " << QString::number(neighbor);

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


void ChatDialog::replyWithRumor(){
// TODO
}


void ChatDialog::rumorMongering(QVariantMap messageMap){

    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << messageMap;
    //send to a neighbor
    sendDatagrams(datagram);
    qDebug() << "Rumor has been mungored";
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
        qDebug() << "SUCCESS: Deserialized datagram to " << messageMap.value("ChatText");
    }
}


void ChatDialog::processIncomingDatagram(QVariantMap messageMap)
// Identify and triage incoming datagram
{
    qDebug() << "Inside processIncomingDatagram" << messageMap;
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
         processMessage(messageMap);
        // TODO Handle Chat message
    }
        else {
        qDebug() << "ERROR: Improperly formatted message";
        return;
    }
}

void ChatDialog::processMessage(QVariantMap messageMap){
    qDebug() << "Inside processMessage" << messageMap;
    
    quint32 origin = messageMap.value("Origin").toUInt();
    quint32 seqNo = messageMap.value("SeqNo").toUInt();

    if(mySocket->myPort != origin) {
        qDebug() << "Inside this if statement" << mySocket->myPort;
        qDebug() << "Want LIST:" << want_list;
        // origin seen before
        if(want_list.contains(QString::number(origin))) {
            qDebug() << "Inside this want_list if statement and seq number is: " << seqNo << want_list.value("Origin");
            //matches the sequences number
            if (seqNo == want_list.value(QString::number(origin))) {
                 qDebug() << "Inside this if statement with the matching seq number";
                 //This is a new message so add to message list
                 addToMessageList(messageMap, origin, seqNo);
            }
            //increment want list
            want_list[QString::number(origin)] = seqNo+1;
        }
        else { 
            qDebug() << "Inside this else statement";
            //first time message is coming from this origin so add to want list 
            want_list.insert(QString::number(origin), seqNo+1); // want the next message
            //also add to the message list
            addToMessageList(messageMap, origin, seqNo);
        }
    }
    
}

//past messages
void ChatDialog::addToMessageList(QVariantMap messageMap, quint32 origin, quint32 seqNo){
    //QString messages = messageMap.value("ChatText").toString();
    //ui->appendString(QString(origin + ": " + messages));
    qDebug() << " Inside addToMessageList";
    if(messages_list.contains(QString::number(origin))) {
        messages_list[QString::number(origin)].insert(seqNo, messageMap);
    }
    else {
        QMap<quint32, QMap<QString, QVariant> > qMap;
        messages_list.insert(QString::number(origin), qMap);
        messages_list[QString::number(origin)].insert(seqNo, messageMap);
    }
    //receiver will now see the sent message from the sender
    textview->append(QString::number(origin) + ": " + messageMap.value("ChatText").toString());

    rumorMongering(messageMap);

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
    
    // increment the want list before sending the datagram
    if(want_list.contains(QString::number(mySocket->myPort))) {
        want_list[QString::number(mySocket->myPort)]++;
    }
    else {
        want_list.insert(QString::number(mySocket->myPort), 1);
    }


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

