# Github

[LostInTheLogs/drone-swarm-simulation](https://github.com/LostInTheLogs/drone-swarm-simulation)

# Jak uruchomić kod

```bash
cmake -B build && cd build && make
./DroneSwarm # symulacja
./commander # interfejs do zarządzania symulacją
```

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

# Opis kodu

## src/common

- kod wspólny dla wszystkich programów

## src/main (./DroneSwarm)

- tworzy (i na końcu usuwa) potrzebne struktury ipc
- startuje `loggera` i `operatora`

## src/logger

- tworzy (i na końcu usuwa) kolejke komunikatów
- czeka na komunikaty, wypisuje je na stdout i do pliku.

## src/operator

wątki:

- reaper: zbiera zniszczone drony
- signal:
    - **sig1**: zwiększa maksymalną ilość dronów 2x aż do $2N$
    - **sig2**: zmniejsza maksymalną ilość dronów 2x aż do $1$
- main:
    - uzupełnia braki dronów co 50ms (jeśli w bazie jest wolne miejsce)

## src/drone

wątki:

- battery: zmienia poziom baterii i budzi wątek główny gdy bateria jest niska lub się naładuje
- signal: czeka na sygnał do misji samobójczej i budzi wątek główny
- main:
    - czeka na zmiany stanu
    - po ładowaniu bateri ($T_1$) opuszcza bazę
    - powraca do bazy przy baterii < 20%
    - destrukcja przy baterii = 0%

## src/commander

- prosty interfejs do zarządzania symulacją

## Działanie wejść:

Wejścia mają miejsce tylko na x dronów, są jednostronne w danym momencie czasu i przejscie przez nie trwa y czasu.

Wchodzenie/wychodzenie z bazy wygląda tak:

1. dron ustawia się w kolejce z priorytetem 100-poziom_baterii (kolejka max heap w shared mem)
2. czeka aż będzie pierwszy w kolejce (pthread_cond):

    - jeśli podczas czekania dostał polecenia ataku samobójczego i kierunek == wejście lub dostał SIGINT/SIGTERM to usuwa się z kolejki i kończy procedure
    - jeśli jest pierwszy w kolejce:
    - jeśli kierunek == wejście to zajmuje miejsce w bazie (wait na semaforze), jeśli podczas czekania... (tak jak wyżej)
    - czeka aż wejście do bazy będzie wolne (pthread_cond), jeśli podczas czekania... (tak jak wyżej)
    - jeśli nie jest już pierwszy w kolejce, zwalnia miejsce w bazie i wraca do 2.
    - próbuje wejśc do wejścia 1/2
        - jeśli się nie udało to wraca do czekania aż wejście będzie wolne
        - usuwa się z kolejki

3. czeka aż przejdzie przez wejście (sleep(długość_tunelu))
    - wychodzi z tunelu
    - jeśli dostał SIGINT/SIGTERM podczas snu zwalnia miejsce w bazie i kończy procedure
    - jeśli kierunek==wyjście to zwalnia miejsce w bazie

W kodzie kroki 1 i 2 są w funkcji WaitForTunInQueue i EnterOneTunnel:
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/679f54510701769ca633ce9e2a9d824984d2fb94/src/drone/drone.cpp#L165-L302>

A Krok 3 w EnterExitSequence, która na początku wywołuje WaitForTunInQueue
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/679f54510701769ca633ce9e2a9d824984d2fb94/src/drone/drone.cpp#L304-L375>

# Testy

## 1. Zwiększanie/zmniejszanie dynamiczne maksymalnej liczby dronów

Sprawdamy, czy operator poprawnie zwieksza i zmniejsza maksymalną liczbe dronów.

Symulacja startuje z 4 dronami:

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
[2026-01-01 18:01:07.928059046] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:08.028220520] WARN  operator(994733): Drone count: 4
```

Zwiększamy ilość dronów do 8 (sig1):

```
[2026-01-01 18:01:08.028239377] WARN  operator(994733): Increasing drone count
[2026-01-01 18:01:08.028255720] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:09.028362929] WARN  operator(994733): Drone count: 8
```

Zwiększamy ilość dronów, tym razem nie powinna się zwiększyć:

```
[2026-01-01 18:01:09.228510854] WARN  operator(994733): Increasing drone count
[2026-01-01 18:01:09.228522099] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:10.228679594] WARN  operator(994733): Drone count: 8
```

Zmienjszamy ilość dronów do 4 (sig1):

```
[2026-01-01 18:01:10.428812922] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:10.428820884] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:11.428908609] WARN  operator(994733): Drone count: 4
```

Zmienjszamy ilość dronów do 2 (sig1):

```
[2026-01-01 18:01:11.629030762] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:11.629044032] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:12.629176805] WARN  operator(994733): Drone count: 2
```

Zmienjszamy ilość dronów do 1 (sig1):

```
[2026-01-01 18:01:12.829382978] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:12.829396736] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:13.830102279] WARN  operator(994733): Drone count: 1
```

Zmienjszamy ilość dronów, tym razem nie powinna się zmienszyć:

```
[2026-01-01 18:01:14.030297976] WARN  operator(994733): Decreasing drone count
[2026-01-01 18:01:14.030308522] WARN  operator(994733): Waiting for the drone count to stablilize...
[2026-01-01 18:01:15.030436686] WARN  operator(994733): Drone count: 1
```

## 2. Dron z niższym poziomem baterii ma pierwszeństwo w kolejce

Symulacja startuje z 10 dronami z losowo niską bateria poza bazą. Maksymalna liczba dronów w wejściach: 1.

```
[2026-01-11 11:31:10.844882868] DEBUG operator(674786): GlobalParameters:
[2026-01-11 11:31:10.844912917] DEBUG operator(674786):   scenario = 2
[2026-01-11 11:31:10.844926961] DEBUG operator(674786):   N=init_drone_count = 10
[2026-01-11 11:31:10.844943359] DEBUG operator(674786):   P=max_drones_at_base = 100
[2026-01-11 11:31:10.844962371] DEBUG operator(674786):   Xi=max_charges = 0
[2026-01-11 11:31:10.845011058] DEBUG operator(674786):   T2=battery_lifetime = 10000 ms
[2026-01-11 11:31:10.845030971] DEBUG operator(674786):   T1=battery_chargetime = 10000 ms
[2026-01-11 11:31:10.845046145] DEBUG operator(674786):   tunnel_length = 200 ms
[2026-01-11 11:31:10.845061680] DEBUG operator(674786):   tun_cap = 1
```

Pierwszy dron od razu wychodzi z kolejki, bo ma wolny tunel

```
[2026-01-11 11:31:10.860757442] INFO  drone(674792): Returning to the base
[2026-01-11 11:31:10.860793791] TRACE drone(674792): Added to queue, priority 89
[2026-01-11 11:31:10.860864895] TRACE drone(674792): Entering tun 2 dir: in
[2026-01-11 11:31:10.860878926] TRACE drone(674792): Left the queue
```

Drugi dron od razu wychodzi z kolejki, bo ma wolny tunel

```
[2026-01-11 11:31:10.862230187] INFO  drone(674794): Returning to the base
[2026-01-11 11:31:10.862289495] TRACE drone(674794): Added to queue, priority 82
[2026-01-11 11:31:10.862352771] TRACE drone(674794): Entering tun 1 dir: in
[2026-01-11 11:31:10.862365445] TRACE drone(674794): Left the queue
```

Reszta dronów czeka aż tunel się zwolni:

```
[2026-01-11 11:31:10.863736634] INFO  drone(674793): Returning to the base
[2026-01-11 11:31:10.863786322] TRACE drone(674793): Added to queue, priority 85 # nr 4

