# Github

[LostInTheLogs/drone-swarm-simulation](https://github.com/LostInTheLogs/drone-swarm-simulation)

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

- tworzy potrzebne ipc
- startuje `loggera` i `operatora`

## src/logger

- tworzy kolejke komunikatów
- czeka na komunikaty, wypisuje je na stdout i do pliku.

## src/operator

- uzupełnia braki dronów
- **sig1**: zwiększa maksymalną ilość dronów 2x
- **sig2**: zmniejsza maksymalną ilość dronów 2x

## src/drone

- startuje w bazie
- po ładowaniu bateri $T_1$ opuszcza bazę
- maksymalny czas lotu $T_2$
- powrót do bazy przy baterii < 20%
- zniszczenie przy baterii = 0%
- po $X_i$ ładowaniach utilizacja
- **sig3**: samobójstwo (nawet w trakcie ładowania), ignorowany jeśli bateria < 20%

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
[2026-01-01 19:40:17.015630845] DEBUG operator(1017166): GlobalParameters:
[2026-01-01 19:40:17.015640204] DEBUG operator(1017166):   scenario = 2
[2026-01-01 19:40:17.015644883] DEBUG operator(1017166):   N=init_drone_count = 10
[2026-01-01 19:40:17.015649143] DEBUG operator(1017166):   P=max_drones_at_base = 100
[2026-01-01 19:40:17.015653404] DEBUG operator(1017166):   Xi=max_charges = 0
[2026-01-01 19:40:17.015657734] DEBUG operator(1017166):   T2=battery_lifetime = 10000 ms
[2026-01-01 19:40:17.015662134] DEBUG operator(1017166):   T1=battery_chargetime = 10000 ms
[2026-01-01 19:40:17.015665975] DEBUG operator(1017166):   tunnel_length = 200 ms
[2026-01-01 19:40:17.015670235] DEBUG operator(1017166):   tun_cap = 1
```

Pierwsze dwa drony od razu wychodzą z kolejki, bo wejścia są puste:

```
[2026-01-01 19:40:17.019857648] TRACE drone(1017173): Added to queue, priority 85
[2026-01-01 19:40:17.019878391] TRACE drone(1017173): Left the queue
[2026-01-01 19:40:17.020225643] TRACE drone(1017175): Added to queue, priority 84
[2026-01-01 19:40:17.020246176] TRACE drone(1017175): Left the queue
```

Następne drony czekają ąż wejśćia będą puste:

```
[2026-01-01 19:40:17.020303516] TRACE drone(1017172): Added to queue, priority 84
[2026-01-01 19:40:17.020359320] TRACE drone(1017174): Added to queue, priority 87 (2)
[2026-01-01 19:40:17.020405485] TRACE drone(1017171): Added to queue, priority 89 (1)
[2026-01-01 19:40:17.020458285] TRACE drone(1017176): Added to queue, priority 87
[2026-01-01 19:40:17.020512621] TRACE drone(1017177): Added to queue, priority 82
[2026-01-01 19:40:17.020742609] TRACE drone(1017178): Added to queue, priority 83
[2026-01-01 19:40:17.020919238] TRACE drone(1017179): Added to queue, priority 85
[2026-01-01 19:40:17.021241207] TRACE drone(1017180): Added to queue, priority 85

```

Dwa drony z najniższą baterią (1,2) wychodzą z kolejki:

```
[2026-01-01 19:40:17.220825193] TRACE drone(1017171): Left the queue
[2026-01-01 19:40:17.270870563] TRACE drone(1017174): Left the queue
```

Dwa drony z najniższą baterią (3,4) wychodzą z kolejki:

```
[2026-01-01 19:40:17.271939487] TRACE drone(1017218): Added to queue, priority 89 (4)
[2026-01-01 19:40:17.271973150] TRACE drone(1017219): Added to queue, priority 90 (3)
[2026-01-01 19:40:17.422255997] TRACE drone(1017219): Left the queue
[2026-01-01 19:40:17.472333703] TRACE drone(1017218): Left the queue
```

Następne drony poprawnie wychodzą z kolejki:

```
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

Symulacja startuje z dwoma dronami z niską baterią w bazie:

```
[2026-01-02 16:00:06.003259578] DEBUG operator(44566): GlobalParameters:
[2026-01-02 16:00:06.003266430] DEBUG operator(44566):   scenario = 3
[2026-01-02 16:00:06.003269717] DEBUG operator(44566):   N=init_drone_count = 2
[2026-01-02 16:00:06.003272302] DEBUG operator(44566):   P=max_drones_at_base = 2
[2026-01-02 16:00:06.003275157] DEBUG operator(44566):   Xi=max_charges = 0
[2026-01-02 16:00:06.003278623] DEBUG operator(44566):   T2=battery_lifetime = 800 ms
[2026-01-02 16:00:06.003281238] DEBUG operator(44566):   T1=battery_chargetime = 800 ms
[2026-01-02 16:00:06.003283823] DEBUG operator(44566):   tunnel_length = 200 ms
[2026-01-02 16:00:06.003287049] DEBUG operator(44566):   tun_cap = 2
```

Dron w bazie z bat <20% poprawnie ignoruje polecenie, i dron w bazie z bat >20% poprawnie akceptuje polecenie:

```
[2026-01-02 16:00:06.085923083] DEBUG drone(44571): Bat:  10%
[2026-01-02 16:00:06.105763546] INFO  drone(44571): Suicide mission order ignored

[2026-01-02 16:00:06.285858298] DEBUG drone(44572): Bat:  60%
[2026-01-02 16:00:06.305873228] INFO  drone(44572): Suicide mission order accepted
```

Dron w locie z bat <20% poprawnie ignoruje polecenie, i dron w locie z bat >20% poprawnie akceptuje polecenie:

```
[2026-01-02 16:00:06.646063571] INFO  drone(44571): Left the base
[2026-01-02 16:00:07.081918238] DEBUG drone(44571): Bat:  20%
[2026-01-02 16:00:07.089927489] INFO  drone(44571): Returning to the base
[2026-01-02 16:00:07.105943575] INFO  drone(44571): Suicide mission order ignored

[2026-01-02 16:00:07.814358067] INFO  drone(45417): Left the base
[2026-01-02 16:00:07.814962612] DEBUG drone(45420): Bat:  50%
[2026-01-02 16:00:07.821908776] INFO  drone(45417): Suicide mission order accepted
```

## 4. Dron z rozładowaną beterią w wejściu do bazy nie blokuje wejscia

Symulacja startuje z dronami z niską baterią poza bazą. Maksymalna liczba dronów w wejściach: 1.

```
[2026-01-01 23:03:40.628199365] DEBUG operator(1058101): GlobalParameters:
[2026-01-01 23:03:40.628206978] DEBUG operator(1058101):   scenario = 4
[2026-01-01 23:03:40.628211867] DEBUG operator(1058101):   N=init_drone_count = 10
[2026-01-01 23:03:40.628216197] DEBUG operator(1058101):   P=max_drones_at_base = 100
[2026-01-01 23:03:40.628220458] DEBUG operator(1058101):   Xi=max_charges = 0
[2026-01-01 23:03:40.628227442] DEBUG operator(1058101):   T2=battery_lifetime = 2000 ms
[2026-01-01 23:03:40.628232191] DEBUG operator(1058101):   T1=battery_chargetime = 2000 ms
[2026-01-01 23:03:40.628236521] DEBUG operator(1058101):   tunnel_length = 200 ms
[2026-01-01 23:03:40.628240432] DEBUG operator(1058101):   tun_cap = 1
```

Dwa drony tracą energię w wejściu do bazy:

```
[2026-01-01 23:03:40.833988668] TRACE drone(1058115): Left the queue
[2026-01-01 23:03:40.913757219] WARN  drone(1058115): Battery died!

[2026-01-01 23:03:40.834418195] TRACE drone(1058119): Left the queue
[2026-01-01 23:03:40.914081983] WARN  drone(1058119): Battery died!
```

Kolejny dron poprawnie wraca do bazy, czyli wejścia nie są zablokowane:

```
[2026-01-01 23:03:40.935791518] TRACE drone(1058160): Left the queue
[2026-01-01 23:03:41.135868814] INFO  drone(1058160): Back on the charging pad
[2026-01-01 23:03:41.135875449] INFO  drone(1058160): Back at the base
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
<https://github.com/LostInTheLogs/drone-swarm-simulation/blob/37cba7ad23e2c57b3976c7c535beb70892751997/src/common/thread.cpp#L27-L32>

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
