## Table of contents
1. [Overlapped IO Performance](#Overlapped-IO-Performance)
2. [Multithread Sync vs Overlapped IO](#Multithread-Sync-vs-Overlapped-IO)
3. [Read Call Task Overhead](#Read-Call-Task-Overhead)
4. [Do Read Call Task as soon as possible](#Do-Read-Call-Task-as-soon-as-possible)
5. [Role Speficied Thread](#Role-Speficied-Thread)
6. [Memory Mapped File](#Memory-Mapped-File)
7. [Massive File Count](#Massive-File-Count)
8. [Summary](#Summary)

## Overlapped IO Performance

![](https://github.com/W298/MultithreadIOSimulator/assets/25034289/1fd5cd9b-ee4b-457d-9105-008b32dcf031)

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/a9ee8f7e-e575-4fab-9515-835175dcfed4)
<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/b2fb822a-8612-423e-ac61-92f57254cb43" width="500px" />  
*<512MB 40개, 20GiB (전체 Read 38461ms)>*

위는 40개의 IO 요청이 겹친 예시이다.

파일들이 겹쳐서 처리되기 때문에 파일 하나만 처리하는 것보다 더 큰 Latency 를 보여준다.  
위처럼 겹치는 파일이 점점 많아질수록 Latency가 일정 수치만큼 계속 증가하는 것을 확인할 수 있다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/c5614130-64ab-4589-afd8-bb17130d401b)
![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/aff60d15-391e-47d7-ba0a-6aa26a806b9e)  
*<512MB 40개, 20GiB (전체 Read 40364ms)>*

위는 40개의 IO 요청을 겹치지 않게 구성한 테스트이다.  
겹쳐서 처리되지 않기 때문에 512MB 파일 1개만 Read 하는 데에 걸린 시간이 `961.72ms` 로 짧아졌다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/aa0e7e9b-31ba-48b1-a606-09bfa375e66a)  
*<512MB 40개, 20GiB (전체 Read 40364ms)>*

Overlapped IO가 더 빠른 성능을 보여주나, 이는 위와 같이 Read Call Task가 `55ms` 정도 걸려 실제 디스크 작업이 끊겨 생기는 오차로, 엄밀하게 Read 속도만 고려한다면 `40364ms - 55ms * 40 = 38164ms` 로 비슷한 결과를 보인다.

Overlapped IO를 사용하여 여러 파일을 겹쳐 Read를 수행해도 결국 거의 Read 자체 성능은 동일한 성능을 가진다.  
하지만 Overlapped IO 를 사용하면, Read Call Task의 실행시간에 영향을 받지 않고서 작업이 가능한 이점이 있다.

하지만 읽은 파일을 Computing 하는 작업은 결국 읽기가 끝나야 가능하기 때문에  
결론적으로 이는 Overlapped IO의 이점을 살릴 수 없다.

Overlapped IO 를 사용하여 여러 파일을 겹쳐 읽었을 때의 걸린 시간을 이용하여 읽기 속도를 계산해 보면,  
`20 * 1024 MiB / 38.461s = 532 MiB/s` 로 현재 SSD 최대 속도와 근접한 모습을 보인다.

## Multithread Sync vs Overlapped IO
```
File Count: 50
File Size - Normal Dist (1MiB, 1KiB ~ 2MiB)
Compute time - Normal Dist (0.8ms, 0.05ms ~ 2.5ms)
```

**Sync**

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/d016e2ce-57da-42bf-95d7-345da2e3cc7d)
*<SYNC 4 Thread (28.33ms)>*

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/36b96a66-de3b-4905-90a5-1c57aa5ead6b" width="350px" />
<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/fd4c13df-6dcf-42f0-8017-36914bc6834e" width="350px" />

```
Execution 55%
Sync 6%
IO 38%
```

동기로 읽기를 진행했을 경우, Waiting 하는 데에 38%를 사용하고 있다.

**Overlapped IO**

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/7797fe24-e32e-4593-9025-7f214c75b005)
*<Overalpped IO 1 Read / 3 Compute (21.62ms)>*

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/555b608b-2a39-4ade-afc5-986e045cc8a9" width="350px" />
<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/02abddd4-4bb9-4f12-a544-52a9489bd2e5" width="350px" />

```
Execution 79%
Sync 18%
IO 1%
Preemption 2%
```

Overalpped IO를 사용하면 동기로 진행했을 때보다 Execution %가 높아진 것을 확인할 수 있다.  
뿐만 아니라 실행 시간도 단축되었다.

Overlapped IO와 동기식 읽기의 차이는
- Compute time 이 길수록,
- 파일 사이즈가 클수록

더욱 두드러지게 나타난다. 동기식의 경우 다음 ReadFile Call을 보내기까지의 걸리는 시간에 저 두 요인이 작용하기 때문이다.

실제로 평균 파일 사이즈를 10MiB 로 늘렸을 때,

```
945.63ms (Sync) vs 932.64ms (Overlapped IO)
Delta 12.99ms
```
로 더 차이가 벌어진 것을 확인할 수 있다.

또한 Compute time 평균을 4.88ms 로 늘렸을 때에도 측정해 보았는데,

```
131.46ms (Sync) vs 118.83ms (Overlapped IO)
Delta 12.63ms
```

로 차이가 벌어진 것을 확인할 수 있다. 하지만 Compute time 이 상당히 커질 경우 오히려 동기로 읽는 것이 빨라지는 시점이 있는데,

```
222.35ms (Sync) vs 234.46ms (Overlapped IO)
Delta -12.11ms
```

이러한 현상이 일어나는 이유는 ReadFile Task를 담당하는 쓰레드가 일을 마친 후 작업하고 있지 않아,  
Compute 를 할 수 있는 쓰레드 수가 하나 줄기 때문에 이런 일이 발생하는 것이다.

이는 총 쓰레드 개수가 달라지면 변화하는데,
```
189.62ms (5) vs 185.13ms (1/4)    -4.49ms
159.82ms (6) vs 152.16ms (1/5)    +7.65ms
141.92ms (7) vs 128.93ms (1/6)    +12.98ms
133.60ms (8) vs 118.21ms (1/7)    +15.39ms
```
쓰레드 개수가 동일함에도 불구하고, 총 쓰레드 개수가 늘어날수록 Overlapped IO가 더욱 빨라진다.

*결과적으로...*
- 파일 사이즈가 클수록 Overlapped IO가 유리하다.
- Compute time 이 길수록 Overlapped IO가 유리하나, 작은 총 쓰레드 수에서는 비슷하거나 오히려 나쁠 수 있다. 하지만 총 쓰레드 수가 늘어날수록 Overlapped IO가 유리해진다.

## Read Call Task Overhead

```
File distribution: IDENTICAL
File size: 4 KiB
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 3.9GiB (100만개)
```

### File Size

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/073be919-6494-48dc-8b21-586ca2895e54)

가로 축은 파일 크기 (512 * N Byte), 세로 축은 ReadFile 함수가 리턴되기까지 걸린 시간이다.

| Size   | CF  | RF   |
| ------ | --- | ---- |
| 512MiB | 0.1 | 56   |
| 128MiB | 0.1 | 15   |
| 32MiB  | 0.1 | 4.3  |
| 8MiB   | 0.1 | 0.97 |
| 1MiB   | 0.1 | 0.15 |
| 256KiB | 0.1 | 0.08 |
| 64KiB  | 0.1 | 0.06 |
| 16KiB  | 0.1 | 0.05 |
| 4KiB   | 0.1 | 0.05 |
| 1KiB   | 0.1 | 0.05 |
| 512B   | 0.1 | 0.04 |

파일 크기가 커질수록 ReadFile에 소요되는 시간이 Linear 하게 증가한다.  
CreateFile의 경우 파일 크기에 연관되어 있지 않다.

SATA 기반 SSD에서 테스트했을 때와, NVMe 기반 SSD에서 테스트했을 때의 ReadFile에 걸린 시간이 동일한 것으로 보아, 디스크 속도와는 관련이 없다고 볼 수 있다. 읽어들일 바이트 수를 줄일 경우, 그 바이트 수만큼만 시간이 걸린다.

### File Count

#### ReadFile Call

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/ade2ea9a-56ff-41d8-8c4a-8632672bdf62" width="600px" />

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/5348a920-07af-4480-997e-c15a2fbb2721)  
*<싱글 쓰레드 - 4KiB 100만개, 3.9GiB>*

처음의 ReadFile Call은 `0.1ms` 였으나, 시간이 지나면서 위와 같이 커널에 병목현상이 생겨 `0.1ms -> 1.2ms`로 증가하였다.  
이전 ReadFile Call을 커널이 처리하기 전에 너무 많이 Call이 되면서 Waiting이 발생하는 문제이다.

보라색 부분이 Waiting이 걸려 진행이 안되고 있는 것이다.

더욱 빠르게 ReadFile Call을 보내면 이러한 현상이 더 두드러지는데,

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/14ca858e-c7c7-4db7-9304-80f516636eaf)

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/209f3fdd-63a6-4774-88bf-40fa8bbcc594" height="550px" />

*<16쓰레드 - 4KiB 100만개, 3.9GiB>*

위와 같이 16쓰레드로 더욱 빠르게 ReadFile Call을 보내보았더니, ReadFile Call이 `0.1ms -> 28ms` 로 *매우* 증가한 것을 확인할 수 있다.

#### CreateFile Call

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/575e6012-ee49-4e94-a9e2-c2e77e188d2a" height="550px" />

*<16쓰레드 - 4KiB 100만개, 3.9GiB>*

위는 이전과 동일한 테스트인데, 초반에 ReadFile Call이 커지기 이전의 모습이다.  
싱글쓰레드로 ReadFile Call을 했을 시에는 `0.1ms`가 걸렸으나,  
멀티쓰레드로 Call 했을 시에는 위와 비슷한 이유로 `0.1ms -> 0.7ms`가 걸렸다.

즉, **ReadFile Call 보다는 아니지만**, CreateFile Call 또한 단일 시간 당 처리할 수 있는 양의 한계가 있다고 볼 수 있다.

## Do Read Call Task as soon as possible

디스크를 최대한으로 사용하기 위해서는 어떻게 해야 할까?

ReadFile Call 이 디스크가 읽는 속도보다 빠른 간격으로 들어와 디스크가 놀지 않게끔 빠르게 보내주어야 한다.  
항상 디스크 큐가 채워져 있고, 디스크가 지체없이 바로 다음 작업을 가져갈 수 있어야 한다.

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/cc287187-0480-4eed-a936-4e50d6e3d743" width="500px" />

SSD 의 경우 워낙 읽는 속도가 빠르고, 병렬로 처리가 가능하기 때문에  
ReadFile Call을 상당히 빠르게 보내주어야 Full-Load 를 기대할 수 있다.

최대한 빠르게 큐를 채워주기 위해서는, ReadFile Call을 빠르게 전부 먼저 보내고 그 다음에 Compute 작업을 해야 한다.  
ReadFile Call을 빠르게 보내기 위해서는 쓰레드 개수를 늘리면 되고, Compute 이전에 전부 Call을 보내버리면 된다.

결과는 파일의 사이즈에 따라서 나뉜다.

### Large File Size

```
File distribution: EXP
File size: 1024 ~ 4194304, Mean(524288)
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 4999.639139 MiB (1만개)
```

![Frame 1](https://github.com/W298/MultithreadIOSimulator/assets/25034289/877d7980-50a1-4bf2-b5d3-e85276f4d293)
![2](https://github.com/W298/MultithreadIOSimulator/assets/25034289/8f92b6ef-064b-4371-ada2-76dc7e96f101)  
*<10000개, 5GB / 16쓰레드 (9382.308006 ms)>*

위 그림을 보면 CPU 는 놀고, 디스크는 열심히 일하는 문제가 발생했음을 알 수 있다.

파일의 크기가 상당히 큰 경우,  
아무리 SSD가 빠르다고 해도 ReadFile Call 을 하는 것보다 파일을 읽는 데에 훨씬 많은 시간이 소비되기 때문에  
후반으로 가면 파일이 다 읽혀지기를 기다리기에 Computing 작업간의 Gap이 생기게 된다.

파일 크기가 크면 클수록 이 Gap이 더 커지게 된다.

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/c9f62184-d08c-430c-842c-ac2e26d8d4a2" height="400px" />

ReadFile Call은 빠르게 보내졌으나, 결국 디스크가 파일을 읽는 속도는 이전에 테스트해 본 것처럼 순차적으로 읽었을 때와 비슷하다.  

Computing 을 막 시작하는 때에는 초반에 쓰레드들이 보내두었던 Call이 쌓여있어 이를 처리하느라 바쁘게 일한다.  
하지만 이후에는 겹치는 IO로 인해서 Latency가 높아지고, 결국 파일이 읽혀야 처리가 가능하기에 Gap이 생기게 되어 쓰레드가 놀게 된다.

그러므로 무작정 ReadFile Call을 쓰레드를 늘려서 빠르게 보내 봤자, 큰 파일에서는 일정시간 이후 쓰레드가 놀아버리는 문제가 발생한다.

그러면 쓰레드가 놀지 않으면서 디스크가 Full-Load 되는 적정 쓰레드 개수가 존재하지 않을까?

![3](https://github.com/W298/MultithreadIOSimulator/assets/25034289/5cf62793-a868-4b18-9f12-6ea17a0be9df)  
*<10000개, 5GB / 4쓰레드 (9377.736092 ms)>*

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/58985dac-b3a9-4048-a709-6a8b5fb60f88)  
*<10000개, 5GB / 2쓰레드 (9379.302979 ms)>*

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/f7832d51-5afe-47c5-9832-ebfa460afd1c)  
*<10000개, 5GB / 싱글쓰레드 (9377.556801 ms)>*

쓰레드 개수를 줄여 보았다. 쓰레드 개수를 줄여도 거의 동일하거나 심지어 약간 더 빠른 결과가 나왔다.  
약간 빠른 이유는 context switch overhead 가 줄어들기 때문으로 볼 수 있을 것 같다.

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/4df08165-e689-4616-a6bf-f1d3259b0915" width="700px" />

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/de57947d-8128-4240-8a07-c29dcb7159eb" width="700px" />

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/ed9c99d7-9a7f-4401-a7df-8522dd86d570" width="700px" />

쓰레드 개수가 줄어들수록, 위처럼 실제로 ReadFile Call 이 끝나는 시간이 늦어진 것은 확실하다.  
4쓰레드 - `1820ms` / 2쓰레드 - `2222ms` / 싱글쓰레드 - `2970ms`

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/e9397ad9-7401-4254-80f1-117943bcbe66" width="800px" />

<img src="https://github.com/W298/MultithreadIOSimulator/assets/25034289/6bbe9c45-10bf-4fee-a1ac-64fdb77166c9" width="800px" />

하지만 위처럼 결국 마지막 파일이 처리되기 시작하는 시간이 같다. 마지막 파일의 읽기가 끝난 순간이 같다는 것이다.

아무리 ReadFile Call 이 빨라졌다고 해도, 디스크가 처리하는 속도보다 더 빠른 속도로 ReadFile Call 이 들어오게 되어 겹치는 양이 해소가 되지 않기에 계속 Latency가 늘어나게 되고, 결과적으로 마지막 파일의 경우 꽤 빠르게 Call 요청이 들어왔음에도 불구하고 매우 느리게 처리된다.

이는 디스크의 읽기 속도 한계 때문에 발생하는 문제로, 해결은 사실상 불가능하다.

하지만 큰 파일의 부분만으로도 Computing 이 가능할 경우, 큰 파일을 작게 나누어 처리하면 Computing 시작 시간을 단축시킬 수는 있다.

### Small File Size

```
File distribution: EXP
File size: 1024 ~ 1048576, Mean(4096), Variance(1048576)
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 33.332114 MiB (1만개)
```

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/1d083ff2-fa73-4f04-9b58-45585e5c0d78)
![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/e8ccfddf-f069-4578-af8c-db67b234339a)  
*<10000개, 33 MiB / 4쓰레드 (1844.8245ms)>*

파일 사이즈를 작게 잡았더니 CPU가 바쁘게 일하고 디스크가 노는 문제가 발생하였다.

이번에는 CPU가 ReadFile Call 하는 속도가 디스크가 읽는 속도를 따라잡지 못하여 디스크 큐가 계속 비는 문제가 발생하였다.  
위에서 볼 수 있듯이 디스크 Operation이 드문드문 빠져있다.

그래서 쓰레드의 개수를 늘려 ReadFile Call을 빠르게 보낼 필요가 있다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/f20fc613-bc57-42cb-a6c8-7e89f48150ec)  
*<10000개, 33 MiB / 16쓰레드 (607.7515ms)>*

