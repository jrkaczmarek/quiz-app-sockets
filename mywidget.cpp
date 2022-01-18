#include "mywidget.h"
#include "ui_mywidget.h"
#include "conf.h"
#include <QMessageBox>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <algorithm>

using namespace std;

MyWidget::MyWidget(QWidget *parent) : QWidget(parent), ui(new Ui::MyWidget) {
    ui->setupUi(this);

    connect(ui->conectBtn, &QPushButton::clicked, this, &MyWidget::connectBtnHit);
    connect(ui->hostLineEdit, &QLineEdit::returnPressed, ui->conectBtn, &QPushButton::click);

    connect(ui->nickBtn, &QPushButton::clicked, this, &MyWidget::nickBtnHit);
    connect(ui->nickLineEdit, &QLineEdit::returnPressed, ui->nickBtn, &QPushButton::click);

    connect(ui->creatqBtn, &QPushButton::clicked, this, &MyWidget::creatqBtnHit);

    connect(ui->answersSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MyWidget::enableAnswers);
    connect(ui->addQpushBtn, &QPushButton::clicked, this, &MyWidget::createQuestion);
    connect(ui->finishPushBtn, &QPushButton::clicked, this, &MyWidget::finishCreatingQuestions);

    connect(ui->joinqBtn, &QPushButton::clicked, this, &MyWidget::joinTheGame);
    connect(ui->joinqLineEdit, &QLineEdit::returnPressed, ui->joinqBtn, &QPushButton::click);

    connect(ui->startGameBtn, &QPushButton::clicked, this, &MyWidget::startGameBtn);

    connect(ui->sndAnsBtn, &QPushButton::clicked, this, &MyWidget::sendMyAnswer);

    connect(ui->loadNextQBtn, &QPushButton::clicked, this, &MyWidget::askForNextQuestion);

    connect(ui->endGameBtn, &QPushButton::clicked, this, &MyWidget::endGameHitBtn);

    msgBuffor = "";
    msgSize = -1;
    myStatus = NO_ONE;

    ui->QandAtextEdit->setReadOnly(true);
    ui->infoTextEdit->setReadOnly(true);
    ui->msgsTextEdit->setReadOnly(true);

    ui->stackedWidget->setCurrentIndex(0);
}

MyWidget::~MyWidget() {
    if(sock)
        sock->close();
    delete ui;
}

void MyWidget::timer() {
    timeToAnswer -= 1;
    ui->timeLabel->setText(QString::number(timeToAnswer));
    if(timeToAnswer == 0){
        if(timerForQuestion) {
            timerForQuestion->deleteLater();
            timerForQuestion=nullptr;
            ui->sndAnsBtn->setEnabled(false);
            ui->msgsTextEdit->append("<b>Your time for question is over</b>");
        }
    } else {
        ui->timeLabel->setText(QString::number(timeToAnswer));
    }
}

void MyWidget::startGameBtn() {
    // sending STARTGAME type message
    saveNumber(sizeof(uint16_t));
    saveNumber(STARTGAME);
}

void MyWidget::endGameHitBtn() {
    myStatus = NO_ONE;
    ui->stackedWidget->setCurrentIndex(0);
    ui->nickGroup->setEnabled(true);
    ui->typePlayerGroup->setEnabled(true);
    ui->rankingListWidget->clear();
    ui->playersListWidget->clear();
}

