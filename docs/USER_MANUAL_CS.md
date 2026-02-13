# RichEditor - Uživatelská příručka (Čeština)

RichEditor je lehký a přístupný textový editor pro systém Windows (Win32, klasické rozhraní pro aplikace pro pracovní plochu) určený pro prostý text. Je navržen tak, aby byl rychlý, ovládání zůstalo předvídatelné a pokročilé funkce byly soustředěny do externích nástrojů (filtrů a šablon). Tato příručka popisuje i funkce, které nejsou na první pohled zřejmé.

## Rychlý start

Otevírejte a ukládejte jako v klasickém editoru: `Soubor -> Otevřít` (`Ctrl+O`) a `Ctrl+S`. Zalamování řádků přepnete v `Zobrazit -> Zalamování řádků` (`Ctrl+W`). Najít, nahradit a přejít na řádek najdete pod `Hledat` (`Ctrl+F`, `Ctrl+H`, `Ctrl+G`).

## Základy rozhraní a stavový řádek

### Indikátory v titulku

RichEditor stav jednoznačně oznamuje. Titulek používá:

- `*` pro neuložené změny
- `[Pouze ke čtení]` pokud je editace blokována
- `[Obnoveno]` pokud byl dokument obnoven po vypnutí
- `[Interaktivní režim]` při běžícím interaktivním filtru

### Stavový řádek

Stavový řádek je navržen pro přesnost i přístupnost. Zobrazuje **pozici** (tabulátorově přesně), **znak** pod kurzorem a **aktuálně vybraný filtr**.

Při zapnutém zalamování se zobrazuje **vizuální** i **fyzická** pozice (měkké zalomení vs. skutečné řádky).

Pokročilé: pole znaku obsahuje kódy Unicode (mezinárodní znaková sada), řídicí znaky, náhradní páry a EOF (konec souboru).

## Editace

Popisky Zpět/Znovu popisují poslední akci (Psaní, Vložit, Nahradit vše, ...). Režim vložení/přepisování se přepíná klávesou **Insert** (Vložit). `F5` vloží datum/čas podle nastavení.

Soubory se ukládají v kódování UTF‑8 bez značky BOM (značka pořadí bajtů). Konce řádků se podle možnosti zachovávají; výchozí ukládání používá CRLF (návrat vozíku + nový řádek).

Pokročilé: `SelectAfterPaste=1` automaticky vybere vložený text. Psaní pak vybranou část nahradí.

## Zalamování řádků a pozice

Zalamování mění pouze **zobrazení**, nikoli uložení souboru.

- **Zapnuto**: řádky se zalamují podle šířky okna.
- **Vypnuto**: dlouhé řádky zůstávají na jednom fyzickém řádku.

Poznámka: Při vypnutém zalamování může textová komponenta RichEdit ve verzi 8 a vyšší stále vizuálně dělit dlouhé řádky kolem ~1000 znaků bez vložení zalomení. Odpovídá to chování Windows 11 Poznámkového bloku a jde pouze o způsob zobrazení.

## Najít a nahradit

Najít a nahradit funguje jako v klasických editorech (`Ctrl+F`, `Ctrl+H`, `F3`, `Shift+F3`). Historie se ukládá pouze po provedení Najít/Nahradit/Nahradit vše.

Přejít na řádek (`Ctrl+G`) skočí na zadané číslo řádku a posune jej do zobrazení.
Počítání řádků se řídí nastavením zalamování: při zapnutém zalamování se počítají vizuální řádky, při vypnutém fyzické řádky.

Pokročilé:
- Escape sekvence (zástupné zápisy znaků): `\n`, `\r`, `\t`, `\\`, `\xNN`, `\uNNNN`
- Zástupné znaky v náhradě: `%0` vloží nalezený text, `%%` vloží znak `%`
- Nahradit vše lze plně vrátit zpět.

## Automatické ukládání a obnova relace

Automatické ukládání běží v intervalu (výchozí 1 minuta) a při přepnutí do jiné aplikace. Nepojmenované soubory se standardně neukládají automaticky.

Při vypnutí nebo restartu systému Windows umí RichEditor obnovit neuloženou práci. Obnovené soubory mají v titulku `[Obnoveno]`.

Pokročilé: `AutoSaveUntitledOnClose=1` uloží nepojmenovanou práci při zavření bez potvrzení.

## Režim pouze pro čtení

Režim pouze pro čtení blokuje editaci, ale stále umožňuje prohlížení a navigaci. Lze otevírat soubory, Uložit jako, kopírovat a vybírat text, používat Najít a spouštět nedestruktivní filtry.

## Filtry (externí nástroje)

