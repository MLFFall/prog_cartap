#include <io.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

//------------------------------------------------
// Programme pour le TP de porte monnaie
// Ce programme permet à l'utilisateur de récupérer son solde, recharger et dépenser à partir de la carte
//
//------------------------------------------------


// d�claration des fonctions d'entr�e/sortie d�finies dans "io.c"
void sendbytet0(uint8_t b);
uint8_t recbytet0(void);

// variables globales en static ram
uint8_t cla, ins, p1, p2, p3;	// ent�te de commande
uint8_t sw1, sw2;		// status word


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

// Declaration de la version dans flash
const char vers[] PROGMEM= "1.03";

// �mission de la version
// t est la taille de la cha�ne sv
void version(int t)
{
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


//le solde es codé sur 2 octets et la valeur initiale est de 10000
const uint16_t max = 0xFFFF;
uint16_t solde EEMEM=10000;

/***
 * Fonction de récupération du solde : case 1
 * */
void GetSolde(){

    	// v�rification de la taille
    	if (p3!=2)
    	{
        	sw1=0x6c;	// taille incorrecte
        	sw2=2;		// taille attendue
        	return;
    	}
    uint16_t r_solde = eeprom_read_word(&solde);
    sendbytet0(ins);	// acquittement
    //convention big endian
    sendbytet0(r_solde >> 8);
    sendbytet0(r_solde & 0xFF );
    sw1=0x90;
}

/**
 * Recharge du compte : case 2
 * */
void recharge(){
	uint16_t ajout;
	if (p3!=2)
	{
		sw1=0x6c;	// taille incorrecte
        sw2=2;		// taille attendue
       	return;
	}
	sendbytet0(ins);
	ajout = (uint16_t) recbytet0() << 8;				// récupération du premier octet
	ajout += (uint16_t) recbytet0();					// concaténation des deux octet
	uint16_t tmp_solde = eeprom_read_word(&solde);		// récupère le solde dans le ram
	uint16_t amount = tmp_solde + ajout;				// calcul du nouveau solde
	if(amount % max < tmp_solde){						// vérification du solde
		sw1=0x61 ;										// solde max dépassé
	}
	else
	{ 
		eeprom_write_word(&solde ,amount);				// écriture du nouveau solde
		sw1=0x90;
	}

}

/**
 * Fonction de dépense case 3
 * */
void depense(){
	uint16_t amount;
	if (p3 !=2 ){
		sw1=0x6c;
		sw2=2;
		return;
	}
	sendbytet0(ins);
	// récupération montant dépensé
	amount = (uint16_t) recbytet0() << 8;
	amount += (uint16_t) recbytet0();
	uint16_t tmp_solde = eeprom_read_word(&solde);
	if(amount > tmp_solde){
		sw1 = 0x61;			//solde insufisant
	}
	else{
		tmp_solde -= amount;
		eeprom_write_word(&solde, tmp_solde);
		sw1 = 0x90;
	}
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
  	atr(11,"Prog Bourse");

	//ee_taille=0;
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
				version(4);
				break;
			case 1:	
				GetSolde();
				break;
			case 2:
				recharge();
				break;
			case 3:
				depense();
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