void MyWidget::finishCreatingQuestions() {
    int flag = createQuestion();
    if(!flag)
        return;

    // SIZEOFMSG TYP ILOSC PYTAN [PYTANIA]+
    // PYTANIE = DLUGOSC PYTANIA + PYTANIE + ILOSC_ODP + [DLUGOSC ODPOWIEDZ + ODPOWIEDZ]+ + dlugosc_poprawnej_odp + poprawna_odpowiedz + czas

    // obliczenie dlugosci wiadomosci
    uint16_t sizeofAllQuestionsAndAnswers = sizeof(uint16_t); // miejsce na typ wiadomosci
    sizeofAllQuestionsAndAnswers += sizeof(uint16_t); // ilosc pytan
    for(int i = 0; i < (int)questions.size(); i++) {
        sizeofAllQuestionsAndAnswers += sizeof(uint16_t); // zapisanie dlugosci pytania
        sizeofAllQuestionsAndAnswers += questions[i].content.size(); // zapisanie pytania
        sizeofAllQuestionsAndAnswers += sizeof(uint16_t); // zapisanie ilosc odpowiedzi
        for(int j = 0; j < (int)questions[i].answers.size(); j++) {
            sizeofAllQuestionsAndAnswers += sizeof(uint16_t); // dlugosc odpowiedzi
            sizeofAllQuestionsAndAnswers += questions[i].answers[j].size(); // odpowiedz
        }
        sizeofAllQuestionsAndAnswers += sizeof(uint16_t); // dlugoscp oprawnej odpowiedzi
        sizeofAllQuestionsAndAnswers += questions[i].correct_answer.size(); // poprawna odpowiedz
        sizeofAllQuestionsAndAnswers += sizeof(uint16_t); // czas odpowiedzi
    }

    saveNumber(sizeofAllQuestionsAndAnswers);
    saveNumber(READYQUESTIONS);
    saveNumber(questions.size());
    for(int i = 0; i < (int)questions.size(); i++) {
        saveString(questions[i].content);
        saveNumber(questions[i].answers.size());
        for(int j = 0; j < (int)questions[i].answers.size(); j++) {
            saveString(questions[i].answers[j]);
        }
        saveString(questions[i].correct_answer);
        saveNumber(questions[i].time);
    }

    ui->stackedWidget->setCurrentIndex(2);
    ui->startGameBtn->setEnabled(true);
}

int MyWidget::createQuestion() {
    // take question
    auto questionQ = ui->QuestionlineEdit->text().trimmed(); // get the name
    if(questionQ.isEmpty()) {
        ui->msgsTextEdit->append("<b>You didn't define a question!</b>");
        return 0; // do nothing if there is no question
    }
    string question = questionQ.toStdString();

    int amount = ui->answersSpinBox->value();
    // A take answer
    auto answerAQ = ui->answerA->text().trimmed();
    if(answerAQ.isEmpty()) {
        ui->msgsTextEdit->append("<b>You didn't define an answer A!</b>");
        return 0;
    }
    string answerA = answerAQ.toStdString();

    // B take answer
    auto answerBQ = ui->answerB->text().trimmed();
    if(answerBQ.isEmpty()) {
        ui->msgsTextEdit->append("<b>You didn't define an answer B!</b>");
        return 0;
    }
    string answerB = answerBQ.toStdString();

    // C take answer
    string answerC = "";
    if(amount >= 3) {
        auto answerCQ = ui->answerC->text().trimmed();
        if(answerCQ.isEmpty()) {
            ui->msgsTextEdit->append("<b>You didn't define an answer C!</b>");
            return 0;
        }
        answerC = answerCQ.toStdString();
    }

    // D take answer
    string answerD = "";
    if(amount == 4) {
        auto answerDQ = ui->answerD->text().trimmed();
        if(answerDQ.isEmpty()) {
            ui->msgsTextEdit->append("<b>You didn't define an answer D!</b>");
            return 0;
        }
        answerD = answerDQ.toStdString();
    }

    string correctAns;
    if(ui->answerAradioButton->isChecked())
        correctAns = "A";
    if(ui->answerBradioButton->isChecked())
        correctAns = "B";
    if(ui->answerCradioButton->isChecked())
        correctAns = "C";
    if(ui->answerDradioButton->isChecked())
        correctAns = "D";

    int time = ui->timeSpinBox->value();

    ui->msgsTextEdit->append("<b>Correctly defined question with answers!</b>");

    Question questionStruct;
    questionStruct.content = question;
    questionStruct.correct_answer = correctAns;
    questionStruct.answers.push_back(answerA);
    questionStruct.answers.push_back(answerB);
    if(amount >= 3)
        questionStruct.answers.push_back(answerC);
    if(amount == 4)
        questionStruct.answers.push_back(answerD);
    questionStruct.time = time;
    questions.push_back(questionStruct);

    ui->QuestionlineEdit->clear();
    ui->answerA->clear();
    ui->answerB->clear();
    ui->answerC->clear();
    ui->answerD->clear();
    ui->answerAradioButton->setChecked(true);
    ui->answersSpinBox->setValue(2);
    ui->timeSpinBox->setValue(20);

    return 1;
}

