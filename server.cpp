#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <poll.h> 
#include <unordered_set>
#include <signal.h>
#include "conf.h"
#include <vector>
#include <string.h>
#include <string>
#include <iostream>
#include <map>
#include <unistd.h>
#include <ctime>
#include <sys/signalfd.h>


using namespace std;

struct MessageBuffor {
    string content;
    int length;
    string tmp_length;
};

struct Player {
    string nick; // nazwa gracza
    int status; // status gracza
    int points;
};

struct Question {
    string content; // tresc pytania
    vector <string> answers; // odpowiedzi
    string correct_answer; // poprawna odpowiedz
    int time; // czas
};

map <int, Player> players; // gracze
map <int, MessageBuffor> messageBuffors; // bufory na wiadomosci
vector <Question> questions; // baza pytan
vector <pollfd> descr; // deskryptory do funkcji poll
int stats[4]; // statystyki wysylane do tworcy gry po kazdym pytaniu

int servFd; // gniazdo serwera
int servStat = FREE; // status serwera
string roomCode = ""; // kod do gry
int questionNumber = -1; // numer biezacego pytania
int CreatorFd; // fd tworcy gry do szybkiego dostepu
int numOfAnswers; // ilosc odpowiedzi w biezacym pytaniu
int numOfPlayers = 0; // ilosc graczy w biezacym quizie
int alarmType;

// converts cstring to port
uint16_t readPort(char * txt){
    char * ptr;
    auto port = strtol(txt, &ptr, 10);
    if(*ptr!=0 || port<1 || (port>((1<<16)-1))) error(1,0,"illegal argument %s", txt);
    return port;
}

// sets SO_REUSEADDR
void setReuseAddr(int sock){
    const int one = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if(res) error(1,errno, "setsockopt failed");
}

// handles SIGINT
void ctrl_c(int){
    for(int i = 1 ; i < (int)descr.size(); ++i){
        shutdown(descr[i].fd, SHUT_RDWR);
        close(descr[i].fd);
    }
    close(servFd);
    printf("Closing server\n");
    exit(0);
}

void eventOnServFd(int revents) {
    if(revents & ~POLLIN){
        error(0, errno, "Event %x on server socket", revents);
        ctrl_c(SIGINT);
    }
    if(revents & POLLIN){
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);
        
        auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
        if(clientFd == -1) error(1, errno, "accept failed");
        
        pollfd newClient{.fd = clientFd, .events = POLLIN|POLLRDHUP, .revents=0};
        descr.push_back(newClient);

        Player newPlayer{.nick = "", .status = NO_ONE, .points = 0};
        players.insert({clientFd, newPlayer});

        MessageBuffor message{.content = "", .length=0, .tmp_length = ""};
        messageBuffors.insert({clientFd, message});

        printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
    }
}

uint16_t readNumberFromString(string & message) {
    uint16_t number = ntohs(*((uint16_t*)message.data()));
    message.erase(0, sizeof(uint16_t));
    return number;
}

string readStringFromString(string & message) {
    uint16_t length = readNumberFromString(message);
    string result = message.substr(0, length);
    message.erase(0, length);
    return result;
}

void saveNumber(int clientFd, uint16_t number) {
    number = htons(number);
    write(clientFd, &number, sizeof(uint16_t));
}

void saveString(int clientFd, string String) { 
    uint16_t dataSize= htons((uint16_t)String.size());
    write(clientFd ,&dataSize, sizeof(uint16_t));
    write(clientFd, String.c_str(), String.size());
}

string random_code() { // funkcja do generowania kodu
    int len = 9;
    char numbers[] = "0123456789";
    string code;
    for (int i = 0; i < len; ++i) {
        code += numbers[rand() % (sizeof(numbers) - 1)];
    }
    return code;
}

