#include <io.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#define VIDE 12345
#define PLEIN 54321
#define NB_ELTS 4 //Nombre d'élements maximum dans une transaction 

//------------------------------------------------
// Programme Pour implémenter une seconde couche de sécurité : l'anti rejeu
// Cela permettra à une personne malveillante de ne pas pouvoir lancer plusieurs fois une même opération de crédit
// Pour cela cette fois on utilise 4 octets de signature, 2 pour le numéro de la transaction et 2 autre pour le montant
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


/***
 * ********TEA*******
 * 
 * 
 */

// chiffrement
// clair[2] : clair 64 bits
// crypto[2] : cryptogramme calculé 64 bits
// k[4] : clé 128 bits
void tea_chiffre(uint32_t * clair,uint32_t * crypto, uint32_t * k)
{
    uint32_t    y=clair[0],
                z=clair[1],
                sum=0;
    int i;
    for (i=0;i<32;i++)
    {
        sum += 0x9E3779B9L;
        y += ((z << 4)+k[0]) ^ (z+sum) ^ ((z >> 5)+k[1]);
        z += ((y << 4)+k[2]) ^ (y+sum) ^ ((y >> 5)+k[3]);
    }
    crypto[0]=y; crypto[1]=z;
}

// déchiffrement
// crypto[2] : cryptogramme
// clair[2] : clair calculé
// k[4] : clé 128 bits
void tea_dechiffre(uint32_t* crypto ,uint32_t* clair, uint32_t*k)
{
    uint32_t    y=crypto[0],
                z=crypto[1],
                sum=0xC6EF3720L;
    int i;
    for (i=0;i<32;i++)
    {
        z -= ((y << 4)+k[2]) ^ (y+sum) ^ ((y >> 5)+k[3]);
        y -= ((z << 4)+k[0]) ^ (z+sum) ^ ((z >> 5)+k[1]);
        sum -= 0x9E3779B9L;
    }
    clair[0]=y; clair[1]=z;
}


/**
 * 
 * Fonctions de transaction
 * 
 * */

//Mémoire tampon
uint16_t ee_state EEMEM=VIDE; 			//Etat de la mémoire tampon
uint8_t ee_nb_elts EEMEM = 0; 			//nombre d'éléments de la transaction (dans le cas de l'intro perso a deux éléments)
uint8_t ee_tailles[NB_ELTS] EEMEM;		//stocke la taille de chaque élément
uint16_t ee_destinations[NB_ELTS] EEMEM;	//stocke les destinations dans la mémoire
uint8_t tampon[100] EEMEM;			//stocke les données

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


// Introduction de la clé dans l'eeprom
uint8_t ee_key[16] EEMEM;

void IntroKey()
{
        int i;
        uint8_t key[16];
        // v�rification de la taille
        if (p3!=16)
        {
                sw1=0x6c;       // taille incorrecte
                sw2=16;          // taille attendue
                return;
        }
        sendbytet0(ins);        // acquittement
        // �mission des donn�es
    
        for(i=0;i<p3;i++)
        {
                key[i] = recbytet0();
        }
        //eeprom_write_block(&key, &ee_key, 16);
        Engage(16,key, ee_key, 0);
        valide();

        sw1=0x90;
}

const uint16_t max = 0xFFFF;
const uint32_t INTEGRITE = 0;

//Structure de données du solde 2 octets pour le num transaction et 2 autre pour le solde
struct structSolde
{
	uint16_t solde;
	uint16_t compteur; 
};
struct structSolde monSolde EEMEM={10000,0};


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
    uint16_t r_solde = eeprom_read_word(&monSolde.solde);
    sendbytet0(ins);	// acquittement
    //convention big endian
    sendbytet0(r_solde >> 8);
    sendbytet0(r_solde & 0xFF );
    sw1=0x90;
}

void GetKey(){

    	// v�rification de la taille
    	if (p3!=16)
    	{
        	sw1=0x6c;	// taille incorrecte
        	sw2=16;		// taille attendue
        	return;
    	}
    uint8_t key[16];
    eeprom_read_block(&key, &ee_key, 16);
    sendbytet0(ins);	// acquittement
    //convention big endian
    for(int i=0; i<16; i++) sendbytet0(key[i]);
    sw1=0x90;
}

/**
 * Recharge du compte : case 2
 * */
uint16_t creditClair[4];

void recharge(){
	uint8_t creditChiffre[8];
	if (p3!=8)
	{
		sw1=0x6c;	// taille incorrecte
        	sw2=8;		// taille attendue
       		return;
	}
	sendbytet0(ins);
	//big endian
	for(int i=0;i<p3;i++)
        {
                creditChiffre[i] = recbytet0();
        }

        uint32_t key[4];
        eeprom_read_block(&key, &ee_key, 16);
        tea_dechiffre((uint32_t*)creditChiffre, (uint32_t*)creditClair, key);
        //verification de l'integrité
        uint32_t verif = (uint32_t)(creditClair[3]<<8);
        verif +=  (uint32_t)creditClair[2];
        	if(verif  != INTEGRITE){
        		sw1 = 0x62;		//ce crédit n'est pas intègre
        		return;
        	}
       	//verification du compteur
       	uint16_t r_count = eeprom_read_word(&monSolde.compteur);
       	uint16_t new_count = creditClair[1];
       	
       	if(new_count <= r_count){
       		sw1 = 0x63;
       		return;
       	}

        uint16_t credit = creditClair[0];
        
        uint16_t r_solde = eeprom_read_word(&monSolde.solde);

        
	uint16_t amount = credit + r_solde;				// calcul du nouveau solde
	
	if(amount % max < r_solde){					// vérification du solde
		sw1=0x61 ;										// solde max dépassé
	}
	else
	{
		r_count++;
        	Engage(2, &amount, &monSolde.solde, 2, &r_count, &monSolde.compteur, 0);
        	valide();
        	//eeprom_write_word(&monSolde.solde ,amount);
		sw1=0x90;
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
  	atr(10,"Anti rejeu");

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
				IntroKey();
				break;
			case 2:
				recharge();
				break;
			case 3:
				GetKey();
				break;
			case 4:
				GetSolde();
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