void MyWidget::enableAnswers() {
    int amount = ui->answersSpinBox->value();
    ui->answerAradioButton->setChecked(true);
    if(amount == 2) {
        ui->answerC->setEnabled(false);
        ui->answerD->setEnabled(false);
        ui->answerCradioButton->setEnabled(false);
        ui->answerDradioButton->setEnabled(false);
    } else if(amount == 3) {
        ui->answerC->setEnabled(true);
        ui->answerD->setEnabled(false);
        ui->answerCradioButton->setEnabled(true);
        ui->answerDradioButton->setEnabled(false);
    } else if(amount == 4) {
        ui->answerC->setEnabled(true);
        ui->answerD->setEnabled(true);
        ui->answerCradioButton->setEnabled(true);
        ui->answerDradioButton->setEnabled(true);
    }
}

void MyWidget::connectBtnHit(){
    ui->connectGroup->setEnabled(false);
    ui->nickGroup->setEnabled(false);
    ui->msgsTextEdit->append("<b>Connecting to " + ui->hostLineEdit->text() + ":" + QString::number(ui->portSpinBox->value())+"</b>");
    if(sock)
        delete sock;
    sock = new QTcpSocket(this);
    connTimeoutTimer = new QTimer(this);
    connTimeoutTimer->setSingleShot(true);
    connect(connTimeoutTimer, &QTimer::timeout, [&]{
        sock->abort();
        sock->deleteLater();
        sock = nullptr;
        connTimeoutTimer->deleteLater();
        connTimeoutTimer=nullptr;
        ui->connectGroup->setEnabled(true);
        ui->nickGroup->setEnabled(true);
        ui->msgsTextEdit->append("<b>Connect timed out</b>");
        QMessageBox::critical(this, "Error", "Connect timed out");
    });

    connect(sock, &QTcpSocket::connected, this, &MyWidget::socketConnected);
    connect(sock, &QTcpSocket::disconnected, this, &MyWidget::socketDisconnected);
    connect(sock, &QTcpSocket::errorOccurred, this, &MyWidget::socketError);
    connect(sock, &QTcpSocket::readyRead, this, &MyWidget::socketReadable);

    sock->connectToHost(ui->hostLineEdit->text(), ui->portSpinBox->value());
    connTimeoutTimer->start(3000);
}

void MyWidget::saveNumber(uint16_t number) {
    number = htons(number);
    sock->write((char*)&number, sizeof(uint16_t));
}

void MyWidget::saveString(string String) {
    uint16_t dataSize= htons((uint16_t)String.size());
    sock->write((char*)&dataSize, sizeof(uint16_t));
    sock->write(String.c_str(), String.size());
}

void MyWidget::nickBtnHit(){
    auto txt = ui->nickLineEdit->text().trimmed(); // get the name
    if(txt.isEmpty())
        return; // do nothing if there is no nickname

    string preparetosend = txt.toStdString();
    saveNumber(sizeof(uint16_t) + sizeof(uint16_t) + preparetosend.size());
    saveNumber(NEWNICKNAME);
    saveString(preparetosend);

    string msgOnTextEdit = "<b>Setting your nick to " + preparetosend + "<b>";
    ui->msgsTextEdit->append(QString::fromStdString(msgOnTextEdit));
    ui->nickLineEdit->clear();
    ui->nickLineEdit->setFocus();
}

void MyWidget::creatqBtnHit(){
    saveNumber(sizeof(uint16_t));
    saveNumber(CREATEGAME);
    string msgOnTextEdit = "<b>Creating game...<b>";
    ui->msgsTextEdit->append(QString::fromStdString(msgOnTextEdit));
}

void MyWidget::joinTheGame() {
    // take question
    auto codeQ = ui->joinqLineEdit->text().trimmed(); // get the code
    if(codeQ.isEmpty()) {
        ui->msgsTextEdit->append("<b>You didn't enter any code!</b>");
        return;
    }
    string code = codeQ.toStdString();

    //sending code to join the game
    saveNumber(sizeof(uint16_t) + sizeof(uint16_t) + code.size());
    saveNumber(JOINROOM);
    saveString(code);

    ui->msgsTextEdit->append("<b>You are trying to join the room!</b>");
}

