# RichEditor - Uzivatelska prirucka (Cestina)

RichEditor je lehky, pristupny Win32 textovy editor zamereny na rychlou praci s prostym textem a silne filtry (externi prikazy).

## Zacatek

- Spusteni: `RichEditor.exe`
- Otevrit soubor: `Soubor -> Otevrit` nebo `Ctrl+O`
- Ulozit: `Ctrl+S` (u nepojmenovaneho souboru se zobrazi Ulozit jako)
- Rezim pouze pro cteni: `Soubor -> Pouze pro cteni` nebo `/readonly` na prikazove radce

### Parametry prikazove radky

```
RichEditor.exe [volby] [soubor]

Volby:
  /nomru     Otevrit bez pridani do MRU
  /readonly  Otevrit v rezimu pouze pro cteni
```

## Zaklady rozhrani

### Stavovy radek

Zobrazuje:
- Radek a sloupec (vypocet zahrnuje tabulatory)
- Znak pod kurzorem (Unicode)
- Aktivni filtr

### Zalomeni radku

Prepinani: `Zobrazit -> Zalomeni radku` nebo `Ctrl+W`. Pri zapnuti se ukazuje vizualni i fyzicka pozice.

## Editace

- Popisky Zpet/Znovu odpovidaji posledni operaci
- Vlozit datum/cas: `F5`
- Standardni zkratky: Vystrihnout/Kopirovat/Vlozit/Vybrat vse

### Datum/Cas sablony

Vystup klavesy `F5` ridite v INI:

```
[Settings]
DateFormat=%shortdate%
TimeFormat=HH:mm
DateTimeTemplate=%date% %time%
```

## Hledat a Nahradit

- Hledat: `Ctrl+F`
- Dalsi vyskyt: `F3`
- Predchozi vyskyt: `Shift+F3`
- Nahradit: `Ctrl+H`

Volby:
- Rozlisovat velikost
- Cele slovo
- Pouzit escape sekvence

Podporovane escape sekvence:
- `\n`, `\r`, `\t`, `\\`
- `\xNN` (hex byte)
- `\uNNNN` (Unicode)

Zastupne znaky v Nahradit:
- `%0` vlozi nalezeny text
- `%%` vlozi znak `%`

Historie se uklada jen po provedeni Hledat/Nahradit/Nahradit vse.
Nahradit vse je plne vratitelne (Undo).

## Filtry (pokrocile)

Filtry spousti externi prikazy nad vyberem (nebo aktualnim radkem) a mohou:
- Vlozit vystup
- Zobrazit vystup
- Zkopirovat do schranky
- Spustit bez vystupu (vedlejsi efekt)

### Spusteni filtru

1) Zvolte filtr: `Nastroje -> Vybrat filtr`
2) Spustte: `Ctrl+Enter`

### Interaktivni REPL

- Start: `Ctrl+Shift+I`
- Konec: `Ctrl+Shift+Q`
- Enter na radku s promptem odesle prikaz

## Sablony

- Vlozit sablonu: `Nastroje -> Vlozit sablonu`
- Vyber sablon: `Ctrl+Shift+T`
- Soubor -> Novy obsahuje sablony pro nove dokumenty

Promenne v sablonach:
- `%cursor%`, `%selection%`, `%date%`, `%time%`, `%clipboard%`

## Autosave a obnova relace

- Casovac autosave (vychozi: 1 minuta)
- Autosave pri ztrate fokusu (pri prepnuti do jine aplikace)
- Autosave standardne neuklada nepojmenovane soubory

Obnova relace:
- Pri vypnuti systemu se neulozena prace zachova
- Pri dalsim spusteni se obsah obnovi s indikaci `[Resumed]`
- Volitelne: `AutoSaveUntitledOnClose=1` pro poznamkovy rezim

## URL

- URL se detekuji automaticky
- Enter na URL ji otevre
- Kontextove menu na URL nabizi Otevrit/Kopirovat

## Konfigurace (RichEditor.ini)

INI soubor se vytvori vedle exe pri prvnim spusteni. Je bezpecne jej upravovat.

Hlavni volby:

```
[Settings]
WordWrap=1
TabSize=8
ShowMenuDescriptions=1
AutosaveEnabled=1
AutosaveIntervalMinutes=1
AutosaveOnFocusLoss=1
SelectAfterPaste=0
AutoSaveUntitledOnClose=0
```

## Klavesove zkratky (zakladni)

```
Ctrl+N  Novy
Ctrl+O  Otevrit
Ctrl+S  Ulozit
Ctrl+Z  Zpet
Ctrl+Y  Znovu
Ctrl+X  Vystrihnout
Ctrl+C  Kopirovat
Ctrl+V  Vlozit
Ctrl+A  Vybrat vse
Ctrl+W  Zalomeni radku
F5      Vlozit datum/cas
Ctrl+F  Hledat
F3      Dalsi vyskyt
Shift+F3 Predchozi vyskyt
Ctrl+H  Nahradit
Ctrl+Enter  Spustit filtr
Ctrl+Shift+I Start REPL
Ctrl+Shift+Q Konec REPL
Ctrl+Shift+T Vyber sablon
```

## Reseni problemu

- Filtr nic nedela: zkontrolujte prikaz ve `RichEditor.ini`.
- Hledani neobteka: obtaceni je zamerne vypnuto.
- Rezim pouze pro cteni blokuje editaci a Nahradit.
