<h1>Programmation Carte à Puce</h1>

Regroupement des TP d'initiation à la programmation carte à puce en Master 1 à l'université paris 8

Ces différents TP aboutissent à un projet de porte monnaie et à sa sécurisation à partir d'une carte à puce

La norme utilisé est ISO 7816 et la communication avec la carte ce fait grace au commandes APDU.

Le compilateur utilisé est gcc-avr et le programme est écrit en C et la simulation ce fait grace au programme scat.
<h2>Installation de l'espace de travail</h2>
Dans un environnement linux:

1. Installer les paquets:
<ul>
<li>gcc-avr</li>
<li>avr-libc</li>
<li>binutils-avr</li>
<li>libpcsclite-dev</li>
<li>pcsc-tools</li>
<li>avrdude</li>
<li>pcscd (et le lancer)</li>
</ul>
2. Installer le paquet libreadline-dev

3. Copier le fichier <em>10-tty.rules</em> dans le répertoire <strong>/etc/udev/rules.d</strong>, cela permet d'avoir les droit d'écriture sur le programmateur et relancer le service <em>udev</em>.

<h2>Compiler et Exécuter</h2>
Pour compiler:
1. Déplacez-vous dans le dossier hello
2. Tapez la commande <strong><em>make progcarte</em></strong>
NB: Le programme doit se trouver dans un fichiers hello.c (consulter le makefile)

Pour exécuter:
1. Dans le dossier du projet exécuter le programme scat
2. Puis dans le prompt taper la commande à exécuter
Syntaxe: <strong>classe ins P1 P2 P3 [data]</strong>
Dans ce projet la classe <strong>80</strong> a été utilisé vous pouvez vérifier l'instruction dans le code avant la mise à jour de ce readme.

#C#programmationC#cartap#systemeembarque#avr#avrgcc#cartapuce#scat
#MLF
