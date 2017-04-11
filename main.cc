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


void ChatDialog::replyWithRumor(){
// TODO
}


void ChatDialog::rumorMongering(){
//TODO
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
         processMessage(messageMap);
        // TODO Handle Chat message
    }
        else {
        qDebug() << "ERROR: Improperly formatted message";
        return;
    }
}

void ChatDialog::processMessage(QVariantMap& messageMap){

}



void ChatDialog::processStatus(QMap<QString, QMap<QString, quint32> > receivedStatusMap receivedStatusMap)
{


    QMap<QString, QVariant> rumorToSend;
    QMap remoteWants = receivedStatusMap["Want"];

    qDebug() << "INFO: Remote WANTS: " << remoteWants;
    QMap localWants; // TODO Temp; localwants to be defined in globally instead
    qDebug() << "INFO: Local WANTS: " << localWants;


    //===Decide the status===
    /* INSYNC: Local SeqNo is exactly same the remote SeqNos
     * AHEAD: Local SeqNo is greater than the remote SeqNo
     * BEHIND:
     * */

    enum Status { INSYNC, AHEAD, BEHIND };
    Status status =  INSYNC;

    // In the local WANTS, iterate through all hosts(key)  and compare SeqNo(value) with remote WANTS
    QMap::const_iterator localIter = localWants.constBegin();

    while (localIter ){
        if(!remoteWants.contains(localIter.key())){
            // If the remote WANTS does NOT contain the local node
            qDebug() << "INFO: Local is AHEAD of remote; Remote does not have Local.";
            status = AHEAD;
            rumorToSend = messageMap[localIter.key()][quint32(0)];
        } else if(remoteWants[localIter.key()] <= localWants[localIter.key()]) {
            qDebug() << "INFO: Local is AHEAD of remote; Remote has Local";
            status = AHEAD; // we are ahead, they are behind
            QMap messageMap; // TODO Temp; messageMap to be defined globally;
            rumorToSend = messageMap[localIter.key()][remoteWants[localIter.key()]];
        }
        else {
            qDebug() << "INFO: Local is BEHIND remote; Local has Remote.";
            status = BEHIND;
        }
    }

    // In the remote WANTS, iterate through all hosts(key) and compare SeqNo(value) with local WANTS
    QMap::const_iterator remoteIter = remoteWants.constBegin();
    while (remoteIter != remoteWants.constEnd()){
        if(!localWants.contains(remoteIter.key())) {
            qDebug() << "INFO: Local is BEHIND remote; Local does NOT have Remote.";
            status = BEHIND;
        }
    }

    // ===Act on the status===
    switch(status) {
        case AHEAD:
            qDebug() << "INFO: Local is AHEAD of the remote. Do nothing";
            break;
        case BEHIND:
            qDebug() << "INFO: Local is BEHIND the remote. Reply with status";
            QByteArray statusBytes = serializeMessage(messages_list);
            sendStatus(statusBytes);
            break;
        case INSYNC:
            qDebug() << "INFO: Local is IN SYNC with remote. Start Rumor Mongering.";
            if(qrand() > .5*RAND_MAX) { // continue rumormongering
                QByteArray rumorBytes = serializeMessage(rumorToSend);
                rumorMongering(rumorBytes); //TODO
                mySocket->restartTimer();
            }
            break;
    }
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



            // initialize timers
            timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(timeoutHandler()));

            return true;
        }
    }

    qDebug() << "Oops, no ports in my default range " << myPortMin
        << "-" << myPortMax << " available";
    return false;
}

void NetSocket::restartTimer() {
    timer->start(1000);
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