Filtry spouštějí externí příkazy nad výběrem (nebo aktuálním řádkem, pokud není výběr). Mohou vložit výstup, zobrazit jej, zkopírovat do schránky nebo běžet pouze pro vedlejší účinky. Tím se editor rozšiřuje bez nabalování vnitřních funkcí. Existují i interaktivní filtry (REPL, režim čti‑vyhodnoť‑vypiš) pro pokročilé použití, popsané v části o INI.

Příklady: Velká písmena, Malá písmena, Seřadit řádky, Počet řádků, Počet slov. Tyto příklady slouží jako základní ukázka; další možnosti jsou dostupné v nabídce.

Pokročilé: filtry a kategorie se definují v `RichEditor.ini`. Kategorie jsou plně uživatelské a lze je přejmenovat nebo vytvořit nové. Lze také nastavit, zda se filtr objeví v kontextové nabídce.

## Šablony

Šablony vkládají textové bloky s proměnnými. Můžete je vložit z `Nástroje -> Vložit šablonu`, použít výběr šablon (`Ctrl+Shift+T`), nebo vytvořit nový dokument ze šablony v `Soubor -> Nový`.

### Proměnné šablon

Šablony podporují:

- `%cursor%` — pozice kurzoru po vložení
- `%selection%` — aktuální výběr
- `%clipboard%` — text ze schránky
- `%date%`, `%time%` — nastavitelné formáty
- `%shortdate%`, `%longdate%`, `%yearmonth%`, `%monthday%`
- `%shorttime%`, `%longtime%`

Pokročilé: šablony lze upravovat v `RichEditor.ini` (konfigurační soubor). Jména a popisy šablon lze lokalizovat pomocí klíčů `Name.xx` a `Description.xx` (například `Name.cs`). Neznámé proměnné zůstávají jako text.

## Adresy URL

Adresy URL (webové odkazy) se detekují automaticky. Enter na adresu URL ji otevře, pravým tlačítkem lze adresu URL otevřít nebo zkopírovat.

## Konfigurace (INI – konfigurační soubor)

Při prvním spuštění se vytvoří `RichEditor.ini` (konfigurační soubor INI) vedle souboru exe (spustitelný soubor programu). Soubor je samopopisný díky komentářům a je bezpečné jej upravovat, pokud je aplikace zavřená. Pokud komentáře odstraníte a chcete je zpět, stačí INI smazat a nechat aplikaci znovu vytvořit.

Níže je kompletní, strukturovaný přehled všech sekcí a klíčů, které editor používá. Uvedené výchozí hodnoty odpovídají automaticky vytvořenému INI.

### [Settings]

Chování editoru a výchozí volby.

- `WordWrap` (výchozí `1`): 1 = zapnuto, 0 = vypnuto; určuje, zda se dlouhé řádky zalamují podle šířky okna.
- `AutosaveEnabled` (výchozí `1`): hlavní přepínač automatického ukládání.
- `AutosaveIntervalMinutes` (výchozí `1`): interval automatického ukládání; 0 vypne časovač, i když je automatické ukládání zapnuté.
- `AutosaveOnFocusLoss` (výchozí `1`): automatické ukládání při přepnutí do jiné aplikace.
- `ShowMenuDescriptions` (výchozí `1`): popisy filtrů v nabídce (přístupnost).
- `SelectAfterPaste` (výchozí `0`): automaticky vybere vložený text.
- `AutoSaveUntitledOnClose` (výchozí `0`): uloží nepojmenované soubory při zavření bez dotazu.
- `TabSize` (výchozí `8`): šířka tabulátoru v počtu mezer (1–32).
- `SelectAfterFind` (výchozí `1`): nechá nalezený text vybraný.
- `FindMatchCase` (výchozí `0`): výchozí hledání rozlišuje velikost písmen.
- `FindWholeWord` (výchozí `0`): výchozí hledání pouze celých slov.
- `FindUseEscapes` (výchozí `0`): povolí escape sekvence (\n, \t, \xNN, \uNNNN) v Najít/Nahradit.

Formát data a času.

- `DateTimeTemplate` (výchozí `%date% %time%`): vkládá `F5` a příkaz Čas a datum. Použijte `%date%`/`%time%` pro respektování `DateFormat`/`TimeFormat`, nebo použijte `%shortdate%`, `%longdate%`, `%shorttime%`, `%longtime%` přímo.
- `DateFormat` (výchozí `%shortdate%`): formát pro `%date%` v šablonách. Nastavte na vestavěnou proměnnou (např. `%shortdate%`) nebo vlastní formátovací řetězec.
- `TimeFormat` (výchozí `HH:mm`): formát pro `%time%` v šablonách. Nastavte na vestavěnou proměnnou (např. `%shorttime%`) nebo vlastní formátovací řetězec.