16쓰레드로 테스트해 보았는데 비약적인 성능 향상이 있었다. 이에는 두가지 이유가 있는데,
1. 4쓰레드로 했을 때보다 ReadFile Call 완료 시간이 `200ms` 정도 빨라졌다.  
2. 16쓰레드로 작업을 처리하니 4쓰레드로 처리하는 것보다 Compute Time을 단축시킬 수 있었다.

그러면 더 많은 쓰레드를 이용하면 더 많은 성능 향상을 기대할 수 있지 않을까?

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/48dba106-40b5-4c19-b720-784678eb34ea)  
*<10000개, 33 MiB / 32쓰레드 (542.4941ms)>*

32쓰레드로 테스트해 보았는데, 이때는 비약적인 성능 향상이 있지는 않았다.

그 이유를 분석해 보았더니, 뒤의 Compute 부분은 확실히 쓰레드가 많아 빠르게 처리되었지만, ReadFile Call 이 완료되는 시간은 비슷하거나 오히려 더 느려졌다.

그 이유는 커널이 단위 시간 당 처리할 수 있는 Operation 에 한계가 있어, 아무리 빠르게 많이 CreateFile / ReadFile을 보내도  
결국 커널이 이전 작업을 처리하기까지 쓰레드가 Waiting을 하기 때문에 성능 향상이 이루어지지 못한 것이다.