void loadNick(int clientFd, string message) {
    string nick = readStringFromString(message);
    // przegladanie czy sie powtarza nick
    map<int, Player>::iterator it;

    int flag = 1;
    for (it = players.begin(); it != players.end(); it++) {
        if(it->first == clientFd) {
            continue;
        }
        if(it->second.nick == nick) {
            flag = 0;
            break;
        }
    }

    if(flag){
        players[clientFd].nick = nick;
        printf("Loaded nick: %s\n", players[clientFd].nick.c_str());
        saveNumber(clientFd, sizeof(uint16_t));
        saveNumber(clientFd, (uint16_t)NICKSUCCESS);
    } else {
        printf("User with this nick already exists: %s\n", nick.c_str());
        saveNumber(clientFd, sizeof(uint16_t));
        saveNumber(clientFd, (uint16_t)NICKFAILED);
    }
}

void createGame(int clientFd) {
    cout << "Creating game..." << endl;
    if(servStat != FREE) {
        cout << "Cannot create the game" << endl;
        saveNumber(clientFd, sizeof(uint16_t)); // wyslanie dlugosci wiadomosci
        saveNumber(clientFd, (uint16_t)CREATEGAMEFAILED);
    }
    else {
        cout << "While creating..." << endl;
        servStat  = CREATING;
        saveNumber(clientFd, sizeof(uint16_t)); // wyslanie dlugosci wiadomosci
        saveNumber(clientFd, (uint16_t)CREATEGAMESUCCESS);
        players[clientFd].status = CREATOR; // ma status stworzyciela gry
        CreatorFd = clientFd;
    }
}

void loadQuestions(int clientFd, string message) {
    cout << "Loading questions..." << endl;
    questions.clear();
    uint16_t amountOfQ = readNumberFromString(message); // wczytanie ilosci pytan
    for(int i = 0; i < amountOfQ; i++) {
        Question question {};
        question.content = readStringFromString(message);
        uint16_t amountOfA = readNumberFromString(message); // wczytanie ilosci odpowiedzi
        for(int j = 0; j < amountOfA; j++) {
            question.answers.push_back(readStringFromString(message));
        }
        question.correct_answer = readStringFromString(message); // na samym koncu prawidlowa odpowiedz
        question.time = readNumberFromString(message);
        questions.push_back(question);
    }
    cout << "Questions loaded" << endl;

    for(int i = 0; i < (int)questions.size(); i++) {
        cout << "Question nr " << i+1 << ": " << questions[i].content << endl;
        for(int j = 0; j < (int)questions[i].answers.size(); j++) {
            cout << "Answer " << char('A'+j) << ": " << " " << questions[i].answers[j] << endl;
        }
        cout << "Correct answer: " << questions[i].correct_answer << endl;
        cout << "Time to answer: " << questions[i].time << endl;
    }
    servStat = ROOM;

    // generuj kod gry
    roomCode = random_code();
    cout << "Generated code for game: " << roomCode << endl;
    saveNumber(clientFd, sizeof(uint16_t) + sizeof(uint16_t) + roomCode.size());
    saveNumber(clientFd, SENDINGCODE);
    saveString(clientFd, roomCode);
}

void sendListOfPlayers(int clientFd) {
    // wiadomosc: dlugosc wiadomosci | typ wiadomosci | ilosc graczy | [dlugosc nicku gracza | nick gracza | ilosc punktow ] +

    map<int, Player>::iterator it;

    // wyliczanie dlugosci wiadomosci
    uint16_t msgSize = sizeof(uint16_t); // typ wiadomosci
    msgSize += sizeof(uint16_t); // ilosc graczy
    uint16_t numOfPlayers = 0;
    for (it = players.begin(); it != players.end(); it++) {
        if(it->second.status == PLAYER) {
            numOfPlayers += 1;
            msgSize += sizeof(uint16_t);// dlugosc nicku
            msgSize += it->second.nick.size(); // miejsce na nick
            msgSize += sizeof(uint16_t); // ilosc punktow
        }
    }

    // wysylanie
    saveNumber(clientFd, msgSize); // dlugosc wiadomosci
    saveNumber(clientFd, LISTOFPLAYERS); // typ wiadomosci
    saveNumber(clientFd, numOfPlayers); // ilosc graczy
    for (it = players.begin(); it != players.end(); it++) {
        if(it->second.status == PLAYER) {
            saveString(clientFd, it->second.nick);
            saveNumber(clientFd, it->second.points);
        }
    }
}

