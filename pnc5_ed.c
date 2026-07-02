#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* Tabela kodowania PNC5 (32 mozliwosci):
   0-25: A-Z
   26: Spacja
   27: Kropka
   28: Przecinek
   29: Znak nowej linii (\n)
   30: Znak zapytania (?)
   31: Koniec pliku / Nieznany znak
*/

/* Bufor na spakowane dane i licznik bitow */
unsigned char bufor[8192];
size_t bity_w_buforze = 0;

/* Konwersja standardowego znaku ASCII na 5-bitowe PNC5 */
unsigned char ascii_na_pnc5(char c) {
    c = toupper(c);
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c == ' ') return 26;
    if (c == '.') return 27;
    if (c == ',') return 28;
    if (c == '\n') return 29;
    if (c == '?') return 30;
    return 31; /* Domyslna wartosc dla nieobslugiwanych znakow */
}

/* Konwersja z 5-bitowego PNC5 z powrotem na ASCII do wyswietlenia */
char pnc5_na_ascii(unsigned char p) {
    if (p <= 25) return 'A' + p;
    if (p == 26) return ' ';
    if (p == 27) return '.';
    if (p == 28) return ',';
    if (p == 29) return '\n';
    if (p == 30) return '?';
    return '*'; 
}

/* Pakowanie 5 bitow do globalnego bufora pamieci */
void zapisz_5_bitow(unsigned char wartosc) {
    size_t bajt_idx = bity_w_buforze / 8;
    size_t bit_offset = bity_w_buforze % 8;
    size_t miejsce_w_bajcie = 8 - bit_offset;

    /* Zabezpieczenie przed przepelnieniem */
    if (bajt_idx >= sizeof(bufor) - 1) return;

    /* Inicjalizacja nowego bajtu zerami */
    if (bit_offset == 0) {
        bufor[bajt_idx] = 0;
        bufor[bajt_idx + 1] = 0;
    }

    wartosc &= 0x1F; /* Maska: upewniamy sie, ze to tylko 5 bitow (00011111) */

    if (miejsce_w_bajcie >= 5) {
        bufor[bajt_idx] |= (wartosc << (miejsce_w_bajcie - 5));
    } else {
        size_t nadmiar = 5 - miejsce_w_bajcie;
        bufor[bajt_idx] |= (wartosc >> nadmiar);
        bufor[bajt_idx + 1] |= (wartosc << (8 - nadmiar));
    }
    bity_w_buforze += 5;
}

/* Odczytywanie 5 bitow z ciaglego strumienia pamieci */
unsigned char czytaj_5_bitow(size_t pozycja_bitowa) {
    size_t bajt_idx = pozycja_bitowa / 8;
    size_t bit_offset = pozycja_bitowa % 8;
    size_t miejsce_w_bajcie = 8 - bit_offset;
    unsigned char wynik = 0;

    if (miejsce_w_bajcie >= 5) {
        wynik = (bufor[bajt_idx] >> (miejsce_w_bajcie - 5)) & 0x1F;
    } else {
        size_t nadmiar = 5 - miejsce_w_bajcie;
        wynik = (bufor[bajt_idx] & ((1 << miejsce_w_bajcie) - 1)) << nadmiar;
        wynik |= (bufor[bajt_idx + 1] >> (8 - nadmiar)) & ((1 << nadmiar) - 1);
    }
    return wynik;
}

/* Funkcja dekodujaca caly bufor i drukujaca zawartosc */
void drukuj_bufor() {
    size_t b = 0;
    while (b + 5 <= bity_w_buforze) {
        unsigned char znak_pnc5 = czytaj_5_bitow(b);
        putchar(pnc5_na_ascii(znak_pnc5));
        b += 5;
    }
}