[2026-01-11 11:31:10.865172375] INFO  drone(674796): Returning to the base
[2026-01-11 11:31:10.865244427] TRACE drone(674796): Added to queue, priority 87 # nr 3

[2026-01-11 11:31:10.865443221] INFO  drone(674795): Returning to the base
[2026-01-11 11:31:10.865483045] TRACE drone(674795): Added to queue, priority 88 # nr 1

[2026-01-11 11:31:10.869294318] INFO  drone(674797): Returning to the base
[2026-01-11 11:31:10.869349979] TRACE drone(674797): Added to queue, priority 84 # nr 5

[2026-01-11 11:31:10.869868536] INFO  drone(674799): Returning to the base
[2026-01-11 11:31:10.869915411] TRACE drone(674799): Added to queue, priority 81 # nr 7

[2026-01-11 11:31:10.870327059] INFO  drone(674801): Returning to the base
[2026-01-11 11:31:10.870362117] TRACE drone(674801): Added to queue, priority 82 # nr 6

[2026-01-11 11:31:10.871727994] INFO  drone(674798): Returning to the base
[2026-01-11 11:31:10.871772665] TRACE drone(674798): Added to queue, priority 81 # nr 8

[2026-01-11 11:31:10.873482745] INFO  drone(674800): Returning to the base
[2026-01-11 11:31:10.873519486] TRACE drone(674800): Added to queue, priority 88 # nr 2
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 1) opuszcza kolejkę

