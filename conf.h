#ifndef CONF_H
#define CONF_H

// stany w jakich moze byc klient
#define NO_ONE 1 // dostaje taki status gdy sie polaczyc z serwerem
#define PLAYER 2 // dostaje status gracza gdy dolacza do gry
#define CREATOR 3 // dostaje status tworcy gry kliknie przycisk tworzenia gry i nie ma w tym momencie gry/pokoju

// stany w jakich moze byc serwer
#define FREE 1 // nie ma gry ani zadnego pokoju do dolaczenia, mozna tylko stworzyc pokoj
#define INGAME 2 // w trakcie gry, nie mozna stworzyc pokoju ani sie nigdzie przylaczyc
#define ROOM 3 // jest stworzony pokoj, mozna sie tylko dolaczyc do tego pokoju pod warunkiem ze sie posiada kod
#define CREATING 4 // gracz jest podczas tworzenia gry, nie mozna dolaczac ani tworzyc pokoju

// typ wiadomosci
#define NICKFAILED 1
#define NICKSUCCESS 2
#define NEWNICKNAME 3
#define CREATEGAME 4
#define CREATEGAMESUCCESS 5
#define CREATEGAMEFAILED 6
#define JOINROOM 7
#define JOINROOMSUCCESS 8
#define JOINROOMFAILED 9
#define READYQUESTIONS 10
#define STARTGAME 11
#define SENDINGCODE 12
#define LISTOFPLAYERS 13
#define NEXTQUESTION 14
#define ANSWER 15
#define ENDOFGAME 16
#define ENFOFQUESTION 17
#define STATISTICS 18
#define SENDNUMOFANSWERS 19
#define HOSTLEFTTHEROOM 20

// ustawianie czasu
#define WAITTIMEFORCREATOR 60 // tyle czekamy aby tworca quizu nacisnal przycisk nastepne pytanie
#define WAITTIMEFORANSWERS 5 // czekamy na odpowiedzi dodatkowe 5 sekund

#endif // CONF_H