void joinRoom(int clientFd, string message) {
    string code = readStringFromString(message);
    if(servStat == ROOM && code == roomCode) {
        players[clientFd].status = PLAYER;
        printf("%s joined the room\n", players[clientFd].nick.c_str());
        saveNumber(clientFd, sizeof(uint16_t)); // wyslanie dlugosci wiadomosci
        saveNumber(clientFd, (uint16_t)JOINROOMSUCCESS);
        //update listy graczy
        map<int, Player>::iterator it;
        for (it = players.begin(); it != players.end(); it++) {
            if(it->second.status == PLAYER || it->second.status == CREATOR) {
                sendListOfPlayers(it->first);
            }
        }
        numOfPlayers += 1;
    } else {
        printf("%s failed to join the room\n", players[clientFd].nick.c_str());
        saveNumber(clientFd, sizeof(uint16_t)); // wyslanie dlugosci wiadomosci
        saveNumber(clientFd, (uint16_t)JOINROOMFAILED);
    }
}

void sendQuestion(int clientFd, int numberOfQuestion){
    numOfAnswers = 0; // statysytka dla serwera ktora wysylam potem do tworcy quizu by widzial ile osob odpowiedzialo, resetuje co pytanie

    // dlugosc wiadomosci | typ wiadomosci | numer pytania | dlugosc pytania | pytanie | ilosc odpowiedzi | [dlugosc odpowiedzi | odpowiedz ] + | dlugosc poprawnej odpowiedzi | poprawna odpowiedz
    uint16_t sizeofQuestionAndAnswers = sizeof(uint16_t); // miejsce na typ wiadomosci
    sizeofQuestionAndAnswers += sizeof(uint16_t); // numer pytania
    sizeofQuestionAndAnswers += sizeof(uint16_t); // zapisanie dlugosci pytania
    sizeofQuestionAndAnswers += questions[numberOfQuestion].content.size(); // zapisanie pytania
    sizeofQuestionAndAnswers += sizeof(uint16_t); // zapisanie ilosc odpowiedzi
    for(int j = 0; j < (int)questions[numberOfQuestion].answers.size(); j++) {
        sizeofQuestionAndAnswers += sizeof(uint16_t); // dlugosc odpowiedzi
        sizeofQuestionAndAnswers += questions[numberOfQuestion].answers[j].size(); // odpowiedz
    }
    sizeofQuestionAndAnswers += sizeof(uint16_t); // dlugoscp oprawnej odpowiedzi
    sizeofQuestionAndAnswers += questions[numberOfQuestion].correct_answer.size(); // poprawna odpowiedz
    sizeofQuestionAndAnswers += sizeof(uint16_t); // czas odpowiedzi

    saveNumber(clientFd, sizeofQuestionAndAnswers); // wyslanie dlugosc wiadomosci
    saveNumber(clientFd, NEXTQUESTION); // wyslanie typu wiadomosci
    saveNumber(clientFd, numberOfQuestion); // wyslanie numeru pytania
    cout << "Sending question nr " << numberOfQuestion << ": " << questions[numberOfQuestion].content << endl;
    saveString(clientFd, questions[numberOfQuestion].content); // wyslanie pytania z dlugoscia pytania
    cout << "Sending answers: " << questions[numberOfQuestion].answers.size() << endl;
    saveNumber(clientFd, questions[numberOfQuestion].answers.size()); // wyslanie ilosc pytan
    for(int j = 0; j < (int)questions[numberOfQuestion].answers.size(); j++) {
        cout << "Sending answer: " << questions[numberOfQuestion].answers[j] << endl;
        saveString(clientFd, questions[numberOfQuestion].answers[j]); // wyslanie odpowiedzi z dlugosci odpowiedzi
    }
    cout << "Sending correct answer: " << questions[numberOfQuestion].correct_answer << endl;
    saveString(clientFd, questions[numberOfQuestion].correct_answer); // wyslanie poprawnej odpowiedzi z dlugoscia poprawnej odpowiedzi
    cout << "Sending time to answer: " << questions[numberOfQuestion].time << endl;
    saveNumber(clientFd, questions[numberOfQuestion].time); // wyslanie czasu na odpowiedz
}