간단히 말하면 커널에서 병목 현상이 생긴 것이다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/a5d92bdf-31f4-49bd-b978-90dad5ba12e8)  
*<10000개, 33 MiB / 32쓰레드>*

실제로 위를 보면 쓰레드에 보라색으로 칠해진 곳에서 Waiting 을 하고 있다. CreateFile 을 바로 처리하지 못하고 기다리고 있는 것이다.

결국 단위 시간당 2배 만큼 Read Call Task 를 보냈으나, 이에 소요되는 시간이 `1.2177ms` 로 이전보다 (`0.6461ms`) 2배가 되면서 성능 향상이 이루어지지 못한 것이다.

### Summary

결론적으로,

파일 사이즈가 충분히 큰 경우에는 쓰레드 개수가 큰 의미를 부여하지 못하기 때문에, 오히려 쓰레드 개수를 줄여 자원을 절약하는 것이 이득이다.  

파일 사이즈가 충분히 작은 경우에는 쓰레드 개수를 늘려 디스크가 놀지 않고 작업하게 하여 시간을 단축시킬 수 있으나, 커널에 병목 현상이 생길 수 있으므로 이는 한계가 존재한다.

따라서, 디스크가 파일을 Read 하는 데 걸리는 시간에 따라서 쓰레드 개수, ReadFile Call 타이밍을 정하는 것이 Optimal 하다.

