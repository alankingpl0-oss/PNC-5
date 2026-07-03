# /* * Makefile zoptymalizowany pod kątem minimalnego rozmiaru pliku wynikowego.
#  * Cel: pkilo
#  */

CC = gcc

# Flagi kompilacji (Size-oriented):
# -Os: Optymalizacja pod kątem rozmiaru (w niektórych przypadkach -Oz działa jeszcze mocniej, ale -Os jest standardem GCC).
# -fomit-frame-pointer: Wywala wskaźnik ramki z rejestrów (oszczędza instrukcje push/pop).
# -fdata-sections -ffunction-sections: Pakuje każdą funkcję i zmienną do osobnej sekcji, żeby linker mógł wywalić nieużywane.
# -fno-asynchronous-unwind-tables: Wyłącza generowanie tabel do obsługi wyjątków (w czystym C zbędne).
CFLAGS = -Os \
         -fomit-frame-pointer \
         -fdata-sections \
         -ffunction-sections \
         -fno-asynchronous-unwind-tables \
         

# Flagi Linkera:
# -Wl,--gc-sections: Usuwa martwy kod (nieużywane funkcje posekcjonowane przez CFLAGS).
# -Wl,-z,norelro: Wyłącza pełne zabezpieczenia relokacji (oszczędza miejsce na nagłówki).
# -Wl,--build-id=none: Usuwa unikalny identyfikator kompilacji z nagłówka ELF.
# -s: Całkowity strip – usuwa wszystkie tabele symboli i informacje debugowania.
LDFLAGS = -Wl,--gc-sections \
          -Wl,-z,norelro \
          -Wl,--build-id=none \
          -s


TARGET = pnc5_ed
SRC = pnc5_ed.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS) $(LIBS)
	@echo "--- Rozmiar pliku binarnego ---"
	@wc -c $(TARGET)

clean:
	rm -f $(TARGET)
