# Github

https://github.com/LostInTheLogs/drone-swarm-simulation

# Opis zadania

Rój autonomicznych dronów liczy początkowo $N$ egzemplarzy. Drony startują (i lądują)
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

## 1. Zwiększanie/zmniejszanie dynamiczne maksymalnej liczby dronów

```
[2026-01-01 18:01:07.925495028] DEBUG operator(994731): GlobalParameters:
[2026-01-01 18:01:07.925503199] DEBUG operator(994731):   scenario = 1
[2026-01-01 18:01:07.925508437] DEBUG operator(994731):   N=init_drone_count = 4
[2026-01-01 18:01:07.925513187] DEBUG operator(994731):   P=max_drones_at_base = 2
[2026-01-01 18:01:07.925517866] DEBUG operator(994731):   Xi=max_charges = 0
[2026-01-01 18:01:07.925523104] DEBUG operator(994731):   T2=battery_lifetime = 500 ms
[2026-01-01 18:01:07.925527923] DEBUG operator(994731):   T1=battery_chargetime = 500 ms
[2026-01-01 18:01:07.925532323] DEBUG operator(994731):   tunnel_length = 1 ms
[2026-01-01 18:01:07.925537491] DEBUG operator(994731):   tun_cap = 10
[2026-01-01 18:01:07.928010297] DEBUG operator(994733): Spawning new drone
[2026-01-01 18:01:07.928059046] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:08.028220520] WARN  operator(994733): Drone count: 4
[2026-01-01 18:01:08.028239377] WARN  operator(994733): Increasing drone count
[2026-01-01 18:01:08.028255720] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:09.028362929] WARN  operator(994733): Drone count: 8
[2026-01-01 18:01:09.228495000] WARN  operator(994733): Drone count: 8
[2026-01-01 18:01:09.228510854] WARN  operator(994733): Increasing drone count
[2026-01-01 18:01:09.228522099] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:10.228679594] WARN  operator(994733): Drone count: 8
[2026-01-01 18:01:10.428801468] WARN  operator(994733): Drone count: 8
[2026-01-01 18:01:10.428812922] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:10.428820884] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:11.428908609] WARN  operator(994733): Drone count: 4
[2026-01-01 18:01:11.629005899] WARN  operator(994733): Drone count: 4
[2026-01-01 18:01:11.629030762] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:11.629044032] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:12.629176805] WARN  operator(994733): Drone count: 2
[2026-01-01 18:01:12.829361536] WARN  operator(994733): Drone count: 2
[2026-01-01 18:01:12.829382978] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:12.829396736] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:13.830102279] WARN  operator(994733): Drone count: 1
[2026-01-01 18:01:14.030277023] WARN  operator(994733): Drone count: 1
[2026-01-01 18:01:14.030297976] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:14.030308522] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:15.030436686] WARN  operator(994733): Drone count: 1
[2026-01-01 18:01:15.230569106] WARN  operator(994733): Drone count: 1
```

## 2. Dron z niższym poziomem baterii ma pierwszeństwo w kolejce

```
[2026-01-01 19:40:17.015630845] DEBUG operator(1017166): GlobalParameters:
[2026-01-01 19:40:17.015640204] DEBUG operator(1017166):   scenario = 2
[2026-01-01 19:40:17.015644883] DEBUG operator(1017166):   N=init_drone_count = 10
[2026-01-01 19:40:17.015649143] DEBUG operator(1017166):   P=max_drones_at_base = 100
[2026-01-01 19:40:17.015653404] DEBUG operator(1017166):   Xi=max_charges = 0
[2026-01-01 19:40:17.015657734] DEBUG operator(1017166):   T2=battery_lifetime = 10000 ms
[2026-01-01 19:40:17.015662134] DEBUG operator(1017166):   T1=battery_chargetime = 10000 ms
[2026-01-01 19:40:17.015665975] DEBUG operator(1017166):   tunnel_length = 200 ms
[2026-01-01 19:40:17.015670235] DEBUG operator(1017166):   tun_cap = 1
[2026-01-01 19:40:17.019857648] TRACE drone(1017173): Added to queue, priority 85
[2026-01-01 19:40:17.019878391] TRACE drone(1017173): Left the queue
[2026-01-01 19:40:17.020225643] TRACE drone(1017175): Added to queue, priority 84
[2026-01-01 19:40:17.020246176] TRACE drone(1017175): Left the queue
[2026-01-01 19:40:17.020303516] TRACE drone(1017172): Added to queue, priority 84
[2026-01-01 19:40:17.020359320] TRACE drone(1017174): Added to queue, priority 87
[2026-01-01 19:40:17.020405485] TRACE drone(1017171): Added to queue, priority 89
[2026-01-01 19:40:17.020458285] TRACE drone(1017176): Added to queue, priority 87
[2026-01-01 19:40:17.020512621] TRACE drone(1017177): Added to queue, priority 82
[2026-01-01 19:40:17.020742609] TRACE drone(1017178): Added to queue, priority 83
[2026-01-01 19:40:17.020919238] TRACE drone(1017179): Added to queue, priority 85
[2026-01-01 19:40:17.021241207] TRACE drone(1017180): Added to queue, priority 85
[2026-01-01 19:40:17.220825193] TRACE drone(1017171): Left the queue
[2026-01-01 19:40:17.270870563] TRACE drone(1017174): Left the queue
[2026-01-01 19:40:17.271939487] TRACE drone(1017218): Added to queue, priority 89
[2026-01-01 19:40:17.271973150] TRACE drone(1017219): Added to queue, priority 90
[2026-01-01 19:40:17.422255997] TRACE drone(1017219): Left the queue
[2026-01-01 19:40:17.472333703] TRACE drone(1017218): Left the queue
[2026-01-01 19:40:17.472757920] TRACE drone(1017233): Added to queue, priority 85
[2026-01-01 19:40:17.671603682] TRACE drone(1017176): Left the queue
[2026-01-01 19:40:17.722107701] TRACE drone(1017179): Left the queue
[2026-01-01 19:40:17.872815532] TRACE drone(1017180): Left the queue
[2026-01-01 19:40:17.923598637] TRACE drone(1017233): Left the queue
[2026-01-01 19:40:18.122503835] TRACE drone(1017172): Left the queue
[2026-01-01 19:40:18.172790367] TRACE drone(1017178): Left the queue
[2026-01-01 19:40:18.323120147] TRACE drone(1017177): Left the queue
```

## 3. Dron poprawnie dostaje polecenie ataku samobójczego w locie i w bazie

- \>= 20%
- < 20%

## 4. Dron tracący beterię w wejściu do bazy nie blokuje wejscia

## 5. Wejścia poprawnie zmieniaja kierunek
