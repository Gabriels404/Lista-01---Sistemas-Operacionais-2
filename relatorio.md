# Relatório de Implementação — Programação Concorrente em C (Windows)

**Disciplina:** Programação Concorrente  
**Linguagem:** C (Windows API – `CreateThread`, `CRITICAL_SECTION`, `CONDITION_VARIABLE`)  
**Autores:** Gabriel Soares Cintra

---

## 🐎 Exercício 1 — Corrida de Cavalos

Neste exercício, cada **cavalo** foi representado por uma **thread** independente.  
Cada thread avança sua posição em passos aleatórios até atingir a linha de chegada.  

Antes da corrida, o programa solicita ao usuário uma **aposta** sobre qual cavalo vencerá.  
As threads são iniciadas de forma **sincronizada**, garantindo que todas larguem ao mesmo tempo.

Para evitar **condições de corrida**, o registro do vencedor é protegido com uma **região crítica** (`CRITICAL_SECTION`).  
Assim, somente uma thread por vez pode atualizar a variável global `vencedor`.

Empates são resolvidos determinísticamente, escolhendo o cavalo com o menor índice (ID).  
No final, o programa exibe o vencedor e informa se a aposta do usuário estava correta.

---

## 🌀 Exercício 2 — Buffer Circular Produtor/Consumidor

Este exercício implementa um **buffer circular** compartilhado entre múltiplos **produtores** e **consumidores**.  
Os produtores geram itens com tempos aleatórios e inserem no buffer, enquanto os consumidores retiram os itens.

Para garantir **exclusão mútua**, foi utilizado um `CRITICAL_SECTION`.  
Além disso, foram usadas duas **variáveis de condição** (`CONDITION_VARIABLE`):
- `not_full`: usada pelos produtores para esperar caso o buffer esteja cheio;  
- `not_empty`: usada pelos consumidores para esperar caso o buffer esteja vazio.

Com isso, evitamos **espera ativa** (busy waiting).  
A execução demonstra o comportamento clássico do problema Produtor-Consumidor, com sincronização adequada e ausência de perdas de dados.

---

## 💸 Exercício 3 — Transferência entre Contas Bancárias

Neste programa, há várias **contas bancárias** compartilhando o mesmo espaço de memória.  
Múltiplas **threads** simulam transferências aleatórias entre contas diferentes.

Cada conta é protegida por um **mutex individual** (`CRITICAL_SECTION`).  
Assim, duas threads diferentes podem operar em contas distintas simultaneamente, mas não podem alterar a mesma conta ao mesmo tempo.

O programa executa diversas transferências e, ao final, verifica se a **soma total de dinheiro** permanece constante.  
Quando as travas são removidas, a soma se torna incorreta, evidenciando a presença de **condições de corrida**.

---

## 🧵 Exercício 4 — Linha de Processamento (Pipeline)

Este exercício simula uma **linha de produção** dividida em três etapas:
1. **Captura** (entrada de dados)
2. **Processamento** (transformação)
3. **Gravação** (saída)

Cada etapa é executada por uma thread separada, e a comunicação entre as etapas ocorre por meio de **duas filas limitadas** (buffers circulares).  
Essas filas são protegidas por **mutex e variáveis de condição**, garantindo sincronização correta entre os estágios.

Para finalizar o processo de forma limpa, é utilizado o conceito de **"poison pill"** — um valor especial (por exemplo, `-1`) que indica o encerramento.  
Com isso, as threads conseguem encerrar sem deadlock e sem perda de dados.

---

## ⚙️ Exercício 5 — Thread Pool (Fila de Tarefas)

O exercício implementa um **pool fixo de threads** (por exemplo, 4 threads) que executam tarefas da fila.  
As tarefas são **CPU-bound**, como o teste de primalidade ou o cálculo de Fibonacci.

O programa mantém uma **fila concorrente** de tarefas, protegida com `CRITICAL_SECTION` e variáveis de condição.  
O usuário insere números na entrada padrão (stdin), e cada número é processado por uma thread disponível.

O pool funciona até o final da entrada (EOF), quando uma sinalização (`acabou = 1`) é enviada para que todas as threads encerrem corretamente.  
Assim, garante-se que **nenhuma tarefa seja perdida** e que a fila seja **thread-safe**.