## Role Speficied Thread

```
File distribution: EXP
File size: 1024 ~ 1048576, Mean(4096), Variance(1048576)
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 33.332114 MiB (1만개)
```

결국 Read Call Task를 빠르게 처리하는 데에는 한계에 부딪혔다. 그러면 결국 남은 것은 Compute Time 이다.  
파일 사이즈가 큰 경우 Compute Time 이 큰 의미를 갖지는 못했으나, 파일 사이즈가 현재 상당히 작고 많기 때문에 Compute Time 이 중요하게 작용한다.

그러면 Read Call Time을 최소화하면서도 Waiting 시간이 거의 없는 쓰레드 개수만큼을 Read Call Task Role로 할당하고,  
나머지 쓰레드는 전부 Compute Task Role로 설정하면 더 좋은 결과를 얻을 수 있지 않을까?

```
            Read Call Ends  Waiting   Elapsed time
32 Thread - 434ms           (49%)     527ms
24 Thread - 420ms           (36%)     524ms
16 Thread - 438ms           (22%)     602ms
15 Thread - 443ms           (19%)     622ms
14 Thread - 451ms           (16%)     675ms
12 Thread - 461ms           (12%)     736ms
08 Thread - 525ms           (6%)      1018ms
```

12 ~ 14쓰레드일 때가 Waiting 비율이 높지 않으면서 적절한 실행시간을 가진다.

