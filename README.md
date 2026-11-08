# Cztery w rzędzie

Otwórz PLAN.html w przeglądarce. Przewodnik zawiera sześć zadań, pełny kod, objaśnienia linii i schemat architektury. Nie wymaga internetu.

Kod jest w katalogu src. Otwórz tam terminal PowerShell i wykonaj:

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic Board.cpp Strategy.cpp tests.cpp -o tests.exe
.\tests.exe
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic Board.cpp Strategy.cpp Game.cpp Experiments.cpp main.cpp -o connect4.exe
.\connect4.exe
```

Potrzebny jest kompilator C++17. Sprawdzono GCC 15.2.0 na Windows. Nie kompiluj wszystkich plików przez *.cpp, bo tests.cpp i main.cpp mają oddzielne funkcje main.

Menu 1 uruchamia grę. Dla każdego gracza wybierz człowieka albo AI. W AI kontra AI naciśnij Enter przed każdym ruchem. Po zakończeniu dowolnego zadania wynik pozostaje na ekranie. Enter wraca do menu, a dopiero 0 zamyka program.

Menu 2 wykonuje pomiary Minimaxu i zapisuje pomiary_minimax.ini. Menu 3 uruchamia automatyczny turniej i zapisuje turniej_podsumowanie.ini oraz turniej_ruchy.ini. Otwórz raporty w edytorze tekstu. Mają sekcje i nazwane pola w stylu INI. Pliki zastępują poprzednie wyniki o tych samych nazwach w katalogu uruchomienia.

Projekt nie zawiera gotowego sprawozdania z wymyślonymi wynikami. Badania na własnym komputerze i oddanie materiałów opisuje przewodnik. Brak pisemnej punktacji nie pozwala gwarantować konkretnej oceny.