---

## 🧠 Exercício 6 — Leitura Paralela e Redução (Map-Reduce)

Neste exercício, um **arquivo grande de inteiros** é dividido em blocos, processados por várias threads em paralelo.  
Cada thread realiza o **mapeamento local** (somando seus valores e criando um histograma local).  

Depois, a **thread principal** combina os resultados (“reduce”) para obter:
- Soma total de todos os números;
- Histograma de frequências global.

A sincronização é feita com exclusão mútua mínima — apenas no momento de combinar os resultados.  
Por fim, o programa mede o **speedup** para diferentes números de threads (`P = 1, 2, 4, 8`), mostrando ganhos de desempenho.

---

## 🍽️ Exercício 7 — Filósofos com Garfos (Deadlock e Starvation)

O clássico **problema dos filósofos** foi implementado com garfos representados por **mutex** (`CRITICAL_SECTION`).  

Foram propostas **duas soluções**:
1. **Ordem global de aquisição:** cada filósofo pega primeiro o garfo de menor índice e depois o de maior. Isso elimina deadlocks.
2. **Semáforo com limite:** apenas quatro filósofos podem tentar comer simultaneamente, reduzindo o risco de bloqueio.

Além disso, foram coletadas métricas por filósofo (número de refeições e tempo de espera).  
O algoritmo foi ajustado para minimizar **starvation**, garantindo que todos eventualmente consigam comer.

---

## 📊 Exercício 8 — Buffer com Rajadas e Backpressure

Este exercício estende o problema Produtor-Consumidor (exercício 2) adicionando **rajadas de produção (bursts)**.  
Os produtores enviam muitos itens rapidamente e depois ficam ociosos.  

Quando o consumidor não consegue acompanhar a taxa de produção, aplica-se o conceito de **backpressure**:  
os produtores são forçados a esperar até que o consumo aumente e o buffer tenha espaço novamente.

Durante a simulação, o programa coleta dados sobre:
- Ocupação média do buffer;
- Períodos de estabilidade e sobrecarga.

Isso permite observar o impacto do **tamanho do buffer** na **estabilidade do sistema**.

---

## 🏃 Exercício 9 — Corrida de Revezamento com Barreira

Cada equipe da corrida de revezamento é composta por várias **threads** (corredores).  
Todas as threads de uma equipe precisam chegar à **barreira** (sincronização) para liberar o próximo corredor.

A implementação usa o tipo `pthread_barrier_t` ou uma **barreira manual** construída com `CRITICAL_SECTION` e `CONDITION_VARIABLE`.  
Dessa forma, cada rodada da corrida só prossegue quando todos os corredores anteriores terminam.

O programa mede quantas **rodadas por minuto** são completadas, permitindo avaliar o desempenho conforme o tamanho da equipe cresce.

---

## 🔒 Exercício 10 — Deadlock e Watchdog

Neste último exercício, o objetivo foi **induzir um deadlock** de forma controlada.  
As threads competem por múltiplos recursos (travas) em ordens aleatórias, o que pode causar bloqueio total.

Uma **thread especial "watchdog"** monitora o sistema:  
se não houver progresso por um tempo (`T` segundos), ela identifica as threads e recursos envolvidos e emite um **relatório de deadlock**.

Em seguida, o programa é reconfigurado para adotar uma **ordem global de travamento**, eliminando o problema.  
O comportamento com e sem correção é comparado, evidenciando o impacto das políticas de travamento consistentes.

---

## 🧩 Conclusões Gerais

- O uso de **mutex**, **semáforos** e **variáveis de condição** é essencial para evitar **condições de corrida** e **deadlocks**.  
- A sincronização correta permite paralelismo eficiente sem perda de dados.  
- O uso de técnicas como **ordem global de travas**, **barreiras**, **poison pill** e **thread pool** melhora a robustez dos sistemas concorrentes.  
- Todos os exercícios demonstram princípios reais de **sistemas operacionais** e **programação concorrente**, aplicáveis em software de alto desempenho.

---

**Fim do Relatório**