void restartQuiz() {
    numOfPlayers = 0;
    questions.clear();
    servStat = FREE;
    roomCode = "";
    questionNumber = -1;
    CreatorFd = -1;
    map<int, Player>::iterator it;
    for (it = players.begin(); it != players.end(); it++) {
        if(it->second.status == PLAYER || it->second.status == CREATOR) {
            it->second.points = 0;
            it->second.status = NO_ONE;
        }
    }
}

void nextQuestion(){
    alarm(0);
    alarmType = 1; // oznacza ze wyslalismy juz pytanie

    for(int i = 0; i < 4; i++)
        stats[i] = 0; // resetowanie statystyk;

    questionNumber += 1; // zaktualizowanie numeru aktualnego pytania

    int flagEndGame = 0;
    // najpierw musze sprawdzic czy juz nie zostaly wykorzystane wszystkie pytania
    if((int)questions.size() == questionNumber) { // przekroczylismy index ostaniego pytania
        flagEndGame = 1;
    }

    map<int, Player>::iterator it;
    for (it = players.begin(); it != players.end(); it++) {
        if(it->second.status == PLAYER || it->second.status == CREATOR) { // wysylam to do graczy i tworcy
            sendListOfPlayers(it->first); // wyslanie zaktualizowanego rankingu graczy
            if(flagEndGame) {
                // wysylam info o koncu gry
                saveNumber(it->first, sizeof(uint16_t));
                saveNumber(it->first, ENDOFGAME);
            } else {
                // wysylam info o nastepnym pytaniu
                sendQuestion(it->first, questionNumber); // wyslanie nastepnego pytania
            }
        }
    }

    if(flagEndGame){
        restartQuiz();
    }
    else {
        alarm(questions[questionNumber].time + WAITTIMEFORANSWERS); // czekanie na odpowiedzi (pod warunkiem ze sie juz nie skonczyla gra)
    }
}

void  ALARMhandler()
{
    // albo koniec czas lub 2/3 odpowiedzialo ALBO przez 60 sekund tworca gry nie otworzyl nastepnego pytania po zakonczeniu poprzedniego

    // zawsze gdy sie koncze pytanie to sie odpala pierwsza opcja alarmType = 1; po tym czekamy 60 sekund az tworca quizu odpowie
    if(alarmType) {
        cout << "Time for answering is over or at least 2/3 players answered" << endl;
        // wysylanie wiadomosci z koncem czasu na odpowiadanie do klientow i statystykami do tworcy quizu
        map<int, Player>::iterator it;
        for (it = players.begin(); it != players.end(); it++) {
            if(it->second.status == PLAYER || it->second.status == CREATOR) { // wysylam to do graczy i tworcy
                sendListOfPlayers(it->first); // wyslanie zaktualizowanego rankingu graczy
                saveNumber(it->first, sizeof(uint16_t));
                saveNumber(it->first, ENFOFQUESTION); // wyslanie info o koncu czasu na odpowiadanie
            }
            if(it->second.status == CREATOR){
                // wyslanie statystyk
                // rozmiar | typ wiadomosci | ile odpowiedzi | stat do odp A | stat do odp B i moze do C i D
                int numOfStats = questions[questionNumber].answers.size();
                int msgSize = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) * numOfStats; // typ wiadomosc, ile statystyk, statystyki
                saveNumber(it->first, msgSize);
                saveNumber(it->first, STATISTICS);
                saveNumber(it->first, numOfStats);
                for(int i = 0; i < numOfStats; i++)
                    saveNumber(it->first, stats[i]);
            }
        }

        alarmType = 0;
        alarm(WAITTIMEFORCREATOR);
    }
    else {
        // tworca quizu nie odpowiada dluzej niz 60 sekund
        nextQuestion();
    }

}