void MyWidget::socketConnected(){
    connTimeoutTimer->stop();
    connTimeoutTimer->deleteLater();
    connTimeoutTimer=nullptr;
    ui->talkGroup->setEnabled(true);
    ui->nickGroup->setEnabled(true);
}

void MyWidget::socketDisconnected(){
    ui->msgsTextEdit->append("<b>Disconnected</b>");
    ui->talkGroup->setEnabled(false);
    ui->nickGroup->setEnabled(false);
    ui->connectGroup->setEnabled(true);
}

void MyWidget::socketError(QTcpSocket::SocketError err){
    if(err == QTcpSocket::RemoteHostClosedError)
        return;
    if(connTimeoutTimer){
        connTimeoutTimer->stop();
        connTimeoutTimer->deleteLater();
        connTimeoutTimer=nullptr;
    }
    QMessageBox::critical(this, "Error", sock->errorString());
    ui->msgsTextEdit->append("<b>Socket error: "+sock->errorString()+"</b>");
    ui->talkGroup->setEnabled(false);
    ui->nickGroup->setEnabled(false);
    ui->connectGroup->setEnabled(true);
}

uint16_t MyWidget::readNumberFromString(string & message) {
    uint16_t number = ntohs(*((uint16_t*)message.data()));
    message.erase(0, sizeof(uint16_t));
    return number;
}

string MyWidget::readStringFromString(string & message) {
    uint16_t length = readNumberFromString(message);
    string result = message.substr(0, length);
    message.erase(0, length);
    return result;
}

bool MyWidget::comparePlayers(const Player &a, const Player &b) {
    return a.points < b.points;
}


void MyWidget::updateListOfPlayers(string message){
    ui->msgsTextEdit->append("<b>Updating the list of players</b>");

    ui->playersListWidget->clear();
    ui->rankingListWidget->clear();

    // reading players
    players.clear();
    int numOfPlayers = readNumberFromString(message);
    for(int i = 0; i < numOfPlayers; i++){
        Player player {};
        player.nick = readStringFromString(message);
        player.points = readNumberFromString(message);
        players.push_back(player);
    }

    for(int i = 0; i < (int)players.size(); i++){
        ui->playersListWidget->addItem(QString::fromStdString(players[i].nick));
        ui->rankingListWidget->addItem(QString::number(i+1) + ". " + QString::fromStdString(players[i].nick) + ": " + QString::number(players[i].points) + " points"); // tutaj z punktami
    }
}

void MyWidget::loadNextQuestion(string message){
    // parsing the question :)
    questionNumber = readNumberFromString(message); // wczytanie numeru pytania
    string currentQuestion = readStringFromString(message); // wczytanie pytania
    int numOfAnswers = readNumberFromString(message); // wczytanie ilosc odpowiedzi
    vector<string> currentAnswers;
    for(int i = 0; i < numOfAnswers; i++) {
        currentAnswers.push_back(readStringFromString(message)); // wczytanie odpowiedzi
    }
    string correctAnswer = readStringFromString(message); // wczytanie poprawnej odpowedzi;
    correctAns = correctAnswer; // zapamietanie do zmiennej globalnej
    timeToAnswer = readNumberFromString(message); //wczytanie czasu na odpowedz

    // przygotowanie ekranu do odpowiadania
    ui->infoTextEdit->clear();
    ui->loadNextQBtn->setEnabled(false); // wylaczenie przycisku pojscia do nastepnego pytania
    ui->stackedWidget->setCurrentIndex(3); //wlaczenie ekranu dostosowanego do pytan
    ui->QandAtextEdit->clear(); // wczyszczenie ekranu do pytan i odpowiedzi;
    ui->QandAtextEdit->append(QString::fromStdString(currentQuestion)); // zaladowanie pytania na ekran
    for(int i = 0; i < numOfAnswers; i++) {
        string letterAns = string(1, char('A' + i));
        ui->QandAtextEdit->append(QString::fromStdString(letterAns) + ". " + QString::fromStdString(currentAnswers[i])); // zaladowanie odpowiedzi na ekran
    }

    ui->finalABtn->setChecked(true);
    ui->endGameBtn->setEnabled(false);
    if(myStatus == PLAYER) {
        ui->sndAnsBtn->setEnabled(true);
        ui->finalCBtn->setEnabled(false);
        ui->finalDBtn->setEnabled(false);

        ui->finalABtn->setEnabled(true);
        ui->finalBBtn->setEnabled(true);
        if(numOfAnswers >= 3)
            ui->finalCBtn->setEnabled(true);
        if(numOfAnswers == 4)
            ui->finalDBtn->setEnabled(true);
        ui->sndAnsBtn->setEnabled(true);
    } else if (myStatus == CREATOR){
        ui->finalABtn->setEnabled(false);
        ui->finalBBtn->setEnabled(false);
        ui->finalCBtn->setEnabled(false);
        ui->finalDBtn->setEnabled(false);
        ui->sndAnsBtn->setEnabled(false);
    }

    ui->timeLabel->setText(QString::number(timeToAnswer));
    timerForQuestion = new QTimer(this);
    connect(timerForQuestion, &QTimer::timeout, this , &MyWidget::timer);
    timerForQuestion->start(1000);

    return;
}