```
(RnC) 12 Thread (C) 20 Thread - 430ms    (10%)    471ms
(Rnc) 12 Thread (C) 16 Thread - 412ms    (10%)    450ms
(Rnc) 12 Thread (C) 12 Thread - 414ms    (10%)    439ms
(Rnc) 08 Thread (C) 12 Thread - 455ms    (6%)     472ms

RnC = 기존 방식 (Read and Compute)  
C = Compute 만 하는 쓰레드
```

Read Call Task가 끝나는 시간이 느려졌지만, 그만큼 Read Call이 전체적으로 잘 분포되었다는 것으로 병목 현상이 어느정도 해결되었음을 알 수 있다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/fba70d65-7b2e-4d43-9f7e-912acfc0e1d5)
![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/768e09ab-3a4f-4ce3-9fcf-a173e7f75e57)
![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/01871645-9941-4998-bbc4-b9393ee286a2)  
*<24쓰레드 (RnC)>*

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/67c71933-792c-4e86-96bb-7cc02e1d064a)
![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/dfa4953d-0693-4b4e-af3f-85ccc491013f)
![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/512eca41-b327-4b9b-b636-ad3178da4a69)  
*<12 (Rnc) / 12(C) 쓰레드>*

여러번 테스트를 진행해 보았는데, 12/12 가 가장 좋은 결과를 보였다.  
총 시간동안 ReadFile Call이 적절히 분포되어 있어, 한번에 모든 Call을 보내는 것보다 Waiting 시간이 적어 유리하다.  
CPU 사용률 또한 그냥 24쓰레드로 진행했을 때보다 더 좋은 결과를 보여주었다.