void processAnswer(int clientFd, string message) {
    cout << "Got an answer from: " << players[clientFd].nick << " -> ";
    int receivedNumQ = readNumberFromString(message); // odczyt numery pytania
    if(receivedNumQ == questionNumber && players[clientFd].status == PLAYER) { // sprawdzam czy wyslal odpowiedz na aktualne pytanie i czy to jeszcze gracz
        string receivedAns = readStringFromString(message);
        stats[receivedAns[0]-'A'] += 1; // zaktualizowanie statystyk
        cout << "Valid answer -> ";
        if(receivedAns == questions[questionNumber].correct_answer) { // sprawdzam czy poprawnie odpowiedzial
            players[clientFd].points += 1;
            cout << "Correct answer" << endl;
        } else {
            cout << "Not correct answer" << endl;
        }

        //wysylanie aktualnej ilosci odpowiedzi do osoby ktora stworzyla quiza
        numOfAnswers += 1;
        saveNumber(CreatorFd, sizeof(uint16_t) + sizeof(uint16_t)); // miejsce na typ wiadomosci i liczbe
        saveNumber(CreatorFd, SENDNUMOFANSWERS);
        saveNumber(CreatorFd, (uint16_t)numOfAnswers);

        //sprawdzam czy odpowiedzialo co najmnie 2/3 graczy, nie sprawdzam czy numOfPlayers != 0 bo kod jest wykonany tylko wtedy gdy dostane jakas odpowiedz od gracza czyli wtedy numOfPlayers > 0
        float ratioAnswer = (float)numOfAnswers / (float)numOfPlayers;
        if(ratioAnswer >= 2.0/3.0) {
            alarm(0);
            ALARMhandler();
        }
    } else {
        // jesli nie jest to gracz albo ma lagi i pozniej odpowiedzial to nic z nim nie robie
        cout << "Not valid answer." << "Not counting it" << endl;
    }
}

void startGame() {
    printf("Starting the game...\n");
    servStat = INGAME;
    questionNumber = -1;

    nextQuestion();
}

void messageInterpreter(int clientFd, string message) {
    //najpierw musimy uzyskac typ wiadomosci
    uint16_t type = readNumberFromString(message);
    cout << "Interpreting message..." << " " << "Type of message: " << type << endl;

    //interpretujemy wiadomosc
    switch(type){
        case NEWNICKNAME:
            loadNick(clientFd, message);
            break;
        case CREATEGAME:
            createGame(clientFd);
            break;
        case READYQUESTIONS:
            loadQuestions(clientFd, message);
            break;
        case JOINROOM:
            joinRoom(clientFd, message);
            break;
        case STARTGAME:
            startGame();
            break;
        case ANSWER:
            processAnswer(clientFd, message);
            break;
        case NEXTQUESTION:
            cout << "Sending next question" << endl;
            nextQuestion();
    }
}

