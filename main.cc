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

    //bind mySocket
    mySocket = new NetSocket();
    if (!mySocket->bind())
        exit(1);

    // initialize timeout timers
    timtoutTimer = new QTimer(this);
    connect(timtoutTimer, SIGNAL(timeout()), this, SLOT(timeoutHandler()));

    //initialize antientropy timer
    antientropyTimer = new QTimer(this);
    connect(antientropyTimer, SIGNAL(timeout()), this, SLOT(antiEntropyHandler()));
    antientropyTimer->start(10000);

    // tracking the receipts of messages status
    m_messageStatus = new QMap<QString, quint32>;

    // Initialize SeqNo
    SeqNo = 0;

    // Register a callback on the textline's returnPressed signal so that we can send the message entered by the user.
    connect(textline, SIGNAL(returnPressed()), this, SLOT(gotReturnPressed()));

    // Register a callback on the textline's readyRead signal so that we can read messages.
    connect(mySocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));


}
//Serialize message from QString to QByteArray
QByteArray ChatDialog::serializeMessage(QString message_text) {

    //msgQVariantMap: Intermediate serialized message
    QVariantMap msgQVariantMap;

    //ChatText:  a QString containing user-entered text;
    msgQVariantMap.insert("ChatText", message_text);

    // Origin : identifies the message’s original sender as a QString value;
    msgQVariantMap.insert("Origin", QString::number(mySocket->myPort));

    // SeqNo: the sequence number assigned by the original sender
    msgQVariantMap.insert("SeqNo",  SeqNo);
    SeqNo += 1;

    // add messages to the message list every time we receive a message
    if(messages_list.contains(QString::number(mySocket->myPort))) { // if this is not the first message
        messages_list[QString::number(mySocket->myPort)].insert(msgQVariantMap.value("SeqNo").toUInt(), msgQVariantMap);
    }
    else {
        // qvariantmap just to initialize the data structure
        QMap<quint32, QVariantMap> qvariantmap;
        messages_list.insert(QString::number(mySocket->myPort), qvariantmap);
        messages_list[QString::number(mySocket->myPort)].insert(msgQVariantMap.value("SeqNo").toUInt(), msgQVariantMap);
    }

    //serialize the message
    QByteArray msgByteArray;
    QDataStream stream(&msgByteArray,QIODevice::ReadWrite);
    stream << msgQVariantMap;

    return msgByteArray;
}

//Serialize status from QMap to QByteArray
QByteArray ChatDialog::serializeStatus() {
    QMap<QString, QMap<QString, quint32> > statusMap;
    statusMap.insert("Want", localWants);

    //serialize status
    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << statusMap;

    return datagram;
}

// Send Rumor to neighbors
void ChatDialog::sendDatagrams(QByteArray datagram) {

    // check if its the first port from the range of ports, if so send datagram to the port above
    if (mySocket->myPort == mySocket->myPortMin) {
        neighbor = mySocket->myPort + 1;
    // check if its the last port from the range of ports, if so send datagram to the port below
    } else if (mySocket->myPort == mySocket->myPortMax) {
        neighbor = mySocket->myPort - 1;
    } else {
        //check if its a middle port from the range of ports, if so send datagram to the random port above or below
        qDebug () << "INFO: Choosing a random neighbor";
        srand(time(NULL));
        (rand() % 2 == 0) ?  neighbor = mySocket->myPort + 1: neighbor = mySocket->myPort - 1;
    }

    qDebug() <<  "INFO: Sending message to port " << QString::number(neighbor);
    // send datagram
    mySocket->writeDatagram(datagram, datagram.size(), QHostAddress("127.0.0.1"), neighbor);
    // start timeout
    timtoutTimer->start(1000);
}

// Send Status  back to the port you received your last message
void ChatDialog::sendStatus(QByteArray datagram)
{
    mySocket->writeDatagram(datagram.data(), datagram.size(), QHostAddress("127.0.0.1"), remotePort);
    timtoutTimer->start(1000);
}

//Rumor Monger by sending message to a neighboring port
void ChatDialog::rumorMongering(QVariantMap messageMap){
    //Serialize rumor message
    QByteArray rumorBytes;
    QDataStream stream(&rumorBytes,QIODevice::ReadWrite);
    stream << messageMap;
    //send to a neighbor
    sendDatagrams(rumorBytes);
    qDebug() << "INFO: Rumor has been mungored.";
}


