# Primjer za Projekt R

Korišteni materijali: ESP32 i ESP32C6 mikrokontroleri, TSL2591 senzor svjetline, 1 LED dioda, žice za povezivanje

Primjer prikazuje osnovnu komunikaciju između mikrokontrolera na temelju IoT arhitekture formata "thing - edge - cloud"

## Brzi opis spajanja
- svjetlosni senzor je spojen na ESP32 mikrokontroler
- LED dioda spojena je na ESP32C6 (ESP32C6 ima pričvršćenu antenu za WiFi)
- Oboje su fizički spojeni na računalo zbog napajanja

# Opis djelovanja
## Prvi smjer
- Senzor svjetlosti redovno očitava vrijednosti svjetla. One se preko mikrokontrolera šalju na laptop (publishaju), gdje se nalazi lokalni Mosquitto (MQTT) broker
- edge(tj. broker) prima podatke, tamo ih kratka Python skripta (try.py) obrađuje tj. filtrira
- nakon filtracije, željeni podaci preko bridge-a (WiFi spojeno) šalju se na cloud - EMQX Serverless Cloud broker
## Drugi smjer
- Cloud broker prosljeđuje podatke subscriberu tj. ESP32C6 mikrokontroleru, ponovo preko WiFi mreže 
- ESP32C6 dodatno provjerava primljene podatke te pali LED diodu na kratko kako bi signalizirao primitak podatka
  
## Mogućnost dvosmjernog slanja podataka s "command"
- uz to što je ESP32 publisher podataka te ESP32C6 subscriber podataka, oba mikrokontrolera subscribana su na esp32/command topic gdje se, uz autentifikaciju s cloudom, mogu slati proizvoljni podatci samim mikrokontrolerima koji se onda pojavljuju u log-u mikrokontrolera

## Objašnjenje datoteka
- u repozitoriju se nalaze dva foldera
- u svakom se nalaze nužni i neki dodatni fileovi neophodni za primjer
- uz app_main.c za svaki mikrokontroler, priloženi su i među ostalim:
  - konfiguracijski file za Mosquitto broker
  - neke korištene komande kako bi se uspostavili brokeri/publishalo ili subscribalo na cloud ili samo lokalno na broker (za testiranje)
  - kod Python skripte koja filtrira podatke na edge-u
  - konfiguracijski fileovi itd.

Dio (baza) source koda preuzeta je s Espressif GitHub repozitorija ( https://github.com/espressif/esp-idf/tree/ad20ded62a2d67a26e501fdc1905f43ba7ad6c81/examples/protocols/mqtt ) te nadograđena za potrebe primjera.

Dijelovi koda (šifre, korisnička imena) te sigurnosni certifikati uklonjeni su ili zamijenjeni placeholderima. 