void eventOnClientFd(int indexInDescr) {
    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;
    
    if(revents & POLLIN){
        // proba uzyskania dlugosci wiadomosci jesli jej nie mamy
        if(messageBuffors[clientFd].tmp_length.size() != sizeof(uint16_t)){
            int howManyCharsToRead = sizeof(uint16_t) - messageBuffors[clientFd].tmp_length.size();
            vector<char> Buf;
            Buf.resize(howManyCharsToRead,0x00);
            read(clientFd, &(Buf[0]), howManyCharsToRead);
            string tmp;
            tmp.assign(&(Buf[0]),Buf.size());
            messageBuffors[clientFd].tmp_length += tmp;
            if(messageBuffors[clientFd].tmp_length.size() == sizeof(uint16_t)){
                messageBuffors[clientFd].length = ntohs(*((uint16_t*)messageBuffors[clientFd].tmp_length.data()));
            }
        } else {
            // jesli mamy dlugosc wiadomosci to sprawdzamy ile mamy do przeczytania
            int howManyCharsToRead = messageBuffors[clientFd].length - messageBuffors[clientFd].content.size();
            vector<char> Buf;
            Buf.resize(howManyCharsToRead,0x00);
            read(clientFd, &(Buf[0]), howManyCharsToRead);
            string tmp;
            tmp.assign(&(Buf[0]),Buf.size());
            messageBuffors[clientFd].content += tmp;
            //jesli udalo nam sie przeczytac cala wiadomosc to interpretujemy ja i usuwamy z bufora
            if((int)messageBuffors[clientFd].content.size() == messageBuffors[clientFd].length) {
                messageInterpreter(clientFd, messageBuffors[clientFd].content);
                messageBuffors[clientFd].content = "";
                messageBuffors[clientFd].tmp_length = "";
                messageBuffors[clientFd].length = 0;
            }
        }
    }
    
    if(revents & ~POLLIN){
        int status = players[clientFd].status;

        printf("removing %d\n", clientFd);
        descr.erase(descr.begin() + indexInDescr);
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
        players.erase(players.find(clientFd));
        messageBuffors.erase(messageBuffors.find(clientFd));

        // aktualizujemy pokoj jesli to byl gracz
        if((servStat == ROOM || servStat == INGAME) && status == PLAYER) {
            map<int, Player>::iterator it;
            for (it = players.begin(); it != players.end(); it++) {
                if(it->second.status == PLAYER || it->second.status == CREATOR) { // wysylam to do graczy i tworcy
                    sendListOfPlayers(it->first); // wyslanie zaktualizowanego rankingu graczy
                }
            }
        }
        if(servStat == ROOM && status == PLAYER) {
            numOfPlayers -= 1; // numofplayers jest uzywany do sprawdzenia 2/3 odpowiedzialo co najmniej, jezeli w czasie gry rozlaczy kogos to i tak pod uwage bierzemy poczatkowa ilosc graczy
        }

        if(status == CREATOR && servStat == CREATING) {
            restartQuiz();
        }

        if(status == CREATOR && servStat == ROOM) {
            map<int, Player>::iterator it;
            for (it = players.begin(); it != players.end(); it++) {
                if(it->second.status == PLAYER || it->second.status == CREATOR) { // wysylam to do graczy i tworcy
                    saveNumber(it->first, sizeof(uint16_t));
                    saveNumber(it->first, HOSTLEFTTHEROOM);
                }
            }
            restartQuiz();
        }
    }
}

int main(int argc, char ** argv){
    srand(time(0));
    if(argc != 2) error(1, 0, "Need 1 arg (port)");
    auto port = readPort(argv[1]);
    
    servFd = socket(AF_INET, SOCK_STREAM, 0);
    if(servFd == -1) error(1, errno, "socket failed");
    
    signal(SIGINT, ctrl_c);
    signal(SIGPIPE, SIG_IGN);
    
    setReuseAddr(servFd);

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((short)port);
    serverAddr.sin_addr = {htonl(INADDR_ANY)};

    if(bind(servFd, (sockaddr*) &serverAddr, sizeof(serverAddr)))
        error(1, errno, "bind failed");
    
    if(listen(servFd, 1))
        error(1, errno, "listen failed");
    
    pollfd serv{.fd = servFd, .events = POLLIN, .revents=0};
    descr.push_back(serv);

    // handling alarm in poll
    int signal_fd;
    sigset_t sigset;
    struct signalfd_siginfo siginfo;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_SETMASK, &sigset, NULL);
    signal_fd = signalfd(-1, &sigset, 0);
    pollfd sign{.fd = signal_fd, .events = POLLIN, .revents=0};
    descr.push_back(sign);
    
    while(true){
        int ready = poll(&descr[0], descr.size(), -1);
        if(ready == -1){
            error(0, errno, "poll failed");
            ctrl_c(SIGINT);
        }
        
        for(int i = 0 ; i < (int)descr.size() && ready > 0 ; ++i){
            if(descr[i].revents){
                if(descr[i].fd == servFd){
                    eventOnServFd(descr[i].revents);
                }
                else if(descr[i].fd == signal_fd)  {
                    read(signal_fd, &siginfo, sizeof(siginfo));
                    ALARMhandler();
                }
                else {
                    eventOnClientFd(i);
                }
                ready--;
            }
        }
    }
}