이는 파일 구조, 사이즈, Compute Time 에 따라서 다른 결과를 보여줄 것이다.

## Memory Mapped File

Memory Mapped File은 Read 시간이 따로 존재하는 것이 아닌 해당 페이지에 접근할 시에 Page Fault가 발생하여 로드된다.  
그러므로 먼저 기존 ReadFile Call과의 비교를 위해서는, Compute Task를 수정해야 한다.

기존에는 Pre-define 된 시간동안 Compute를 진행하는 것으로 했으나,  
MMAP은 순수히 Compute 하는 시간을 분리할 수 없기 때문에 파일 크기에 비례하는 작업을 수행하는 것으로 변경하였다.

작업은 Check Sum을 50번 구하는 코드로 구성하였다. 작업이 변경되었기 때문에 Optimal Role 비율도 달라졌을 것이다.

### Small File Size

```
File distribution: EXP
File size: 1024 ~ 1048576, Mean(4096), Variance(1048576)
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 33.332114 MiB (1만개)
```

```
[READFILE]
12/12 - 387ms
9/15 - 355ms
8/16 - 365ms
```

현재 상황에서는 9/15 쓰레드가 최적의 값을 보인다.

```
[MMAP]
16쓰레드 - 404ms
24쓰레드 - 326ms
32쓰레드 - 337ms
```

MMAP 으로 테스트해 보았는데, 24쓰레드에서 가장 좋은 값을 보였다.  
동일 쓰레드 수로 비교해 보아도, MMAP이 일반적인 Read 방식보다 빠르다.

그 이유를 알아보기 위해 4KiB 1만개를 가지고 테스트해 보았다.

**1. ReadFile Call을 할 필요가 없다.**

기존 방식 중 Optimal 한 비율로 작업했을 때에도 크면 `1.6ms` 까지 나오기도 했다. 이 수치를 절약할 수 있다.  
그러므로 파일의 개수가 매우 많아질 경우 *매우* 유리해질 가능성이 높다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/661ae125-6551-43fd-a8fb-366b8956dfd8)  
*<READFILE - 4KiB 1만개>*

기존 방법으로 Task를 수행하는 시간만 계산해 보면 `0.635ms + 0.201ms = 0.836ms` 정도이다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/8a67a32e-2841-4e5e-a2fd-186d617bfb76)  
*<MMAP - 4KiB 1만개>*

MMAP을 사용하면 전체 Task를 수행하는 시간이 `0.725ms` 정도로 비슷하거나 조금 더 빠르다. 작업 간 전환이 없기 때문에 더욱 유리하다.