//slot function that gets executed when a new message has arrived
void ChatDialog::readPendingDatagrams()
{
    while (mySocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(mySocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        // receive and read message
        mySocket->readDatagram(datagram.data(), datagram.size(), &senderAddress,  &senderPort);
        //save the sender port number in a global variable
        remotePort = senderPort;
        processIncomingDatagram(datagram);
    }
}

// process the data received to see if its a status or a message
void ChatDialog::processIncomingDatagram(QByteArray incomingBytes)
{
    //create message map of type QVariantMap which holds the chat text
    QVariantMap messageMap;
    QDataStream serializer(&incomingBytes, QIODevice::ReadOnly);
    serializer >> messageMap;
    if (serializer.status() != QDataStream::Ok) {
        qDebug() << "ERROR: Failed to deserialize datagram into QVariantMap";
        return;
    }
    qDebug() << "INFO: Inside processIncomingDatagram" << messageMap;
    //create status map of type QMap<QString, QMap<QString, quint32> > which holds the status
    QMap<QString, QMap<QString, quint32> > statusMap;
    QDataStream stream(&incomingBytes, QIODevice::ReadOnly);
    stream >> statusMap;
    //check to see if the message map contains map or if it contains Chat Text
    if (messageMap.contains("Want")) {
        qDebug() << "INFO: Received  a Status Message";
        if (statusMap.isEmpty()) { // Also handles when "Want" key doesn't exist,
            // b/c nil.toMap() is empty;
            qDebug() << "ERROR: Received invalid or empty status map";
            return;
        }
        //process status if the message is status
        processStatus(statusMap);
    }  else if(messageMap.contains("ChatText")){
         qDebug() << "INFO: Received a Chat message";
         // process message if the message received is a chattext
         processMessage(messageMap);
    }  else {
        //It should ideally never come here
        qDebug() << "ERROR: Message is neither ChatText or Want";
        return;
    }
}

//if the message received is a chat text / rumor then check the message to see if the origin is new and send back the status and rumormonger 
void ChatDialog::processMessage(QVariantMap messageMap){
    qDebug() << "INFO: Inside processMessage()." << messageMap;
    
    //initialize origin and seqNo to the origin value and seqNo value in message map
    quint32 origin = messageMap.value("Origin").toUInt();
    quint32 seqNo = messageMap.value("SeqNo").toUInt();

    //if the sender and receiver of the message are not the same
    if(mySocket->myPort != origin) {
        //check if we have seen messages from this origin before
        if(localWants.contains(QString::number(origin))) {
            //check if the received sequence number matches the wanted sequence number
            if (seqNo == localWants.value(QString::number(origin))) {
                 //If this is a new message so add to message list by calling addtoMessageList function
                 addToMessageList(messageMap, origin, seqNo);
            }
            //not sure if this is correct
            localWants[QString::number(origin)] = seqNo + 1;
        }

        else { 
            //IF first time message is coming from this origin,  add to want list
            localWants.insert(QString::number(origin), seqNo+1); // want the next message
            //also add to the message list by calling the function
            addToMessageList(messageMap, origin, seqNo);
        }
    }
    else {
        //sender is receiving his own message
        qDebug() << "INFO: Sender and Receiver are the same.";
        if(localWants.contains(QString::number(origin))) {
            // if the origin is in the local want list
            localWants[QString::number(origin)] = seqNo + 1;
        }
        else {
            // if the origin is NOT in the local want list
            localWants.insert(QString::number(origin), seqNo+1);
        }
    }
    //stop the time out
    timtoutTimer->stop();
    //send the status back to the sender
    sendStatus(serializeStatus());
}

//add past messages to message list and rumor monger
void ChatDialog::addToMessageList(QVariantMap messageMap, quint32 origin, quint32 seqNo){
    qDebug() << "Inside addToMessageList";
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
    //perform rumor mongering
    rumorMongering(messageMap);
    //add the message that was sent to the global variable last_message
    last_message = messageMap;

}


void ChatDialog::processStatus(QMap<QString, QMap<QString, quint32> > receivedStatusMap)
{


    QMap<QString, QVariant> rumorMapToSend;
    QMap<QString, quint32> remoteWants = receivedStatusMap["Want"];

    qDebug() << "INFO: Remote WANTS: " << remoteWants;
    qDebug() << "INFO: Local WANTS: " << localWants;


    /* ===Decide the status===
     * INSYNC: Local SeqNo is exactly same the remote SeqNo.
     * AHEAD: Local SeqNo is greater than the remote SeqNo.
     * BEHIND: Local SeqNo is less than the remote SeqNo.
     */

    enum Status { INSYNC = 1, AHEAD = 2 , BEHIND = 3 };
    // Default status is INSYNC, if no mismatch found in the following steps.
    Status status =  INSYNC;

    // In the local WANTS, iterate through all hosts(key) and compare SeqNo(value) with remote WANTS
    QMap<QString, quint32>::const_iterator localIter = localWants.constBegin();

    while (localIter != localWants.constEnd()){
        if(!remoteWants.contains(localIter.key())){
            // If the remote WANTS does NOT contain the local node
            qDebug() << "INFO: Local is AHEAD of remote; Remote does not have Local.";
            status = AHEAD;
            rumorMapToSend = messages_list[localIter.key()][quint32(0)];
        } else if(remoteWants[localIter.key()] < localWants[localIter.key()]) {
            qDebug() << "INFO: Local is AHEAD of remote; Remote has Local";
            status = AHEAD; // we are ahead, they are behind
            rumorMapToSend = messages_list[localIter.key()][remoteWants[localIter.key()]];
        }
        else if(remoteWants[localIter.key()] > localWants[localIter.key()]){
            qDebug() << "INFO: Local is BEHIND remote; Local has Remote.";
            status = BEHIND;
        }
        ++localIter;
    }

    // In the remote WANTS, iterate through all hosts(key) and compare SeqNo(value) with local WANTS
    QMap<QString, quint32>::const_iterator remoteIter = remoteWants.constBegin();
    while (remoteIter != remoteWants.constEnd()){
        if(!localWants.contains(remoteIter.key())) {
            qDebug() << "INFO: Local is BEHIND remote; Local does NOT have Remote.";
            status = BEHIND;
        }
        ++remoteIter;
    }
    timtoutTimer->stop();

    // Serialize the rumor
    QByteArray rumorByteArray;
    QDataStream * stream = new QDataStream(&rumorByteArray, QIODevice::ReadWrite);
    (*stream) << rumorMapToSend;
    delete stream;

    // Act on the status
    qDebug() << "INFO: Act on Status#: " << QString::number(status);
    switch(status) {
        case AHEAD:
            qDebug() << "INFO: Local is AHEAD of the remote. Send new rumor." << rumorByteArray;
            mySocket->writeDatagram(rumorByteArray, QHostAddress::LocalHost, remotePort);
            qDebug() << QString("INFO: Sent datagram to port " + QString::number(remotePort));
            break;
        case BEHIND:
            qDebug() << "INFO: Local is BEHIND the remote. Reply with status.";
            sendStatus(serializeStatus());
            break;
        case INSYNC:
            qDebug() << "INFO: Local is IN SYNC with remote. Start Rumor Mongering.";
            if(qrand() > .5*RAND_MAX) {
                rumorMongering(last_message);
            }
            break;
    }
}


//Slot value that gets called when message is sent
void ChatDialog::gotReturnPressed()
{
    // Just echo the string locally.
    qDebug() << "INFO: Entered gotReturnPressed(). Message Sending: " << textline->text();
    textview->append(QString::number(mySocket->myPort) + ": " + textline->text());

    QString inputString = textline->text(); // get message text
    QByteArray message = serializeMessage(inputString); // serialize into bytestream
    
    // increment the want list before sending the datagram
    if(localWants.contains(QString::number(mySocket->myPort))) {
        localWants[QString::number(mySocket->myPort)] += 1;
    }
    else {
        localWants.insert(QString::number(mySocket->myPort), 1);
    }

    // Send datagram to neighbors
    sendDatagrams(message);
    // Clear the textline to get ready for the next input message.
    textline->clear();
}

//Timeout handler that simply sends a last message to the last port the message tried sending to.
void ChatDialog::timeoutHandler() {
    qDebug() << "INFO: Entered timeoutHandler().";
    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream << last_message;
    mySocket->writeDatagram(data, data.size(), QHostAddress("127.0.0.1"), neighbor);
    // reset the timer to 1 second
    timtoutTimer->start(1000);
}

//antiEntropyHandler that sends a status to the neighboring port every 10 seconds
void ChatDialog::antiEntropyHandler(){
    qDebug() << "INFO: Entered antiEntropyHandler().";
    sendDatagrams(serializeStatus());
    // Restart anti-entropy timer
    antientropyTimer->start(10000);
}

//constructing NetSocket Class
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
            qDebug() << "INFO: bound to UDP port " << p;
            //store myPort number
            myPort = p;

           return true;
        }
    }
    qDebug() << "ERROR: No ports avaialble in the default range " << myPortMin << "-" << myPortMax << " available";
    return false;
}



//main function
int main(int argc, char **argv)
{
    // Initialize Qt toolkit
    QApplication app(argc,argv);

    // Create an initial chat dialog window
    ChatDialog dialog;
    dialog.show();

    // Enter the Qt main loop; everything else is event driven
    return app.exec();
}

