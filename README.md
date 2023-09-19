# Aplicatie client-server TCP si UDP pentru gestionarea mesajelor

Pentru aceasta tema, am folosit ca schelet laboratorul 7.

### subscriber.c
* Mod de pornire: ./subscriber <ID> <ip> <port>
* Am modificat fisierul client.c de la laborator, modificand cerintele
asupra argumentelor si incepand totodata si cu dezactivarea algoritmului
Nagle. (TCP_NODELAY)
* Aici trimit si serverului ID-ul subscriberului, iar apoi se apeleza functia
run_client. In run_client, se face multiplexarea intre citirea de la tastatura
si primirea unui mesaj cu ajutorul pollfd. Daca se citeste de la tastatura,
vom trimite serverului pachetul si daca comanda data a fost exit, subscriberul
se va opri. ALtfel, daca primim mesaj, vom verifica daca este exit de la
server, caz in care se va inchide si subscriberul si vom printa mesajul sosit.

### server.c
* Mod de pornire: ./server <port>
* Baza este fisierul server.c din laborator 7.
* In run_chat_multi_server incepem prin a declara structura pollfd, pentru
multiplexare. Din start avem 3 socketi: unul tcp, unul udp si STDIN_FILENO
pentru citirea de la tastatura.
* Daca socketul este tcp_listenfd, a venit o cerere de conexiunea noua.
Dezactivam algoritmul Nagle, adaugam noul socket la multimea poll_fds,
primim ID-ul noului subscriber si 
* Daca socketul este udp_listenfd, se primeste un mesaj si trebuie sa il
parsam. Am luat un buffer alocat static in care primesc mesajul, si send in
care voi formata mesajul catre client.  Primii 50 de biti vor fi topicul,
pentru care am creat o structura ce contine numele si sf-ul. Apoi verificam
tipul, in functie de acesta vom formata mesajul potrivit. La final trimitem
tuturor clientilor abonati mesajul.
* Daca socketul este STDIN_FILENO, primim mesajul si verificam daca este exit.
Daca da, se vor inchide toate conexiunile si se va trimite mesaj cu "exit"
pentru clienti pentru a ii inchide si pe ei. Altfel, se afiseaza un mesaj
care precizeaza ca poti trimite doar comanda exit.
* Ultimul caz de else este pentru datele trimise de unul din subscriberi. 
Sunt 3 tipuri de comenzi valabile.
* Daca se primeste comanda subscribe, extrag topicul si sf-ul, daca topicul
nu exista, il adaug in vectorul de topicuri specific subscriberului si trimit
mesajul "Subscribed to topic.", altfel daca exista trimit mesajul
"You're already subscribed to this."
* Daca se primeste comanda unsubscribe, extrag topicul, aflu indexul
subscriberului si al topicului si in caz ca nu este abonat oricum, afisez un
mesaj corespunzator, altfel scot din vectorul de topics topicul respectiv si
trimit mesajul "Unsubscribed from topic." pentru a putea fi afisat in
subscriber.
* Daca se primeste comanda exit, aflu subscriberul, afisez mesajul
corespunzator, marchez campul online cu 0, inchid socketul si il scot din
multimea pollfd, scazand si numarul de clienti activi.
* Mentionez ca am dezactivat si algoritmul Nagle si am folosit si sugestia
cu setvbuf din enuntul temei.

### helpers.h
* Structura topic contine numele si sf-ul.
* Structura de subscriber care contine id, socket, campul online care verifica
daca este conectat sau nu, vectorul de topics si num_topics care retine 
numarul de topicuri la care subscriberul este abonat.

### common.c
* Contine functiile implementate la laborator.
* Cat timp nu s-au primit/trimis toti bitii, se da send si se adauga cei cititi
in campul bytes_sent/bytes_received. Daca nu mai e nimic de citit, returnam
numarul de biti.


In cazul de same_id daca este vorba de primul subscriber nu merge cum e in checker, dar
pentru mai multi merge. (daca sunt 2 conectati sau au fost)
