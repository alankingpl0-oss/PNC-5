#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* PNC5 Edit v2.2, licencja GPL 2.0 */

/* * Tabela kodowania PNC5 (32 mozliwosci):
 * 0-25: A-Z
 * 26: Spacja
 * 27: Kropka
 * 28: Przecinek
 * 29: Znak nowej linii (\n)
 * 30: Znak zapytania (?)
 * 31: Marker RLE / Koniec pliku (EOFT)
 */

#define MARKER_RLE 31

unsigned char bufor[8192];
size_t bity_w_buforze = 0;

unsigned char ascii_na_pnc5(char c) {
    /* Używamy wstawki AT&T/GCC inline assembly, która pod maską 
       generuje czyste instrukcje kompatybilne z NASM dla x86_64.
     */
    unsigned char wynik;
    unsigned char temp = (unsigned char)c;

    /* Najpierw toupper w C, żeby ułatwić sprawę */
    if (temp >= 'a' && temp <= 'z') {
        temp -= 32;
    }

    __asm__ __volatile__ (
        "movb %[c], %%al\n\t"         /* Załaduj znak do rejestru AL */
        "cmpb $'A', %%al\n\t"         /* Sprawdź czy >= 'A' */
        "jb .L_not_alpha\n\t"
        "cmpb $'Z', %%al\n\t"         /* Sprawdź czy <= 'Z' */
        "ja .L_not_alpha\n\t"
        "subb $'A', %%al\n\t"         /* c - 'A' */
        "jmp .L_done\n\t"
        
    ".L_not_alpha:\n\t"
        "cmpb $' ', %%al\n\t"         /* Spacja -> 26 */
        "jne .L_check_dot\n\t"
        "movb $26, %%al\n\t"
        "jmp .L_done\n\t"
        
    ".L_check_dot:\n\t"
        "cmpb $'.', %%al\n\t"         /* Kropka -> 27 */
        "jne .L_check_comma\n\t"
        "movb $27, %%al\n\t"
        "jmp .L_done\n\t"
        
    ".L_check_comma:\n\t"
        "cmpb $',', %%al\n\t"         /* Przecinek -> 28 */
        "jne .L_check_newline\n\t"
        "movb $28, %%al\n\t"
        "jmp .L_done\n\t"
        
    ".L_check_newline:\n\t"
        "cmpb $'\n', %%al\n\t"        /* Nowa linia -> 29 */
        "jne .L_check_question\n\t"
        "movb $29, %%al\n\t"
        "jmp .L_done\n\t"
        
    ".L_check_question:\n\t"
        "cmpb $'?', %%al\n\t"         /* Znak zapytania -> 30 */
        "jne .L_default\n\t"
        "movb $30, %%al\n\t"
        "jmp .L_done\n\t"
        
    ".L_default:\n\t"
        "movb $31, %%al\n\t"          /* Wszystko inne -> MARKER_RLE */
        
    ".L_done:\n\t"
        "movb %%al, %[wynik]\n\t"     /* Zapisz rejestr AL do zmiennej wyjściowej */
        : [wynik] "=rm" (wynik)
        : [c] "rm" (temp)
        : "eax"
    );

    return wynik;
}


char pnc5_na_ascii(unsigned char p) {
    if (p <= 25) return 'A' + p;
    if (p == 26) return ' ';
    if (p == 27) return '.';
    if (p == 28) return ',';
    if (p == 29) return '\n';
    if (p == 30) return '?';
    return '*'; 
}

void zapisz_5_bitow(unsigned char wartosc) {
    size_t bajt_idx = bity_w_buforze / 8;
    size_t bit_offset = bity_w_buforze % 8;
    size_t miejsce_w_bajcie = 8 - bit_offset;

    if (bajt_idx >= sizeof(bufor) - 1) return;

    if (bit_offset == 0) {
        bufor[bajt_idx] = 0;
        bufor[bajt_idx + 1] = 0;
    }

    wartosc &= 0x1F;

    if (miejsce_w_bajcie >= 5) {
        bufor[bajt_idx] |= (wartosc << (miejsce_w_bajcie - 5));
    } else {
        size_t nadmiar = 5 - miejsce_w_bajcie;
        bufor[bajt_idx] |= (wartosc >> nadmiar);
        bufor[bajt_idx + 1] |= (wartosc << (8 - nadmiar));
    }
    bity_w_buforze += 5;
}

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

void drukuj_bufor() {
    size_t b = 0;
    while (b + 5 <= bity_w_buforze) {
        unsigned char kod = czytaj_5_bitow(b);
        b += 5;

        if (kod == MARKER_RLE) {
            if (b + 10 <= bity_w_buforze) {
                unsigned char nastepny = czytaj_5_bitow(b);
                unsigned char licznik = czytaj_5_bitow(b + 5);
                b += 10;

                if (nastepny == MARKER_RLE) {
                    break;
                }

                int powtorzenia = licznik + 3;
                char c = pnc5_na_ascii(nastepny);
                for (int i = 0; i < powtorzenia; i++) {
                    putchar(c);
                }
            } else {
                break;
            }
        } else {
            putchar(pnc5_na_ascii(kod));
        }
    }
}

