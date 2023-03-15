#include <io.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdarg.h>
#define VIDE 12345
#define PLEIN 54321
#define NB_ELTS 4 //Nombre d'élements maximum dans une transaction 
//Les données de la mémoire tampon 

//------------------------------------------------
// Programme pour creer des transactions
// Cela nous permet de garder l'etat initial de la carte si un souci se passe pendant la transaction
//On utilise une mémoire tampon ou on stocke les données lors de la transaction
// et une variable état pour connaitre l'état de la carte avant d'écrire les données
// Le nombre d'éléments maximum par transaction est 4
//------------------------------------------------


// d�claration des fonctions d'entr�e/sortie d�finies dans "io.c"
void sendbytet0(uint8_t b);
uint8_t recbytet0(void);

// variables globales en static ram
uint8_t cla, ins, p1, p2, p3;	// ent�te de commande
uint8_t sw1, sw2;		// status word

// Declaration de la version dans flash
const char vers[] PROGMEM= "1.02";
//char * pvers=vers;

// Proc�dure qui renvoie l'ATR
void atr(uint8_t n, char* hist)
{
    	sendbytet0(0x3b);	// d�finition du protocole
    	sendbytet0(n);		// nombre d'octets d'historique
    	while(n--)		// Boucle d'envoi des octets d'historique
    	{
        	sendbytet0(*hist++);
    	}
}


// �mission de la version
// t est la taille de la cha�ne sv

void versionFromFlash(int t){
	int i;
    	// v�rification de la taille
    	if (p3!=t)
    	{
        	sw1=0x6c;	// taille incorrecte
        	sw2=t;		// taille attendue
        	return;
    	}
	sendbytet0(ins);	// acquittement
	// �mission des donn�es
	for(i=0;i<p3;i++)
    	{
        	sendbytet0(pgm_read_byte(&vers[i]));
    	}
    	sw1=0x90;
}

/********************
 * 
 * Intro perso pour stoquer les données de l'utilisatateur
 * et get réponse pour lire ces données
 * On utilise les transactions avec les fonctions engage() et valide()
 * 
 * */


//Variable pour stocker les données dans l'eeprom
uint8_t ee_taille EEMEM=0; 		
char ee_data[16] EEMEM;	
	

//Mémoire tampon
uint16_t ee_state EEMEM=VIDE; 			//Etat de la mémoire tampon
uint8_t ee_nb_elts EEMEM = 0; 			//nombre d'éléments de la transaction (dans le cas de l'intro perso a deux éléments)
uint8_t ee_tailles[NB_ELTS] EEMEM;		//stocke la taille de chaque élément
uint16_t ee_destinations[NB_ELTS] EEMEM;	//stocke les destinations dans la mémoire
uint8_t tampon[100] EEMEM;			//stocke les données

/**
 * Fonction Engage récupère les données et les stocke dans la mémoire tampon
 * Pour chaque donnée il prend la taille en octets , l'adresse ou elle est stocké dans la ram et l'adresse de destination dans l'eeprom
 * On utilise la bibliothéque stdarg.h de c pour programmer un nombre de paramètre dynamique
 * A la fin on marque l'état comme PLEIN si tous ces bien passé
 * */
void Engage(int n,...)
{
	void* AddrRam;				//adresse des données dans la ram
	uint16_t AddrEE;			//adresse où on stoque les données dans l'eeprom
	va_list args;				//pointeur sur les arguments
	va_start(args, n);			//récupère premier argument (la taille)
	uint8_t i=0;
	uint8_t *p=tampon;			//pointeur sur le tampon
	while(n!=0)
	{
		AddrRam=va_arg(args, void * ); 
		AddrEE=(uint16_t) va_arg(args, void * );
		eeprom_write_block(AddrRam,p,n); 			//ecriture dans le tampon
		eeprom_write_byte(&ee_tailles[i],n);			//stocke la taille
		eeprom_write_word(&ee_destinations[i], AddrEE);		//stocke la destination
		p += n;							//on se déplace dans le tampon pour pas écraser les données
		n=va_arg(args, int);					//on récupère la taille des prochains données
		i++;
	}
	eeprom_write_word(&ee_state, PLEIN);				//l'état devient plein
	eeprom_write_byte(&ee_nb_elts, i);				//on sauvegarde le nombre d'éléments

}

