
Claudia Rogoz
331CA 
	Mini Shell
	
Tema a constat in realizarea unui "mini-shell" care sa parseze si execute comenzi
interne si externe, dar si asignari de environment variables. 

Functiile de care m-am folosit in implementare sunt descrise in fisierul utils-lin.c;
In ceea ce priveste parsareea comenziilor, m-am folosit de skeletul de cod oferit.

Completari in fiserul utils-lin.c:

*functia shell_cd:
	apeleaza functia chdir de schimbare a directorului curent; Am considerat
	separat cazul in care argumentul comenzii "cd" era ~; In acest caz, chdir
	primea ca parametru valoarea $HOME (directorul home al utilizatorului curent)

*functia shell_exit(corespunzatoare comenzilor interne quit/exit):
	apeleaza macroul SHELL_EXIT

*functiile redirect_stdin/redirect_stdout/redirect_error
	fac redirectari in fisierele date ca parametrii;
	totodata in functie de flagurile setate, deschiderea
	si redirectarea catre fisiere este diferita;
	in cazul redirectarii catre std_err, se verifica
	daca file_out == file_err, caz in care se file 
	descriptorii vor fi fd_err = fd_out;
*functia parse_simple
	-apeleaza functiile shell_cs, respectov shell_exit in cazul comenzilor 
	interne(quit, exit, cd);
	-se ocupa de variabilele de mediu: se verifica daca in cadrul comenzii
	primite exista un "=" atunci va asigna valoarea cu variabila prin
	intermediul apelului setenv()
	-in final, daca este comanda externa, executia acesteia este realizata
	prin apelul unui fork(), prin redirectari(daca este cazul) si apoi prin
	apelul execvp()
*functia do_in_parallel: executa 2 comenzi in paralel prin apelul unui 
	fork() si apoi atribuirea atat parintelui, cat si copilului a 
	celor 2 comenzi date ca parametru
*functia do_on_pipe:
	se creeaza un pipe pentru comunicarea intre procesle (X si Y)care
	vor executa cmd1 si cmd2; se face apoi un fork(), in urma caruia copilul X
	va execute prima comanda data ca parametru(cmd1); de asemenea, copilul va
	scrie rezultatul in pipe la iesire, urmand ca apoi parintele sa mai faca
	un fork(), in urma caruia copilul Y va citi datele de le pipe; la final, parintele
	isi asteapta copiii sa termine 
*functia parse_command:
	in functie de tipul operatiei se apeleaza, conform si comentariilor din cod
	functiile corespunzatoare
 
	