void MyWidget::sendMyAnswer(){
    // dlugosc wiadomosci | typ wiadomosci | numer pytania | dlugosc odpowiedzi | odpowiedz
    // sprawdzam co zaznaczyl gracz
    string Answer;
    if(ui->finalABtn->isChecked())
        Answer = "A";
    else if(ui->finalBBtn->isChecked())
        Answer = "B";
    else if(ui->finalCBtn->isChecked())
        Answer = "C";
    else if(ui->finalDBtn->isChecked())
        Answer = "D";

    ui->sndAnsBtn->setEnabled(false);
    ui->msgsTextEdit->append("<b>Sending your answer to server</b>");

    int msgSize = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + Answer.size(); // miejsce na: typ+numer_pytania+dlugosc_odpowiedzi+odpowiedz
    saveNumber(msgSize);
    saveNumber(ANSWER);
    saveNumber(questionNumber);
    saveString(Answer);
}

void MyWidget::gameIsOver(){
    ui->endGameBtn->setEnabled(true);
    ui->infoTextEdit->append("The end of game. Press end game button to exit.");
    ui->loadNextQBtn->setEnabled(false);
}

void MyWidget::QuestionOverFromServer(){
    // jezeli dostalismy od serwera odpowiedz o koncu czasu to wyswietlamy prawidlowa odpowiedz
    // dodatkowo odblokowujemy przycisk pojscia do nastepnego pytania dla osoby ktora stworzyla quiz
    ui->infoTextEdit->append("Correct answer is: " + QString::fromStdString(correctAns));
    if(myStatus == CREATOR)
        ui->loadNextQBtn->setEnabled(true);

    // usuwanie stopera jesli jeszcze istnieje
    if(timerForQuestion) {
        timerForQuestion->deleteLater();
        timerForQuestion=nullptr;
    }

    ui->sndAnsBtn->setEnabled(false);
}

void MyWidget::statisticsFromServer(string message){
    // to dostaje tylko tworca quizu, dostaje ta wiadomosc po koncu czasu od serwera o statystykach dotyczacych odpowiedzi
    ui->infoTextEdit->append("Odpowiedzi na pytania:");

    int sizeOfStats = readNumberFromString(message);
    for(int i = 0; i < sizeOfStats; i++){
        int stats = readNumberFromString(message);
        string letterAns = string(1, char('A' + i));
        ui->infoTextEdit->append(QString::fromStdString(letterAns) + ". " + QString::number(stats)); // zaladowanie odpowiedzi na ekran
    }
}

void MyWidget::receivedNumOfAnswers(string message)
{
    ui->msgsTextEdit->append("<b>Received num of answers</b>");
    int numberOfAnswers = readNumberFromString(message);
    ui->infoTextEdit->clear();
    ui->infoTextEdit->append("Number of answers: " + QString::number(numberOfAnswers));
}