/**
 * Cette fonction vérifie l'etat et si elle est pleine recopie les données de la mémoire tampon dans l'eeprom
 * */
void valide(){
	uint8_t nbe;		//nombre d'éléments
	uint8_t i,j;
	uint8_t *p = tampon;	// adresse de la mémoire tampon
	uint8_t *destination;	//destination des donnée dans l'eeprom
	uint8_t t;		//taille de l'élément e
	if (eeprom_read_word(&ee_state)==PLEIN){
		nbe = eeprom_read_byte(&ee_nb_elts);
		for(i=0; i<nbe; i++){
			t = eeprom_read_byte(&ee_tailles[i]);					//récupère la taille
			destination = (uint8_t*)eeprom_read_word(&ee_destinations[i]);		//récupère la destination
			for(j=0; j<t; j++){
				eeprom_write_byte(destination++, eeprom_read_byte(p++));	//on écrit dans la destination le contenu de la mémoire tampon
			}
		}
		eeprom_write_word(&ee_state, VIDE);		//état à vide
	}
}

void IntroPerso()
{
	char data[16];
        int i;
        // v�rification de la taille
        if (p3>16)
        {
                sw1=0x6c;       // taille incorrecte
                sw2=16;          // taille attendue
                return;
        }
        sendbytet0(ins);        // acquittement
        // �mission des donn�es
    
        for(i=0;i<p3;i++)
        {
                data[i] = recbytet0();
        }
        Engage(1,&p3, &ee_taille, p3, data, ee_data, 0);
        valide();
        sw1=0x90;
}

void GetReponse(){
	char data[16];
	uint8_t taille = eeprom_read_byte(&ee_taille); 
	int i;
    	// v�rification de la taille
    	if (p3!=taille)
    	{
        	sw1=0x6c;	// taille incorrecte
        	sw2=taille;		// taille attendue
        	return;
    	}
    eeprom_read_block(data, ee_data, taille);
    sendbytet0(ins);	// acquittement
    for(i=0;i<p3;i++)
    	{
        	sendbytet0(data[i]);
    	}
    	sw1=0x90;
}


// Programme principal
//--------------------
int main(void)
{
  	// initialisation des ports
	ACSR=0x80;
	PORTB=0xff;
	DDRB=0xff;
	DDRC=0xff;
	DDRD=0;
	PORTC=0xff;
	PORTD=0xff;
	ASSR=(1<<EXCLK)+(1<<AS2);
	//TCCR2A=0;
//	ASSR|=1<<AS2;
	PRR=0x87;


	// ATR
  	atr(11,"Transaction");

	ee_taille=0;
	sw2=0;		// pour �viter de le r�p�ter dans toutes les commandes
  	// boucle de traitement des commandes
  	for(;;)
  	{
    		// lecture de l'ent�te
    		cla=recbytet0();
    		ins=recbytet0();
    		p1=recbytet0();
	    	p2=recbytet0();
    		p3=recbytet0();
	    	sw2=0;
		switch (cla)
		{
	  	
		case 0x80:
			switch(ins)
			{
			case 0:
				versionFromFlash(4);
				break;
			case 1:	
				IntroPerso();
				break;
			case 2:
				GetReponse();
				break;
			default:
				sw1=0x6d;	//code erreur ins inconnu
			}
			break;
      	default:
        		sw1=0x6e; // code erreur classe inconnue
		}
		sendbytet0(sw1); // envoi du status word
		sendbytet0(sw2);
	
  	}
  	return 0;
}