/* * Poprawiona funkcja kompresji RLE. 
 * Wprowadzono zmienna pomocnicza, dzieki czemu indeks glowny 'i' inkrementuje sie prawidlowo.
 */
void kompresuj_i_zapisz_ciag(const char *wiersz) {
    int i = 0;
    while (wiersz[i] != '\0') {
        int dlugosc = 1;
        /* Porownujemy wielkie litery, by 'A' i 'a' wpadaly do jednej serii RLE */
        while (wiersz[i + dlugosc] != '\0' && toupper((unsigned char)wiersz[i]) == toupper((unsigned char)wiersz[i + dlugosc])) {
            dlugosc++;
        }

        unsigned char kod = ascii_na_pnc5(wiersz[i]);
        int pozostalo = dlugosc;

/* TO WSTAWIAMY: */
if (pozostalo >= 4 && kod != 31) {
    while (pozostalo >= 4) {
        int chunk = (pozostalo > 34) ? 34 : pozostalo;
        /* Jeśli po odjęciu max chunk zostanie nam np. 1, 2 lub 3 znaki,
           to pętla while się skończy, a te resztki dopisze dolna pętla 'for'. */
        zapisz_5_bitow(MARKER_RLE);
        zapisz_5_bitow(kod);
        zapisz_5_bitow(chunk - 3);
        pozostalo -= chunk;
    }
    /* Dopisanie resztek (0-3 znaków), dla których RLE byłoby nieopłacalne */
    for (int j = 0; j < pozostalo; j++) {
        zapisz_5_bitow(kod);
    }
}

        i += dlugosc; /* Indeks przesuwa sie teraz o faktyczna liczbe znakow */
    }
}

void zapisz_do_pliku(const char *nazwa_pliku) {
    FILE *f = fopen(nazwa_pliku, "wb");
    if (!f) {
        printf("?\n");
        return;
    }

    zapisz_5_bitow(MARKER_RLE);
    zapisz_5_bitow(MARKER_RLE);

    size_t bajty_do_zapisu = (bity_w_buforze + 7) / 8;
    if (bajty_do_zapisu > 0) {
        fwrite(bufor, 1, bajty_do_zapisu, f);
    }

    fclose(f);
    printf("%zu bajtow zapisanego pliku\n", bajty_do_zapisu);
    
    bity_w_buforze -= 10;

    /* Czyszczenie bitow po markerze EOFT, by uniknac interferencji przy nastepnym 'a' */
    for (size_t i = bity_w_buforze; i < bity_w_buforze + 10; i++) {
        size_t b_idx = i / 8;
        size_t b_off = i % 8;
        bufor[b_idx] &= ~(1 << (7 - b_off));
    }
}

void wczytaj_z_pliku(const char *nazwa_pliku) {
    FILE *f = fopen(nazwa_pliku, "rb");
    if (!f) {
        printf("?\n");
        return;
    }

    memset(bufor, 0, sizeof(bufor));
    size_t przeczytane_bajty = fread(bufor, 1, sizeof(bufor), f);
    fclose(f);

    if (przeczytane_bajty == 0) {
        bity_w_buforze = 0;
        return;
    }

    bity_w_buforze = przeczytane_bajty * 8;
    printf("%zu bajtow wczytane\n", przeczytane_bajty);
}

void wyciagnij_nazwe_pliku(const char *komenda, char *nazwa_wyjsciowa) {
    int i = 1;
    while (komenda[i] == ' ' || komenda[i] == '\t') {
        i++;
    }
    strcpy(nazwa_wyjsciowa, komenda + i);
    
    size_t len = strlen(nazwa_wyjsciowa);
    if (len > 0 && nazwa_wyjsciowa[len - 1] == '\n') {
        nazwa_wyjsciowa[len - 1] = '\0';
    }

    if (strlen(nazwa_wyjsciowa) == 0) {
        strcpy(nazwa_wyjsciowa, "tekst.pnc5");
    }
}

void pokaz_statystyki() {
    size_t bajty = (bity_w_buforze + 7) / 8;
    double procent_zajetosci = ((double)bity_w_buforze / (sizeof(bufor) * 8)) * 100.0;
    
    printf("\n=== STATYSTYKI BUFORA ===\n");
    printf("Zapisane bity:    %zu\n", bity_w_buforze);
    printf("Zajęte bajty:     %zu / %zu\n", bajty, sizeof(bufor));
    printf("Stan zapełnienia: %.2f%%\n", procent_zajetosci);
    printf("=========================\n\n");
}



int main() {
    char komenda[256];
    printf("Edytor PNC5 1.00. Komendy: a, p, w [plik], r [plik], s, q.\n");

    while (1) {
        printf("* ");
        if (!fgets(komenda, sizeof(komenda), stdin)) break;

        if (komenda[0] == 'q') {
            break;
        } else if (komenda[0] == 'p') {
            drukuj_bufor();
        } else if (komenda[0] == 's') {
            pokaz_statystyki();
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
                if (wiersz[0] == '.' && wiersz[1] == '\n') break;
                kompresuj_i_zapisz_ciag(wiersz);
            }
        } else {
            printf("?\n");
        }
    }
    return 0;
}