**2. Page fault 처리가 꽤 빠르다.**

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/6088df3f-51c6-4ca9-bb39-04fb0b422133)  
*<READFILE - 4KiB 1만개>*

Read Latency는 4KiB 기준으로 `0.1ms` 전후로 나온다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/51e6f088-760d-411e-aa83-c0e6dd2d25ee)  
*<MMAP - 4KiB 1만개>*

Page Fault는 `0.02ms ~ 0.09ms` 정도로 대개 Read latency 보다 빠르다.

**3. 매커니즘이 간단하다.**

한 번의 Task로 처리가 가능하기 때문에, 따로 Task 간의 Sync를 맞출 필요가 없다. 결과적으로 락을 덜 사용할 수 있다.  

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/3b7db0de-6a8c-4c8f-a195-c005b237e0ff)  
*<READFILE - 4KiB 1만개>*

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/b9b800d2-6952-4a0f-ae21-fed191537383)  
*<MMAP - 4KiB 1만개>*

### Large File Size

```
File distribution: EXP
File size: 1024 ~ 4194304, Mean(524288)
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 4999.639139 MiB (1만개)
```

큰 파일의 경우 기존 방법으로 진행 시, CPU가 노는 문제가 발생했었다. 하지만 Compute 작업이 *밀린 것*은 아니다.

MMAP으로 이를 테스트해 보았는데, 파일 사이즈가 커지니 page fault를 처리하느라 다음 작업을 처리하는 것이 늦어지는 현상이 발생하였다.

Overlapped IO와는 달리 MMAP은 처리 도중 page fault가 발생하기 때문에 동기적으로 작동하는 것과 같다.  
그래서 오히려 MMAP으로 처리하는 것이 느린 결과를 보였다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/3c2d4253-433d-4201-9a7e-a2f67729f5fe)  
*<24쓰레드 - READFILE (9383.28ms)>*

```
[READFILE]
16쓰레드 - 9380.02ms
8쓰레드 - 9377.42ms
4쓰레드 - 9378.26ms 
```

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/10d7dcdf-6f3a-4ece-99bc-dbbcac321015)  
*<24쓰레드 - MMAP (9455.10ms)>*

```
[MMAP]
32쓰레드 - 9464.34ms 
16쓰레드 - 9460.73ms 
8쓰레드 - 9512.90ms 
```

### Page fault and File Size

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/69be71f6-5d54-44e0-acd5-28c774c5f854)  
*<싱글쓰레드 - 512MiB>*

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/56df4dde-570e-46c6-a614-7794c805690c)  
*<싱글쓰레드 - 32MiB>*

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/0225ddea-7d03-4924-a8df-31d5825d2e6c)  
*<싱글쓰레드 - 256KiB, 64KiB, 16KiB, 4KiB>*

Page의 크기 (16KiB) 만큼 잘려서 page fault가 발생하게 된다.  
중간 중간 잘리는 것이 생각보다 커 보이지 않을 수 있으나, %로 계산을 해보면...

| Size   | Per |
|--------|-----|
| 64KiB  | 14% |
| 256KiB | 16% |
| 1MiB   | 14% |
| 8MiB   | 26% |
| 32MiB  | 29% |

상당한 시간을 차지하고 있음을 알 수 있다.

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/f6e183f6-0760-48f6-b6ba-db74fa6fb166)

또한 Gap이 작업이 진행되면서 점점 커지기 때문에 512MiB의 경우 Gap 하나가 최대 `0.7ms`가 된다.  
즉, 싱글쓰레드로 작업할 경우 page size 와 같은 크기의 파일을 Memory Mapped File로 가져올 때가 Optimal하다.

멀티쓰레드로 작업할 경우 이러한 고민을 좀 덜 수 있는데,

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/e9dc80a2-ba51-4187-abe8-ef78b404e29c)
*<2쓰레드 - 512MiB>*

위와 같이 Gap을 매울 수 있기 때문에, MMAP을 사용한 처리는 멀티쓰레드가 유리하다.

## Massive File Count

```
File distribution: IDENTICAL
File size: 4 KiB
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 3.9GiB (100만개)
```

### ReadFile vs ReadFile