Proměnné data/času a vlastní formáty (formátovací značky systému Windows).

- Vestavěné proměnné:
  - `%shortdate%`: krátké datum systému (např. 20. 1. 2026).
  - `%longdate%`: dlouhé datum systému (např. pondělí 20. ledna 2026).
  - `%yearmonth%`: rok a měsíc (např. leden 2026).
  - `%monthday%`: měsíc a den (např. 20. ledna).
  - `%shorttime%`: krátký čas bez sekund (např. 22:30).
  - `%longtime%`: dlouhý čas se sekundami (např. 22:30:45).
- Značky pro datum: `d dd ddd dddd M MM MMM MMMM y yy yyyy g gg`.
- Značky pro čas: `h hh H HH m mm s ss t tt`.
- Příklady šablon: `DateTimeTemplate=%date% 'v' %time%`, `DateTimeTemplate=%longdate% 'v' %shorttime%`.
- Vlastní formáty data (příklady): `yyyy-MM-dd`, `dd.MM.yyyy`, `MMMM d, yyyy`.
- Vlastní formáty času (příklady): `HH:mm`, `h:mm tt`, `HH:mm:ss`.
- Literály: uzavřete do jednoduchých uvozovek (příklad: `DateTimeTemplate=%longdate% 'v' %shorttime%`).

Volba knihovny RichEdit (pokročilé).

RichEdit je textová komponenta editoru; v případě potřeby lze použít jinou nebo novější knihovnu.

- `RichEditLibraryPath` (výchozí prázdné): cesta k vlastní knihovně RichEdit (DLL, dynamická knihovna); prázdné = automatická volba.
- `RichEditClassName` (výchozí prázdné): volitelné přepsání třídy okna (např. `RichEditD2DPT`, `RichEdit60W`).

Interní stav (běžně se neupravuje ručně).

- `CurrentFilter` (výchozí prázdné): naposledy vybraný filtr podle názvu.
- `CurrentREPLFilter` (výchozí prázdné): naposledy vybraný interaktivní filtr (REPL) podle názvu.

### [Filters] a [FilterN]

`[Filters]` má jediný klíč:

- `Count` (výchozí `10`): počet filtrů v dalších sekcích.

Každý `[FilterN]` definuje jeden filtr. Povinné položky:

- `Name`: zobrazený název.
- `Command`: příkaz k provedení.

Běžné volitelné položky:

- `Description`: krátký popis (zobrazuje se v nabídce, pokud je zapnuto).
- `Category`: skupina v podnabídce (libovolný název).
- `Name.xx` / `Description.xx`: lokalizace (např. `Name.cs`).
- `Action` (výchozí `insert`): typ chování. `insert` = vložit výstup, `display` = zobrazit výstup, `clipboard` = uložit do schránky, `none` = pouze vedlejší účinek, `repl` = interaktivní režim (REPL).

Klíče specifické pro akci:

- `Insert` (výchozí `below`): `replace` (nahradit), `below` (vložit pod řádek) nebo `append` (připojit na konec).
- `Display` (výchozí `messagebox`): `messagebox` (dialogové okno) nebo `statusbar` (stavový řádek).
- `Clipboard` (výchozí `copy`): `copy` (zkopírovat) nebo `append` (připojit k již zkopírovanému).
- `PromptEnd` (výchozí `> `): ukončení REPL výzvy (např. `> ` nebo `$ `).
- `EOLDetection` (výchozí `auto`): rozpoznání konce řádku (EOL, konec řádku) ve výstupu REPL. `auto` se řídí prvním výstupem; `crlf` = Windows, `lf` = Unix/Linux (např. Linux a WSL – Subsystém Windows pro Linux), `cr` = klasický Mac.
- `ExitNotification` (výchozí `1`): 1 zobrazí dialog po ukončení REPL filtru, 0 bez hlášení.

Kontextová nabídka:

- `ContextMenu` (výchozí `0`): zobrazit v kontextové nabídce (1) nebo pouze v nabídce Nástroje (0).
- `ContextMenuOrder` (výchozí `999`): pořadí v kontextové nabídce; nižší číslo = výše.

### [Templates] a [TemplateN]

`[Templates]` má jediný klíč:

- `Count` (výchozí `15`): počet šablon v dalších sekcích.

Každý `[TemplateN]` definuje jednu šablonu:

- `Name`: název šablony.
- `Description`: krátký popis.
- `Category`: skupina v podnabídce (libovolný název).
- `Name.xx` / `Description.xx`: lokalizace (např. `Name.cs`).
- `FileExtension` (výchozí prázdné): omezení na typ souboru; prázdné = všechny.
- `Template`: text šablony; podporuje `\n`, `\t`, `\r`, `\\`.
- `Shortcut` (výchozí prázdné): volitelná klávesová zkratka. Použijte formát jako `Ctrl+1`, `Ctrl+Shift+C` nebo `Alt+F2`. Názvy kláves se zapisují anglicky. Podporované názvy kláves: písmena A–Z, čísla 0–9, F1–F12 a klávesy jako `Enter` (potvrzení), `Space` (mezerník), `Tab` (tabulátor), `Backspace` (zpětné mazání), `Delete` (smazat), `Insert` (vložit), `Home` (začátek), `End` (konec), `PageUp`/`PageDown` (o stránku) a šipky. Zkratky v konfliktu s vestavěnými příkazy jsou ignorovány; pokud si nejste jistí názvy kláves, podívejte se na ukázky v `RichEditor.ini`.

### [MRU]

MRU je seznam naposledy použitých souborů. `File1` je nejnovější položka, poté `File2`, `File3` atd. Délka seznamu závisí na použití.

### [FindHistory] / [ReplaceHistory]

Každá sekce obsahuje:

- `Count`: počet položek historie.
- `Item1`, `Item2`, ...: nejnovější položka je vždy `Item1`.

### [Resume]

Data obnovy autosave:

- `ResumeFile`: cesta k souboru obnovení.
- `OriginalPath`: původní cesta (prázdné pro nepojmenované soubory).

Soubory obnovení jsou standardně v `%TEMP%\RichEditor\` (dočasná složka systému).

## RichEdit knihovna (pokročilé)

RichEditor umí načítat novější knihovny RichEdit (DLL) pro lepší přístupnost a vykreslování. Windows 11 Poznámkový blok používá moderní RichEdit s lepším chováním kurzoru a výběru v prostém textu (zejména u konců řádků a koncových mezer) a s podporou DirectWrite a UI Automation pro barevná emoji a čtečky obrazovky.

Můžete použít knihovnu RichEdit ze sady Microsoft Office (např. Office 2013+), která často obsahuje novější opravy než systémová knihovna. K tomu slouží `RichEditLibraryPath` a `RichEditClassName` v INI. Pro Windows Poznámkový blok může cesta vypadat například takto: `C:\Program Files\WindowsApps\Microsoft.WindowsNotepad_11.x.x.x_x64__8wekyb3d8bbwe\Notepad\riched20.dll`.

## Příkazová řádka (pokročilé)

```
RichEditor.exe [volby] [soubor]

/nomru     Otevřít bez přidání do MRU (seznam nedávných souborů)
/readonly  Otevřít v režimu pouze pro čtení
```

## Klávesové zkratky

Názvy kláves jsou uvedeny podle popisků na klávesnici (Ctrl, Shift, Alt, Enter apod.).

### Zkratky aplikace

| Zkratka | Akce |
| --- | --- |
| Ctrl+N | Nový |
| Ctrl+O | Otevřít |
| Ctrl+S | Uložit |
| Ctrl+Z | Zpět |
| Ctrl+Y | Znovu |
| Ctrl+X | Vyjmout |
| Ctrl+C | Kopírovat |
| Ctrl+V | Vložit |
| Ctrl+A | Vybrat vše |
| Ctrl+W | Zalamování řádků |
| F3 | Najít další |
| Shift+F3 | Najít předchozí |
| Ctrl+F | Najít |
| Ctrl+H | Nahradit |
| Ctrl+G | Přejít na řádek |
| Ctrl+F2 | Přepnout záložku |
| F2 | Další záložka |
| Shift+F2 | Předchozí záložka |
| F5 | Čas a datum |
| Ctrl+Enter | Spustit filtr |
| Ctrl+Shift+T | Výběr šablon |
| Ctrl+Shift+I | Spustit interaktivní režim |
| Ctrl+Shift+Q | Ukončit interaktivní režim |

### Zkratky RichEdit

Tyto zkratky poskytuje samotný RichEdit (editovací komponenta):

| Zkratka | Akce |
| --- | --- |
| Alt+X | Převod Unicode hex na znak (a zpět) |
| Alt+Shift+X | Převod znaku na Unicode hex |
| Ctrl+Left/Right | Pohyb o slovo |
| Ctrl+Up/Down | Pohyb po odstavcích |
| Ctrl+Home/End | Začátek/konec dokumentu |
| Ctrl+Page Up/Down | Pohyb o stránku |
| Shift+Arrow | Rozšířit výběr |
| Ctrl+Shift+Arrow | Rozšířit výběr o slovo/odstavec |
| Ctrl+Backspace | Smazat předchozí slovo |
| Ctrl+Delete | Smazat další slovo |
| Shift+Delete | Vyjmout |
| Shift+Insert | Vložit |
| Insert | Přepnout přepisování |
