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
    connect(textline, SIGNAL(returnPressed()),
        this, SLOT(gotReturnPressed()));

    // Register a callback on the textline's readyRead signal so that we can read messages.
    connect(mySocket, SIGNAL(readyRead()),
            this, SLOT(readPendingDatagrams()));


}
//Serialize message from QString to QByteArray
QByteArray ChatDialog::serializeMessage(QString message_text) {

    QVariantMap msg;
    //ChatText:  a QString containing user-entered text;
    msg.insert("ChatText", message_text);

    // Origin : identifies the message’s original sender as a QString value;
    msg.insert("Origin", QString::number(mySocket->myPort));

    // SeqNo: the monotonically increasing sequence number assigned by the original sender, as a quint32 value.
    msg.insert("SeqNo",  SeqNo);
    SeqNo += 1;

    // add messages to the message list everytime we receive a message
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
//Serialize message from QMAP to QByteArray
QByteArray ChatDialog::serializeStatus() {
    QMap<QString, QMap<QString, quint32> > statusMap;
    statusMap.insert("Want", want_list);

    //serialize status
    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << statusMap;

    return datagram;
}

// Send Chat Message to Neighbors 
void ChatDialog::sendDatagrams(QByteArray datagram) {
    // neighbor port
    int neighbor;
    //check if its the first port from the range of ports, if so send datagram to the port above
    if (mySocket->myPort == mySocket->myPortMin) {
        neighbor = mySocket->myPort + 1;
    //check if its the last port from the range of ports, if so send datagram to the port below
    } else if (mySocket->myPort == mySocket->myPortMax) {
        neighbor = mySocket->myPort - 1;
    } else {
        //check if its a middle port from the range of ports, if so send datagram to the random port above or below
        qDebug () << "I am choosing random neighbor";
        srand(time(NULL));
        (rand() % 2 == 0) ?  neighbor = mySocket->myPort + 1: neighbor = mySocket->myPort - 1;
    }

    qDebug() <<  "Sending message to port " << QString::number(neighbor);
    //send datagram
    mySocket->writeDatagram(datagram, datagram.size(), QHostAddress("127.0.0.1"), neighbor);
    // start timeout
    timtoutTimer->start(1000);
}

// Send Status Message back to the port you received your last message
void ChatDialog::sendStatus(QByteArray datagram)
{
    mySocket->writeDatagram(datagram.data(), datagram.size(), QHostAddress("127.0.0.1"), remotePort);
    timtoutTimer->start(1000);
}

//Rumor Monger by sending message to a neighboring port
void ChatDialog::rumorMongering(QVariantMap messageMap){
    //Serialize
    QByteArray datagram;
    QDataStream stream(&datagram,QIODevice::ReadWrite);
    stream << messageMap;
    //send to a neighbor
    sendDatagrams(datagram);
    qDebug() << "Rumor has been mungored";
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
        // process the data to see if its a status or a message
        processIncomingDatagram(datagram);
        //qDebug() << "SUCCESS: Deserialized datagram to " << messageMap.value("ChatText");
    }
}

