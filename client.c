#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "client.h"



int sd;
struct sockaddr_in serv_addr;
int idUser;
char pseudo[15];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

fd_set set; // Ensemble des descripteurs de fichiers en lecture

struct timeval timeout;


int idSalon;


void clean_stdin(void)
{
    int c;

    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
}


void initSelect(){

    /* Initialize the file descriptor set. */
    FD_ZERO (&set);
    FD_SET (STDIN_FILENO, &set);
    FD_SET (sd, &set);


    /* Initialize the timeout data structure. */
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
}

int main (int argc, char *argv[])
{
    //Variables du main
    int cptr=0;
    pthread_t threadHeartBeat;
    char addresseIP[20];
    
    /*******************ACCEUIL********************/
    //Accueil: config user & ip
    system("clear");
    printf("\n\n");
    printf("         ///   ///////       //////  \n");
    printf("        ///   ///    ///  ///        \n");
    printf("       ///   ///////      ///        //   ||_||  //\\\\ =||=\n");
    printf("      ///   ///   ///      ///////   \\\\   || || //~\\\\  || \n\n");
    printf("      ___________________________\n");
    printf("               CONNEXION         \n");
    printf("      ___________________________");
    printf("\n      Pseudo: ");
    scanf("%s",pseudo);
    clean_stdin();
    do{
        printf("\n      IP serveur: ");
        scanf("%s",addresseIP);
        clean_stdin();
    }while(creerSocket(addresseIP)==-1);



    //Thread de perssistance de connexion
    pthread_mutex_lock(&mutex);
    if(pthread_create(&threadHeartBeat, NULL, heartBeats, NULL) == -1) {
        perror("pthread_create threadHeartBeat");
        return EXIT_FAILURE;
    }


    printf("\n===// IRCChat //===========================================\n");

    // Connexion au serveur
    int code;
    do{
        code = connexion();
        switch(code){

            case ConnectNumberRefuse:
                printf("\n      *Veuillez réessayer plus tard.");
                exit(-1);


            case ConnectUserRefuse:
                printf("\n      Veuillez saisir un autre pseudo : ");
                scanf("%s",pseudo);
                clean_stdin();
                break;
        }
     }while(code!= ConnectOk);

    /*******************MODE T'CHAT********************/
    system("clear");
    printf("\n===// IRCChat //==============================================");

    printf("\n     | *Connexion acceptée");

    //lancer le thread HeartBeat
    pthread_mutex_unlock(&mutex);



    int retval;
    Trame trame;


    while(1){

        initSelect();
        retval = select(sd+1,&set, NULL, NULL,&timeout);

        if (retval == -1)
        {
            perror ( "\n     | *Erreur de select." ) ;
        }
        else if (retval == 1)
        {


            // Nouvelles données clavier
            if (FD_ISSET(0, &set))
            {
                printf("\t| *Clavier\n");
                traitementEnvoye();
            }

                //Nouveau message du serveur;
            else if(FD_ISSET(sd, &set)){
                printf("\t| *Reseau\n");
                recevoir(&trame,serv_addr);
                traitementReception(trame);
            }

        }
        else{
            printf("\n     | *TIMEOUT\n");
            cptr++;
            if(cptr==3){
                printf("\n     | *Le serveur n'est plus disponible...Connexion perdue.");
                exit(-1);
            }
        }



    }

    close(sd);
    return 0;
}

void traitementReception(Trame trameRecue){
    switch(trameRecue.ID_OP){
        case ConnectOk :
            idUser = trameRecue.ID_USER;//maj id
            printf("\n     | *%s\n",trameRecue.DATA);
            break;
        case JoinOk :
	    printf("\n     | *%s\n",trameRecue.DATA);
            break;
        case JoinRefuse :
	    printf("\n     | *%s\n",trameRecue.DATA);
            break;
        case SayOk :
	    printf("\n     | *%s\n",trameRecue.DATA);
            break;
        case SayError :
	    printf("\n     | *%s\n",trameRecue.DATA);
            break;
	case ErrorCommande :
	    printf("\n     | *%s\n",trameRecue.DATA);
            break;
        case Echo :
            printf("\n     | %s",trameRecue.DATA);
            break;

    }

}




int traitementEnvoye() {

    printf("Debut du traitement\n");

    Trame trame;

    char buf[2000];



    scanf("%[^\n]",buf);
    clean_stdin();

    const char s[2] = " ";

    char *token;

    /* get the first token */
    token = strtok(buf, s);


    trame.ID_OP = commandToInt(token);

    if(trame.ID_OP==-1)
        printf("Commande inconue : %s\n",token);
    else
        printf("Commande reconnue : %s\n",token);

    token = strtok(NULL, s);

    strcpy(trame.DATA,"");
    /* walk through other tokens */
    while( token != NULL )
    {
        strcat(trame.DATA,token);
        token = strtok(NULL, s);
    }

    trame.ID_SALON = idSalon;
    trame.ID_USER = idUser;

    int ret =  envoyer(trame,serv_addr);
    if(trame.ID_OP==Disconnect)
    {
        printf("ByeBye....\n");
        exit(0);
    }

    return ret;


}




int commandToInt(char * command) {

    if(!strcmp(command,":say"))
        return Say;
    if(!strcmp(command,":join"))
        return Join;
    if(!strcmp(command,":leave"))
        return Leave;
    if(!strcmp(command,":liste"))
        return Liste;
    if(!strcmp(command,":disconnect"))
        return Disconnect;


    //n'est pas une commande
    return -1;
}




int connexion(){
    Trame trame;
    trame.ID_OP = Connect;
    strcpy(trame.DATA,pseudo);
    socklen_t taille=sizeof(serv_addr);

    if(envoyer(trame,serv_addr)==-1){
        perror("sendto");
        exit( -1);
    }


    if(recevoir(&trame,serv_addr) == -1){
        perror("sendto");
        exit(-1);
    }

    idUser = trame.ID_OP== ConnectOk ? trame.ID_USER : -1;
    printf("%s\n",trame.DATA);
    return trame.ID_OP;

}




int creerSocket(const char * adresseIp){

    struct sockaddr_in client_addr;

    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port        = htons(0);

    if ((sd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Impossible de creer le socket");
        return -1;
    }

    if (bind(sd,(struct sockaddr *)&client_addr, sizeof client_addr) == -1)
    {
        perror("bind");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    if (inet_aton(adresseIp, &(serv_addr.sin_addr)) == 0)
    {
        printf("Invalid IP address format <%s>\n",adresseIp);
        return -1;
    }
    serv_addr.sin_port = htons(SERVER_PORT);

    return 1;
}


void* heartBeats(void* arg){

    Trame trame;

    trame.ID_OP = HeartBeat;
    trame.ID_USER = idUser;

    pthread_mutex_lock(&mutex);
    printf("\nLe Coeur commence à battre^^\n");

    while(1){
        sleep(FREQ_HEART);
        envoyer(trame,serv_addr);
    }
}

int envoyer(Trame trame, struct sockaddr_in addresseServeur) {
    return  (int)sendto(sd, &trame , sizeof(trame) , 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
}

int recevoir(Trame * trame, struct sockaddr_in addresseServeur) {
    socklen_t taille=sizeof(serv_addr);
    return (int)recvfrom(sd, trame, sizeof(*trame) , 0, (struct sockaddr *)&serv_addr, &taille );
}