void MyWidget::msgInterpreter(string message){
    //najpierw musimy uzyskac typ wiadomosci
    uint16_t type = readNumberFromString(message);
    string code;
    //interpretujemy wiadomosc
    switch(type){
        case NICKSUCCESS:
            ui->msgsTextEdit->append("<b>Your nick was accepted</b>");
            ui->nickGroup->setEnabled(false);
            ui->typePlayerGroup->setEnabled(true);
            break;
        case NICKFAILED:
            ui->msgsTextEdit->append("<b>Your nick was not accepted. Try again with another nick</b>");
            break;
        case CREATEGAMESUCCESS:
            ui->msgsTextEdit->append("<b>You can create the game</b>");
            ui->typePlayerGroup->setEnabled(false);
            ui->stackedWidget->setCurrentIndex(1);
            myStatus = CREATOR;
            questions.clear();
            break;
        case CREATEGAMEFAILED:
            ui->msgsTextEdit->append("<b>You can't create the game right now</b>");
            break;
        case JOINROOMSUCCESS:
            ui->msgsTextEdit->append("<b>You joined the room!</b>");
            ui->typePlayerGroup->setEnabled(false);
            ui->startGameBtn->setEnabled(false);
            ui->stackedWidget->setCurrentIndex(2);
            myStatus = PLAYER;
            break;
        case JOINROOMFAILED:
            ui->msgsTextEdit->append("<b>Failed to join the room</b>");
            break;
        case SENDINGCODE:
            code = readStringFromString(message);
            ui->msgsTextEdit->append("<b>Your code for game:</b>");
            ui->msgsTextEdit->append(QString::fromStdString(code));
            break;
        case LISTOFPLAYERS:
            updateListOfPlayers(message);
            break;
        case NEXTQUESTION:
            ui->msgsTextEdit->append("<b>Loading next question</b>");
            loadNextQuestion(message);
            break;
        case ENDOFGAME:
            ui->msgsTextEdit->append("<b>Game is over</b>");
            gameIsOver();
            break;
        case ENFOFQUESTION:
            ui->msgsTextEdit->append("<b>Signal from server that time is over!</b>");
            QuestionOverFromServer();
            break;
        case STATISTICS:
            ui->msgsTextEdit->append("<b>Got statistics from server!</b>");
            statisticsFromServer(message);
            break;
        case SENDNUMOFANSWERS:
            receivedNumOfAnswers(message);
            break;
        case HOSTLEFTTHEROOM:
            ui->msgsTextEdit->append("<b>Host has left the room!</b>");
            endGameHitBtn();
            break;
        default:
            ui->msgsTextEdit->append("<b>Got undefinied type of message</b>");
            ui->msgsTextEdit->append(QString::number(type));
            ui->msgsTextEdit->append(QString::number(message.size()));
            ui->msgsTextEdit->append(QString::fromStdString(message));
    }
}

void MyWidget::askForNextQuestion(){
    saveNumber(sizeof(uint16_t));
    saveNumber(NEXTQUESTION);
}

void MyWidget::socketReadable(){
    QByteArray ba = sock->readAll();
    string received = ba.toStdString();
    msgBuffor += received;

    if(msgSize == -1 && msgBuffor.size() >= sizeof(uint16_t)){ // jezeli nie mamy dlugosci wiadomosci to probujemy ja uzyskac
        string msgSizeString = msgBuffor.substr(0, sizeof(uint16_t));
        msgBuffor.erase(0, sizeof(uint16_t));
        msgSize = ntohs(*((uint16_t*)msgSizeString.data()));
    }
    else if(msgSize == -1 && msgBuffor.size() < sizeof(uint16_t)) { // nic jeszcze nie mozemy zrobic
        return;
    }

    //dojdziemy tu gdy mamy juz podana dlugosc wiadomosci do odczytania
    while((int)msgBuffor.size() >= msgSize && msgSize != -1) {
        string msgToInterpret = msgBuffor.substr(0, msgSize);
        msgBuffor.erase(0, msgSize);
        msgInterpreter(msgToInterpret);
        msgSize = -1;

        // mozemy jeszcze odczytac z bufora dlugosc nastepnej wiadomosci
        if(msgBuffor.size() >= sizeof(uint16_t)){
            string msgSizeString = msgBuffor.substr(0, sizeof(uint16_t));
            msgBuffor.erase(0, sizeof(uint16_t));
            msgSize = ntohs(*((uint16_t*)msgSizeString.data()));
        }
    }
}
