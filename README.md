Clientul trimite serverului:
-list
-update <filename> <offset> <string>
-get <filename>
-put <filename>
-bye //inchide conexiunea
-search <string>  //returneaza o lista de fisiere in care s-a gasit stringul sau EROARE

Am folosit 2 protocoale de comunicare diferite: 
receive_basic() si receive_basic2()
receive_basic2() este folosit pentru transmiterea listelor cu nume de fisiere
separate cu '\0' (folosit de list si search).

receive_basic() protocol comunicare:
<int size of string>-<string>

receive_basic2() protocol comunicare:
<status>\0<int total size>\0<nume1>\0<nume2>...

Serverul are 2 thread-uri care ruleaza permanent:
1. thread de log care este "trezit" din functia writeMessage
atunci cand trebuie scris un mesaj in log

2. thread care cauta in fiecare fisier 10 cuvinte cu frecventele cele mai mari de aparitie
(search cauta doar in aceste liste cu 10 cuvinte)
Acest thread este trezit doar de update si put, deoarece delete sterge intrarea struct file cu totul,
deci impreuna cu lista de cuvinte.

La primirea semnalului SIGINT se asigura ca toate thread-urile termina daca aveau o operatie importanta de facut,
asteapta sa elibereze mutex-ul si inchide thread-ul.




globalFilesList-> tine cont de toate fisierele din ./DIR si de listele de cuvinte
logBuffer-> Buffer-ul asociat thread-ului de log
tidsList-> Lista cu toate tid-urile thread-urilor