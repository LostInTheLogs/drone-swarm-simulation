# Github

https://github.com/LostInTheLogs/drone-swarm-simulation

# Opis zadania

Rój autonomicznych dronów liczy początkowo N egzemplarzy. Drony startują (i lądują)
z ukrytej platformy (bazy), na której w danym momencie może znajdować
się co najwyżej $P (P < N/2)$ dronów.

Dron, który chce wrócić do bazy, musi wlecieć przez jedno z dwóch istniejących
wejść. Wejścia te są bardzo wąskie, więc możliwy jest w nich jedynie ruch w
jedną stronę w danej chwili czasu.

Zbyt długie przebywanie w bazie – ładowanie baterii – grozi jej przegrzaniem,
dlatego każdy z dronów opuszcza bazę po pewnym skończonym czasie $T_{1i}$.

Jedno pełne ładowanie wystarcza na lot, który maksymalnie może trwać $T_{2i}
(T_{2i} = 2.5 \cdot T_{1i})$. Przy poziomie naładowania baterii 20% dron
automatycznie rozpoczyna powrót do bazy. Jeżeli w trakcie lotu poziom
naładowania baterii osiągnie 0%, dron ulega zniszczeniu.

Znajdujący się w bazie operator co pewien czas $T_k$ stara się uzupełnić
braki w liczbie dronów, pod warunkiem, że w bazie jest wystarczająca ilość
miejsca.

Dowódca systemu może dołożyć (sygnał1 do operatora) dodatkowe platformy
startowe, które pozwalają zwiększyć liczbę dronów maksymalnie do $2 \cdot N$
egzemplarzy. Może również zdemontować (sygnał2 do operatora) platformy
startowe, ograniczając bieżącą maksymalną liczbę egzemplarzy o 50%.

Dowódca systemu może do danego drona (nawet jeśli jest w bazie w trakcie
ładowania) wysłać polecenie wykonania ataku samobójczego (sygnał3). Jeżeli
poziom naładowania baterii jest niższy niż 20%, dron ignoruje sygnał3.

Napisz program dowódcy systemu, operatora i dronów tak, aby zasymulować cykl
życia roju dronów. Każdy z dronów jest utylizowany (wycofywany z eksploatacji)
po pewnym określonym czasie $X_i$, liczonym w ilościach ładowań (pobytów w
bazie).

Raport z przebiegu symulacji zapisać w pliku (plikach tekstowych).

# Testy

TODO