```
[2026-01-11 11:31:11.061001389] INFO  drone(674792): Back on the charging pad

[2026-01-11 11:31:11.061171435] TRACE drone(674795): Entering tun 2 dir: in
[2026-01-11 11:31:11.061244468] TRACE drone(674795): Left the queue
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 2) opuszcza kolejkę

```
[2026-01-11 11:31:11.062456088] INFO  drone(674794): Back on the charging pad

[2026-01-11 11:31:11.062582719] TRACE drone(674800): Entering tun 1 dir: in
[2026-01-11 11:31:11.062670104] TRACE drone(674800): Left the queue
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 3) opuszcza kolejkę

```
[2026-01-11 11:31:11.261382790] INFO  drone(674795): Back on the charging pad

[2026-01-11 11:31:11.261460847] TRACE drone(674796): Entering tun 2 dir: in
[2026-01-11 11:31:11.261487321] TRACE drone(674796): Left the queue
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 4) opuszcza kolejkę

```
[2026-01-11 11:31:11.262754818] INFO  drone(674800): Back on the charging pad

[2026-01-11 11:31:11.262810190] TRACE drone(674793): Entering tun 1 dir: in
[2026-01-11 11:31:11.262833044] TRACE drone(674793): Left the queue
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 5) opuszcza kolejkę

```
[2026-01-11 11:31:11.461607945] INFO  drone(674796): Back on the charging pad

[2026-01-11 11:31:11.461705447] TRACE drone(674797): Entering tun 2 dir: in
[2026-01-11 11:31:11.461725915] TRACE drone(674797): Left the queue
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 6) opuszcza kolejkę

```
[2026-01-11 11:31:11.462943678] INFO  drone(674793): Back on the charging pad

[2026-01-11 11:31:11.463009210] TRACE drone(674801): Entering tun 1 dir: in
[2026-01-11 11:31:11.463029923] TRACE drone(674801): Left the queue
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 7) opuszcza kolejkę

```
[2026-01-11 11:31:11.661838861] INFO  drone(674797): Back on the charging pad

[2026-01-11 11:31:11.661919654] TRACE drone(674799): Entering tun 2 dir: in
[2026-01-11 11:31:11.661937070] TRACE drone(674799): Left the queue
```

Tunel się zwolnił i dron z najwyższym priorytetem (nr 8) opuszcza kolejkę

```
[2026-01-11 11:31:11.663136361] INFO  drone(674801): Back on the charging pad

[2026-01-11 11:31:11.663211133] TRACE drone(674798): Entering tun 1 dir: in
[2026-01-11 11:31:11.663222335] TRACE drone(674798): Left the queue
```

## 3. Dron poprawnie dostaje polecenie ataku samobójczego w locie i w bazie

Symulacja startuje z dwoma dronami z niską baterią w bazie:

```
[2026-01-11 12:04:47.908036991] DEBUG operator(683446): GlobalParameters:
[2026-01-11 12:04:47.908066885] DEBUG operator(683446):   scenario = 3
[2026-01-11 12:04:47.908080961] DEBUG operator(683446):   N=init_drone_count = 2
[2026-01-11 12:04:47.908097438] DEBUG operator(683446):   P=max_drones_at_base = 30
[2026-01-11 12:04:47.908116561] DEBUG operator(683446):   Xi=max_charges = 0
[2026-01-11 12:04:47.908138025] DEBUG operator(683446):   T2=battery_lifetime = 1600 ms
[2026-01-11 12:04:47.908182511] DEBUG operator(683446):   T1=battery_chargetime = 800 ms
[2026-01-11 12:04:47.908199198] DEBUG operator(683446):   tunnel_length = 200 ms
[2026-01-11 12:04:47.908223512] DEBUG operator(683446):   tun_cap = 2
```

Dron w bazie z bat <20% poprawnie ignoruje polecenie, i dron w bazie z bat >20% poprawnie akceptuje polecenie:

```
[2026-01-11 12:04:48.011624224] INFO  drone(683454): Hello world
[2026-01-11 12:04:48.068360116] INFO  drone(683454): Bat:  10%
[2026-01-11 12:04:48.114863551] WARN  operator(683448): Sending suicide order
[2026-01-11 12:04:48.114913184] INFO  drone(683454): Suicide mission order ignored

[2026-01-11 12:04:48.011717069] INFO  drone(683455): Hello world
[2026-01-11 12:04:48.308249496] DEBUG drone(683455): Bat:  40%
[2026-01-11 12:04:48.314983010] WARN  operator(683448): Sending suicide order
[2026-01-11 12:04:48.315054447] INFO  drone(683455): Suicide mission order accepted
```