/* Funkcja zapisujaca aktualny stan bufora do pliku *.pnc5 */
void zapisz_do_pliku(const char *nazwa_pliku) {
    FILE *f = fopen(nazwa_pliku, "wb");
    if (!f) {
        printf("?\n"); /* Tradycyjny blad ed */
        return;
    }

    /* Najpierw zapisujemy liczbe bitow jako naglowek, aby uniknac doczytywania zer na koncu bajtu */
    fwrite(&bity_w_buforze, sizeof(bity_w_buforze), 1, f);

    /* Obliczamy ile pelnych/czesciowych bajtow musimy zrzucic na dysk */
    size_t bajty_do_zapisu = (bity_w_buforze + 7) / 8;
    if (bajty_do_zapisu > 0) {
        fwrite(bufor, 1, bajty_do_zapisu, f);
    }

    fclose(f);
    printf("%zu bitow zapisane\n", bity_w_buforze);
}

/* Funkcja wczytujaca dane z pliku *.pnc5 do bufora */
void wczytaj_z_pliku(const char *nazwa_pliku) {
    FILE *f = fopen(nazwa_pliku, "rb");
    if (!f) {
        printf("?\n");
        return;
    }

    /* Odczytujemy naglowek z liczba bitow */
    if (fread(&bity_w_buforze, sizeof(bity_w_buforze), 1, f) != 1) {
        printf("?\n");
        fclose(f);
        return;
    }

    /* Obliczamy ile bajtow musimy przeczytac */
    size_t bajty_do_odczytu = (bity_w_buforze + 7) / 8;
    if (bajty_do_odczytu > sizeof(bufor)) {
        /* Zabezpieczenie przed przepelnieniem bufora statycznego */
        printf("?\n");
        bity_w_buforze = 0;
        fclose(f);
        return;
    }

    if (bajty_do_odczytu > 0) {
        fread(bufor, 1, bajty_do_odczytu, f);
    }

    fclose(f);
    printf("%zu bitow wczytane\n", bity_w_buforze);
}

/* Pomocnicza funkcja parsujaca nazwe pliku po komendzie 'w' lub 'r' */
void wyciagnij_nazwe_pliku(const char *komenda, char *nazwa_wyjsciowa) {
    int i = 1;
    while (komenda[i] == ' ' || komenda[i] == '\t') {
        i++;
    }
    strcpy(nazwa_wyjsciowa, komenda + i);
    
    /* Usuniecie znaku nowej linii z konca lancucha */
    size_t len = strlen(nazwa_wyjsciowa);
    if (len > 0 && nazwa_wyjsciowa[len - 1] == '\n') {
        nazwa_wyjsciowa[len - 1] = '\0';
    }

    /* Jesli nie podano nazwy, uzywamy domyslnej */
    if (strlen(nazwa_wyjsciowa) == 0) {
        strcpy(nazwa_wyjsciowa, "tekst.pnc5");
    }
}

/* Glowna funkcja edytora */
int main() {
    char komenda[256];
    printf("Edytor PNC5 (styl ed). Komendy: a (dodaj), p (wypisz), w [plik], r [plik], q (wyjdz).\n");

    while (1) {
        printf("* ");
        if (!fgets(komenda, sizeof(komenda), stdin)) break;

        if (komenda[0] == 'q') {
            break;
        } else if (komenda[0] == 'p') {
            drukuj_bufor();
        } else if (komenda[0] == 'w') {
            char nazwa_pliku[256];
            wyciagnij_nazwe_pliku(komenda, nazwa_pliku);
            zapisz_do_pliku(nazwa_pliku);
        } else if (komenda[0] == 'r') {
            char nazwa_pliku[256];
            wyciagnij_nazwe_pliku(komenda, nazwa_pliku);
            wczytaj_z_pliku(nazwa_pliku);
        } else if (komenda[0] == 'a') {
            char wiersz[256];
            while (1) {
                if (!fgets(wiersz, sizeof(wiersz), stdin)) break;
                
                /* Wyjscie z trybu wprowadzania kropka w nowej linii (jak w ed) */
                if (wiersz[0] == '.' && wiersz[1] == '\n') break;

                /* Konwersja znak po znaku i pakowanie w locie */
                for (int i = 0; wiersz[i] != '\0'; i++) {
                    zapisz_5_bitow(ascii_na_pnc5(wiersz[i]));
                }
            }
        } else {
            /* Tradycyjny komunikat bledu z edytora ed */
            printf("?\n");
        }
    }
    return 0;
}