```
READFILE (16쓰레드) - 1602226ms (1602초)
READFILE (8/8쓰레드) - 77443ms (77초)
READFILE (12/12쓰레드) - 39592ms (39초)
```

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/c463e139-cd93-4a18-9644-09096af18570)
*<READFILE 16쓰레드 - 1602226ms>*

READFILE 16쓰레드일 때는 총 `1602초`가 걸렸다.  
이전에 테스트한 것과 같이, ReadFile Call이 커널 병목으로 인해 매우 느려지면서 매우 느리게 실행된 결과이다.

특히나 파일 개수가 많아지면서 차이가 굉장히 벌어지는 것을 확인할 수 있다.

### ReadFile vs Memory Mapped File

```
READFILE (8/8쓰레드) - 77443ms (77초)
READFILE (12/12쓰레드) - 39592ms (39초)
MMAP (8쓰레드) - 49619ms (49초)
MMAP (16쓰레드) - 36059ms (36초)
MMAP (24쓰레드) - 31281ms (31초)
MMAP (32쓰레드) - 29456ms (29초)
MMAP (48쓰레드) - 27975ms (27초)
MMAP (64쓰레드) - 29079ms (29초)
```

![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/899029c7-b596-4e65-abfb-eff569eed6e8)
![image](https://github.com/W298/MultithreadIOSimulator/assets/25034289/145912f6-0cb6-4af2-b8e9-cd97468c2e1b)  
*<MMAP 16쓰레드 - 36059ms>*

Memory Mapped File로 테스트한 결과는 위와 같이 균일한 Task time들을 가졌다.  
ReadFile의 Optimal 조합인 12/12쓰레드로 테스트한 결과보다 더 좋은 결과를 보였다.

쓰레드 개수를 더 늘려 보았더니, 시간을 더 단축할 수 있었다.

이러한 결과가 나온 이유는,

1. 파일의 사이즈가 page size보다 작거나 같기 때문에 page fault 간의 Gap이 없고,
2. 파일의 개수가 매우 많아도 ReadFile Call을 보낼 필요가 없으므로 병목도 생기지 않아 MMAP이 유리한 결과를 보여주는 것으로 해석할 수 있다.

### General File Set

```
File distribution: EXP
File size: 1024 ~ 1048576, Mean(4096), Variance(1048576)
File compute time: 0.048828 ms ~ 4.882812 ms, Mean(0.390625 ms), Variance(0.390625 ms)
Total file size: 3.44GiB (100만개)
```

```
[READFILE]
4/4 쓰레드 - 85946ms
8/8 쓰레드 - 51360ms
12/12 쓰레드 - 39747ms
16/16 쓰레드 - 35883ms
```

```
[MMAP]
8쓰레드 - 54847ms
16쓰레드 - 33253ms
24쓰레드 - 27761ms
32쓰레드 - 28042ms
```

## Summary

![](https://github.com/W298/MultithreadIOSimulator/assets/25034289/259ed900-a018-4707-96b5-ff2a6879622f)

![](https://github.com/W298/MultithreadIOSimulator/assets/25034289/19a65b02-f062-4917-b5e8-fa2b4d945f38)

### File Size

- 파일 사이즈가 page size 보다 작은 경우, Memory Mapped File을 사용하는 것이 최적이다.
- 디스크 큐가 비어있지 않을 만큼 CreateFile / ReadFile Call을 빠르게 보내주어야 한다.

    - 파일이 너무 빨리 처리되어 디스크 큐가 자꾸 빈다면, Read Call 쓰레드의 개수를 늘려야 한다.
    - Compute 쓰레드는 Compute Time이 길수록, 파일 개수가 많을수록 이에 비례하여 할당하면 된다.

- 파일 사이즈가 커서 디스크는 일하지만 CPU가 노는 경우, 쓰레드의 개수를 줄이거나 작업을 끊어 처리함으로써 자원을 절약하는 방법을 사용해야 한다.

    - 이 경우 실행시간을 단축하는 것은 불가능하다.

### File Count

- 파일 사이즈가 page size 보다 작고, 개수가 매우 많은 경우 Memory Mapped File을 사용하고, 가능한 한 많은 쓰레드를 할당해 주는 것이 유리하다.
- ReadFile Call을 한번에 많이 몰아서 보내는 것보다, 전체 실행시간에 분포되도록 해야 한다.