Dron w locie z bat <20% poprawnie ignoruje polecenie, i dron w locie z bat >20% poprawnie akceptuje polecenie:

```
[2026-01-11 12:04:48.988498922] INFO  drone(683454): Left the base
[2026-01-11 12:04:50.076231066] INFO  drone(683454): Returning to the base
[2026-01-11 12:04:50.060276091] INFO  drone(683454): Bat:  20%
[2026-01-11 12:04:50.115129833] WARN  operator(683448): Sending suicide order
[2026-01-11 12:04:50.115192422] INFO  drone(683454): Suicide mission order ignored

[2026-01-11 12:04:49.980495007] INFO  drone(683467): Left the base
[2026-01-11 12:04:50.732222782] DEBUG drone(683467): Bat:  40%
[2026-01-11 12:04:50.815281301] WARN  operator(683448): Sending suicide order
[2026-01-11 12:04:50.815360505] INFO  drone(683467): Suicide mission order accepted
```

## 4. Dron z rozładowaną beterią w wejściu do bazy nie blokuje wejscia

Symulacja startuje z dronami z niską baterią poza bazą. Maksymalna liczba dronów w wejściach: 1.

```
[2026-01-11 12:42:04.307899021] DEBUG operator(694031): GlobalParameters:
[2026-01-11 12:42:04.307937157] DEBUG operator(694031):   scenario = 4
[2026-01-11 12:42:04.307955125] DEBUG operator(694031):   N=init_drone_count = 10
[2026-01-11 12:42:04.307976326] DEBUG operator(694031):   P=max_drones_at_base = 100
[2026-01-11 12:42:04.308000866] DEBUG operator(694031):   Xi=max_charges = 0
[2026-01-11 12:42:04.308029191] DEBUG operator(694031):   T2=battery_lifetime = 1500 ms
[2026-01-11 12:42:04.308087686] DEBUG operator(694031):   T1=battery_chargetime = 400 ms
[2026-01-11 12:42:04.308107616] DEBUG operator(694031):   tunnel_length = 600 ms
[2026-01-11 12:42:04.308133451] DEBUG operator(694031):   tun_cap = 1
```

Dwa drony tracą energię w wejściu do bazy:

```
[2026-01-11 12:42:04.329100448] INFO  drone(694037): Hello world
[2026-01-11 12:42:04.329739527] INFO  drone(694037): Returning to the base
[2026-01-11 12:42:04.329789331] TRACE drone(694037): Added to queue, priority 83
[2026-01-11 12:42:04.329887608] TRACE drone(694037): Entering tun 2 dir: in
[2026-01-11 12:42:04.329902969] TRACE drone(694037): Left the queue
[2026-01-11 12:42:04.570876662] WARN  drone(694037): Battery died!

[2026-01-11 12:42:04.330422535] INFO  drone(694038): Hello world
[2026-01-11 12:42:04.330988681] INFO  drone(694038): Returning to the base
[2026-01-11 12:42:04.331098962] TRACE drone(694038): Added to queue, priority 85
[2026-01-11 12:42:04.331174611] TRACE drone(694038): Entering tun 1 dir: in
[2026-01-11 12:42:04.331190818] TRACE drone(694038): Left the queue
[2026-01-11 12:42:04.541433781] WARN  drone(694038): Battery died!
```

Kolejne drony dalej mogą wejść do tunelu:

```
[2026-01-11 12:42:04.337186145] INFO  drone(694043): Hello world
[2026-01-11 12:42:04.337618819] INFO  drone(694043): Returning to the base
[2026-01-11 12:42:04.337679196] TRACE drone(694043): Added to queue, priority 85
[2026-01-11 12:42:04.541782396] TRACE drone(694043): Entering tun 1 dir: in
[2026-01-11 12:42:04.541809383] TRACE drone(694043): Left the queue
[2026-01-11 12:42:04.547887277] WARN  drone(694043): Battery died!

[2026-01-11 12:42:04.517796755] INFO  drone(694079): Hello world
[2026-01-11 12:42:04.518156139] INFO  drone(694079): Returning to the base
[2026-01-11 12:42:04.518661600] TRACE drone(694079): Added to queue, priority 83
[2026-01-11 12:42:04.548418644] TRACE drone(694079): Entering tun 1 dir: in
[2026-01-11 12:42:04.548440284] TRACE drone(694079): Left the queue
[2026-01-11 12:42:04.773373930] WARN  drone(694079): Battery died!
```

