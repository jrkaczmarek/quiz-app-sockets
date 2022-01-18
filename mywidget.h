#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QWidget>
#include <QTcpSocket>
#include <QTimer>
#include <string>

using namespace std;

struct Question {
    string content; // tresc pytania
    vector <string> answers; // odpowiedzi
    string correct_answer; // poprawna odpowiedz
    int time; // czas
};

struct Player {
    string nick;
    int points;
};

namespace Ui {
class MyWidget;
}

class MyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MyWidget(QWidget *parent = 0);
    ~MyWidget();

protected:
    QTcpSocket * sock {nullptr};
    QTimer * connTimeoutTimer{nullptr};
    string msgBuffor;
    int msgSize;
    vector <Question> questions; // baza pytan
    vector <Player> players; // baza graczy z punktami (do wyswietlania rankingu)
    int myStatus; // status gracza
    int questionNumber; // numer aktualnego pytania na ktore gracz odpowiada
    string correctAns; // poprawna odpowiedz na aktualne pytanie - wyswietla sie po zakonczeniu czasu na odpowiadanie
    int timeToAnswer;
    QTimer * timerForQuestion{nullptr};

    void connectBtnHit();
    void socketConnected();
    void socketDisconnected();
    void socketError(QTcpSocket::SocketError);
    void socketReadable();
    void nickBtnHit();
    void saveNumber(uint16_t number);
    void saveString(string String);
    void msgInterpreter(string String);
    void creatqBtnHit();
    uint16_t readNumberFromString(string & message);
    string readStringFromString(string & message);
    void enableAnswers();
    int createQuestion();
    void finishCreatingQuestions();
    void joinTheGame();
    void startGameBtn();
    void updateListOfPlayers(string);
    bool comparePlayers(const Player &a, const Player &b);
    void loadNextQuestion(string);
    void sendMyAnswer();
    void askForNextQuestion();
    void gameIsOver();
    void QuestionOverFromServer();
    void statisticsFromServer(string);
    void receivedNumOfAnswers(string);
    void endGameHitBtn();
    void timer();

private:
    Ui::MyWidget * ui;


};

#endif // MYWIDGET_H