// process the data received to see if its a status or a message
void ChatDialog::processIncomingDatagram(QByteArray datagram)
{
    //create message map of type QVariantMap which holdes the chat text
    QVariantMap messageMap;
    QDataStream serializer(&datagram, QIODevice::ReadOnly);
    serializer >> messageMap;
    if (serializer.status() != QDataStream::Ok) {
        qDebug() << "ERROR: Failed to deserialize datagram into QVariantMap";
        return;
    }
    qDebug() << "INFO: Inside processIncomingDatagram" << messageMap;
    //create status map of type QMap<QString, QMap<QString, quint32> > which holds the status
    QMap<QString, QMap<QString, quint32> > statusMap;
    QDataStream stream(&datagram, QIODevice::ReadOnly);
    stream >> statusMap;
    //check to see if the message map containts map or if it contains Chat Text
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
    qDebug() << "Inside processMessage" << messageMap;
    
    //initialize origin and seqNo to the origin value and seqNo value in message map
    quint32 origin = messageMap.value("Origin").toUInt();
    quint32 seqNo = messageMap.value("SeqNo").toUInt();

    //if the sender and receiver of the message are not the same
    if(mySocket->myPort != origin) {
        qDebug() << "Inside this if statement" << mySocket->myPort;
        qDebug() << "Want LIST:" << want_list;
        //check if we have seen messages from this origin before
        if(want_list.contains(QString::number(origin))) {
            qDebug() << "Inside this want_list if statement and seq number is: " << seqNo << want_list.value("Origin");
            //check if the received sequence number matches the wanted sequence number
            if (seqNo == want_list.value(QString::number(origin))) {
                 qDebug() << "Inside this if statement with the matching seq number";
                 //This is a new message so add to message list by calling addtoMessageList function
                 addToMessageList(messageMap, origin, seqNo);
            }
            //not sure if this is correct
            want_list[QString::number(origin)] = seqNo+1;
        }

        else { 
            qDebug() << "Inside this else statement";
            //first time message is coming from this origin so add to want list 
            want_list.insert(QString::number(origin), seqNo+1); // want the next message
            //also add to the message list by calling the function
            addToMessageList(messageMap, origin, seqNo);
        }
    }
    //sender is receiving his own message 
    else {
        qDebug() << "Sender and receiver are the same";
        if(want_list.contains(QString::number(origin))) {
            want_list[QString::number(origin)] = seqNo+1;
        }
        else {
            want_list.insert(QString::number(origin), seqNo+1); 
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
//    QMap<QString, quint32> localWants= want_list; // TODO To standardize the naming
    qDebug() << "INFO: Local WANTS: " << want_list;


    //===Decide the status===
    /* INSYNC: Local SeqNo is exactly same the remote SeqNos
     * AHEAD: Local SeqNo is greater than the remote SeqNo
     * BEHIND:
     * */

    enum Status { INSYNC = 1, AHEAD = 2 , BEHIND = 3 };
    Status status =  INSYNC;

    // In the local WANTS, iterate through all hosts(key)  and compare SeqNo(value) with remote WANTS
    QMap<QString, quint32>::const_iterator localIter = want_list.constBegin();

    qDebug() << "Remote Wants" << remoteWants << "\n Local Wants: "<< want_list;
    while (localIter != want_list.constEnd()){
        if(!remoteWants.contains(localIter.key())){
            // If the remote WANTS does NOT contain the local node
            qDebug() << "INFO: Local is AHEAD of remote; Remote does not have Local.";
            status = AHEAD;
            rumorMapToSend = messages_list[localIter.key()][quint32(0)];
        } else if(remoteWants[localIter.key()] < want_list[localIter.key()]) {
            qDebug() << "INFO: Local is AHEAD of remote; Remote has Local";
            status = AHEAD; // we are ahead, they are behind
            rumorMapToSend = messages_list[localIter.key()][remoteWants[localIter.key()]];
        }
        else if(remoteWants[localIter.key()] > want_list[localIter.key()]){
            qDebug() << "INFO: Local is BEHIND remote; Local has Remote.";
            status = BEHIND;
        }
        ++localIter;
    }

    // In the remote WANTS, iterate through all hosts(key) and compare SeqNo(value) with local WANTS
    QMap<QString, quint32>::const_iterator remoteIter = remoteWants.constBegin();
    while (remoteIter != remoteWants.constEnd()){
        if(!want_list.contains(remoteIter.key())) {
            qDebug() << "INFO: Local is BEHIND remote; Local does NOT have Remote.";
            status = BEHIND;
        }
        ++remoteIter;
    }
    timtoutTimer->stop();

    //serialize the rumor
    QByteArray rumorByteArray;
    QDataStream * stream = new QDataStream(&rumorByteArray, QIODevice::ReadWrite);
    (*stream) << rumorMapToSend;
    delete stream;

    // Act on the status
    qDebug() << QString("INFO: Act on Status#: " + QString::number(status));
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
            if(qrand() > .5*RAND_MAX) { // continue rumormongering
                rumorMongering(last_message);
            }
            break;
    }
}


//Slot value that gets called when message is sent
void ChatDialog::gotReturnPressed()
{
    // Just echo the string locally.
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

    //send datagram to neighbors
    sendDatagrams(message);
    // Clear the textline to get ready for the next input message.
    textline->clear();
}

//timeout handler that simply sends a last message to the last port the message tried sending to. 
void ChatDialog::timeoutHandler() {
    qDebug() << "INFO: Entered timeoutHandler.";
    QByteArray data;
    QDataStream stream(&data, QIODevice::ReadWrite);
    stream << last_message;
    mySocket->writeDatagram(data, data.size(), QHostAddress("127.0.0.1"), remotePort);
    // reset the timer to 1 second
    timtoutTimer->start(1000);
}

//antiEntropyHandler that sends a status to the neighboring port every 10 seconds
void ChatDialog::antiEntropyHandler(){
    qDebug() << "INFO: Entered Antientropy handler.";
    sendDatagrams(serializeStatus());
    // restart anti-entropy timer
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
            qDebug() << "bound to UDP port " << p;
            //store myPort number
            myPort = p;

           return true;
        }
    }
    qDebug() << "Oops, no ports in my default range " << myPortMin
        << "-" << myPortMax << " available";
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