## 5. Wejścia poprawnie zmieniaja kierunek

Maksymalna liczba dronów w wejściach: 1.
Symulacja startuje z dronami z pełną baterią w bazie i losowo z niską baterią poza bazą.
Czyli te drony od razu zechcą przejśc przez wejścia.

```
[2026-01-01 23:15:21.734168749] DEBUG operator(1061076): GlobalParameters:
[2026-01-01 23:15:21.734175942] DEBUG operator(1061076):   scenario = 5
[2026-01-01 23:15:21.734179295] DEBUG operator(1061076):   N=init_drone_count = 4
[2026-01-01 23:15:21.734182717] DEBUG operator(1061076):   P=max_drones_at_base = 100
[2026-01-01 23:15:21.734186069] DEBUG operator(1061076):   Xi=max_charges = 0
[2026-01-01 23:15:21.734190329] DEBUG operator(1061076):   T2=battery_lifetime = 1000 ms
[2026-01-01 23:15:21.734194031] DEBUG operator(1061076):   T1=battery_chargetime = 1000 ms
[2026-01-01 23:15:21.734197314] DEBUG operator(1061076):   tunnel_length = 200 ms
[2026-01-01 23:15:21.734200596] DEBUG operator(1061076):   tun_cap = 1
```

Pierwszy dron wychodzi z bazy, zajmuje wejście 1:

```
[2026-01-01 23:15:21.739134802] INFO  drone(1061081): Leaving the base
[2026-01-01 23:15:21.739143043] TRACE drone(1061081): Added to queue, priority 0
[2026-01-01 23:15:21.739158687] TRACE drone(1061081): Entering tun 1 dir: out
```

Drugi dron wchodzi do bazy, zajmuje wejście 2:

```
[2026-01-01 23:15:21.739780343] INFO  drone(1061082): Returning to the base
[2026-01-01 23:15:21.739789911] TRACE drone(1061082): Added to queue, priority 85
[2026-01-01 23:15:21.739805416] TRACE drone(1061082): Entering tun 2 dir: in
```

Dron 3 wychodzi wejśćiem 2, po tym jak drugi dron je opuszcza, wejście zmienia kierunek:

```
[2026-01-01 23:15:21.739826927] INFO  drone(1061083): Leaving the base
[2026-01-01 23:15:21.739834051] TRACE drone(1061083): Added to queue, priority 0
[2026-01-01 23:15:21.879880871] WARN  drone(1061082): Battery died! # tun 2 empty
[2026-01-01 23:15:21.890375967] TRACE drone(1061083): Entering tun 2 dir: out
```

Dron 4 wchodzi wejściem 1, po tym jak pierszy dron je opuszcza, wejście zmienia kierunek:

```
[2026-01-01 23:15:21.939265872] INFO  drone(1061081): Left the base # tun 1 empty
[2026-01-01 23:15:21.890974296] INFO  drone(1061096): Returning to the base
[2026-01-01 23:15:21.941126300] TRACE drone(1061096): Entering tun 1 dir: in
```

# Linki do funkcji

## Tworzenie i obsługa plików

open(), write(), close():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/logger.cpp#L137-L165>

## Tworzenie procesów

fork():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/process.cpp#L40-L50>

exec:
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/process.cpp#L113-L122>

## Tworzenie i obsługa wątków

pthread_create():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/thread.cpp#L5-L25>

pthread_join():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/5fa6a9a51088f672181d8d1557193422a23575ca/src/common/thread.cpp#L62-L73>

pthread_detach():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/5fa6a9a51088f672181d8d1557193422a23575ca/src/common/thread.cpp#L82-L89>

pthread_cancel():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/thread.cpp#L34-L39>

pthread_mutex_lock(), pthread_mutex_unlock():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/thread_utils.cpp#L5-L10>

pthread_cond_broadcast(), pthread_cond_wait():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/thread_utils.cpp#L12-L17>

## Łącza nazwane i nienazwane

fork() z pipe():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/process.cpp#L57-L78>

## Segmenty pamięci dzielonej

shmget():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/shared_memory.h#L104-L112>

shmdt():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/shared_memory.h#L80-L85>

shmat():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/shared_memory.h#L72-L78>

shmctl():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/shared_memory.h#L64-L70>

## Kolejki komunikatów

msgget():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/msg_queue.cpp#L47-L55>

msgctl():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/msg_queue.cpp#L57-L67>

msgsnd():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/msg_queue.h#L35-L62>

msgrcv():
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/ipc/msg_queue.h#L64-L92>
